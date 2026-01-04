/*
Copyright (C) 2023- YAYC team <yaycteam@gmail.com>

This work is licensed under the terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/ or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

In addition to the above,
- The use of this work for training artificial intelligence is prohibited for both commercial and non-commercial use.
- Any and all donation options in derivative work must be the same as in the original work.
- All use of this work outside of the above terms must be explicitly agreed upon in advance with the exclusive copyright owner(s).
- Any derivative work must retain the above copyright and acknowledge that any and all use of the derivative work outside the above terms
  must be explicitly agreed upon in advance with the exclusive copyright owner(s) of the original work.

*/

#include "YaycUtilities.h"
#include "Platform.h"
#include "ThumbnailFetcher.h"
#include "RequestInterceptor.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTimer>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QProcess>
#include <QLoggingCategory>
#include <QDateTime>
#include <QVersionNumber>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QStringView>
#include <QLatin1String>
#include <QQmlEngine>
#include <QQmlContext>
#include <QtWebEngineQuick/qquickwebengineprofile.h>

// Static member definitions
bool YaycUtilities::isPlasma = false;
QDateTime YaycUtilities::appstartTS;

YaycUtilities::YaycUtilities(QObject *parent)
    : QObject(parent)
    , tcpSocket(new QTcpSocket(this))
{
    connect(tcpSocket, &QAbstractSocket::connected, this, &YaycUtilities::onSocketConnected);
    connect(tcpSocket, &QAbstractSocket::errorOccurred, this, &YaycUtilities::onSocketError);
}

YaycUtilities::~YaycUtilities()
{
}

QUrl YaycUtilities::urlWithPosition(const QString &url, const int position) const
{
    auto vendor = Platform::urlToVendor(url);
    if (vendor == Platform::UNK) {
        return {};
    }
    if (vendor == Platform::YTB) {
        if (url.indexOf(QLatin1String("&t=")) >= 0 || position == 0 || isYoutubeShortsUrl(url))
            return url;
        auto id = getVideoID(url);
        if (id.isEmpty())
            return {};

        return url + QLatin1String("&t=") + QString::number(int(position)) + QLatin1Char('s');
    }
    return {};
}

void YaycUtilities::yDebug(const QString &s)
{
    auto modelReadyTS = QDateTime::currentDateTimeUtc();
    auto msecs = YaycUtilities::appstartTS.msecsTo(modelReadyTS);
    QLoggingCategory category("qmldebug");
    qCInfo(category) << QString::number(msecs / 1000.0, 'f', 1) << ": " << s;
}

void YaycUtilities::addRequestInterceptor(QObject *webEngineView)
{
    QQmlEngine *engine = qmlEngine(webEngineView);
    if (!engine)
        qFatal("NULL engine for view");
    QQuickWebEngineProfile *profile =
        qvariant_cast<QQuickWebEngineProfile *>(webEngineView->property("profile"));
    if (!profile)
        qFatal("NULL profile for view");

    RequestInterceptor *interceptor = new RequestInterceptor(engine);
    profile->setUrlRequestInterceptor(interceptor);
    engine->rootContext()->setContextProperty("requestInterceptor", interceptor);
}

bool YaycUtilities::isYoutubeVideoUrl(QUrl url)
{
    url = removeWww(url);
    const QString surl = url.toString();
    return (isYoutubeStandardUrl(surl) || isYoutubeShortsUrl(surl));
}

bool YaycUtilities::isYoutubeStandardUrl(QUrl url)
{
    url = removeWww(url);
    const QString surl = url.toString();
    return isYoutubeStandardUrl(surl);
}

bool YaycUtilities::isYoutubeStandardUrl(const QString &url)
{
    return url.startsWith(standardVideoPattern);
}

bool YaycUtilities::isYoutubeChannelPage(QUrl url)
{
    url = removeWww(url);
    const QString surl = url.toString();
    return isYoutubeChannelPage(surl);
}

bool YaycUtilities::isYoutubeChannelPage(const QString &url)
{
    return url.startsWith(youtubeHomePattern);
}

bool YaycUtilities::isYoutubeHomepage(QUrl url)
{
    url = removeWww(url);
    const QString surl = url.toString();
    return isYoutubeHomepage(surl);
}

bool YaycUtilities::isYoutubeHomepage(const QString &url)
{
    return (url.size() <= (youtubeHomePattern.size() + 1)) && url.startsWith(youtubeHomePattern);
}

bool YaycUtilities::isYoutubeShortsUrl(QUrl url)
{
    url = removeWww(url);
    const QString surl = url.toString();
    return isYoutubeShortsUrl(surl);
}

bool YaycUtilities::isYoutubeShortsUrl(const QString &url)
{
    return url.startsWith(shortsVideoPattern);
}

QString YaycUtilities::getVideoID(QUrl url) const
{
    url = removeWww(url);
    const QString surl = url.toString();
    Platform::Vendor vendor = Platform::urlToVendor(url);
    if (vendor == Platform::UNK) {
        Q_UNREACHABLE();
        qWarning("Unknown Video platform");
        return {};
    }
    if (vendor == Platform::YTB) {
        if (isYoutubeStandardUrl(surl)) {
            const QStringView stripped = QStringView{surl}.mid(standardVideoPattern.size());
            const int idx = stripped.indexOf('&');
            return "YTBv_" + stripped.mid(0, idx).toString();
        } else if (isYoutubeShortsUrl(surl)) {
            const QStringView stripped = QStringView{surl}.mid(shortsVideoPattern.size());
            const int idx = stripped.indexOf('?');
            return "YTBs_" + stripped.mid(0, idx).toString();
        } else {
            return {};
        }
    }
    Q_UNREACHABLE();
    return {};
}

QString YaycUtilities::getVideoID(const QString &key, const QString &sVendor, bool isShorts) const
{
    const Platform::Vendor vendor = Platform::toVendor(sVendor);
    if (vendor == Platform::YTB) {
        if (isShorts) {
            return QLatin1String("YTBs_") + key;
        } else {
            return QLatin1String("YTBv_") + key;
        }
    }
    qWarning() << "getVideoID error: " << key << " " << sVendor << " " << isShorts;
    return {};
}

QString YaycUtilities::getChangelog()
{
    static QString changelog;
    if (changelog.isEmpty()) {
        QFile f(":/CHANGELOG.md");
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            changelog = f.readAll();
            f.close();
        } else {
            changelog = QLatin1String("Error retrieving CHANGELOG");
        }
    }
    return changelog;
}

QString YaycUtilities::getDisclaimer()
{
    static QString disclaimer;
    if (disclaimer.isEmpty()) {
        QFile f(":/DISCLAIMER.md");
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            disclaimer = f.readAll();
            f.close();
        }
    }
    return disclaimer;
}

void YaycUtilities::checkConnectivity()
{
    tcpSocket->connectToHost("github.com", 80);
}

void YaycUtilities::getLatestVersion()
{
    QNetworkRequest request(latestReleaseVersionURL);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    QNetworkReply *reply = m_nam.get(std::move(request));
    connect(reply, &QNetworkReply::finished, this, &YaycUtilities::onReplyFinished);
}

void YaycUtilities::getDonateEtag()
{
    QNetworkRequest request(donateURL);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    QNetworkReply *reply = m_nam.head(std::move(request));
    connect(reply, &QNetworkReply::finished, this, &YaycUtilities::onDonateEtagReplyFinished);
}

void YaycUtilities::getDonateURL()
{
    QNetworkRequest request(donateURL);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    QNetworkReply *reply = m_nam.get(std::move(request));
    connect(reply, &QNetworkReply::finished, this, &YaycUtilities::onDonateReplyFinished);
}

void YaycUtilities::printSettingsPath()
{
    QLoggingCategory category("qmldebug");
    QSettings settings;
    qCInfo(category) << "Settings path: " << settings.fileName();
}

void YaycUtilities::restartApp(int code)
{
    QTimer::singleShot(0, this, [code]() {
        auto app = QCoreApplication::instance();
        if (!app)
            qFatal("QCoreApplication::instance returned NULL");
        app->exit(code);
    });
}

void YaycUtilities::restartApp()
{
    restartApp(YaycUtilities::EXIT_CODE_REBOOT);
}

void YaycUtilities::clearSettings()
{
    restartApp(YaycUtilities::EXIT_CODE_ERASE_SETTINGS);
}

bool YaycUtilities::executableExists(const QString &exe) const
{
    return isExec(exe);
}

void YaycUtilities::fetchMissingThumbnails()
{
    ThumbnailFetcher::fetchMissing();
}

int YaycUtilities::compareSemver(const QString &version1, const QString &version2)
{
    QVersionNumber v1 = QVersionNumber::fromString(version1);
    QVersionNumber v2 = QVersionNumber::fromString(version2);

    if (v1 < v2)
        return -1;
    if (v1 > v2)
        return 1;
    return 0;
}

bool YaycUtilities::isShortVideo(const QString &fkey)
{
    if (!fkey.size())
        return false;
    return videoType(fkey) == QLatin1String("s_");
}

void YaycUtilities::openInBrowser(const QString &key, const QString &extWorkingDirRoot)
{
    if (!key.size())
        return;

    QDir d(extWorkingDirRoot);

    const bool exists = d.exists() && d.exists(key);
    if (exists) {
        if (YaycUtilities::isPlasma) {
            QProcess::startDetached(QLatin1String("/usr/bin/dolphin"),
                                    QStringList()
                                        << QUrl::fromLocalFile(d.filePath(key)).toString(),
                                    extWorkingDirRoot);
        } else {
            QDesktopServices::openUrl(QUrl::fromLocalFile(d.filePath(key)));
        }
    }
}

void YaycUtilities::onSocketConnected()
{
    tcpSocket->close();
    emit networkFound();
}

void YaycUtilities::onSocketError()
{
    tcpSocket->abort();
}

void YaycUtilities::onReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply) {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit latestVersion(QString(reply->readAll()).trimmed());
    }
}

void YaycUtilities::onDonateEtagReplyFinished()
{
    static const QByteArray headerName{"ETag"};
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply) {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError && reply->hasRawHeader(headerName)) {
            emit donateETag(reply->rawHeader(headerName));
        }
    }
}

void YaycUtilities::onDonateReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply) {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            emit donateUrl(QString(reply->readAll()).trimmed());
        }
    }
}
