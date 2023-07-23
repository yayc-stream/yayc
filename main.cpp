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

#include <QGuiApplication>
#include <QApplication>
#include <QSettings>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>
#include <QtWebEngine/qtwebengineglobal.h>
#include <QtWebEngine/qquickwebengineprofile.h>
#include <QtWebEngineCore/qwebengineurlrequestinterceptor.h>
#include <QTimer>
#include <QHash>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QString>
#include <QLatin1String>
#include <QFileSystemWatcher>
#include <QFileSystemModel>
#include <QFileIconProvider>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonValueRef>
#include <QDateTime>
#include <QQuickStyle>
#include <QDesktopServices>
#include <QDebug>
#include <QRunnable>
#include <QThreadPool>
#include <QProcess>
#include <QSortFilterProxyModel>
#include <QQuickImageProvider>
#include <QByteArray>
#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QTcpSocket>
#include <QMutex>
#include <QRecursiveMutex>
#include <QMutexLocker>
#include <QAtomicInteger>
#include <QMetaEnum>
#include <QMetaObject>
#include <QtGlobal>

#include "third_party/ad-block/ad_block_client.h"

namespace  {
bool isPlasma{false};
constexpr const int EXIT_CODE_REBOOT = -123456789;
constexpr const int EXIT_CODE_ERASE_SETTINGS = -123456788;
const QString videoExtension{"yayc"};
const QString channelExtension{"yaycc"};
const QString shortsVideoPattern{"https://youtube.com/shorts/"}; // ToDo: use www and rework removeWww()
const QString standardVideoPattern{"https://youtube.com/watch?v="};
const QString repositoryURL{"https://github.com/yayc-stream/yayc"};
const QString latestReleaseVersionURL{"https://raw.githubusercontent.com/yayc-stream/yayc/master/APPVERSION"};
const QString donateURL{"https://raw.githubusercontent.com/yayc-stream/yayc/master/DONATE"};
const QImage emptyImage(1,1, QImage::Format_RGB32);
QByteArray appVersion() {
    QByteArray sversion(QT_STRINGIFY(APPVERSION));
    return sversion;
}
QString videoType(const QString &key) {
    return key.mid(3,2);
}
bool isShorts(const QString &key) {
    return videoType(key).startsWith('s');
}
QString videoVendor(const QString &key) {
    return key.mid(0,3);
}
QString videoID(const QString &key) {
    return key.mid(5);
}
QString channelID(const QString &key) {
    return key.mid(4);
}
QUrl removeWww(QUrl url) {
    QString host = url.host();
    if (host.startsWith(QLatin1String("www."))) {
        host = host.mid(4); // remove first 4 chars
        url.setHost(host);
    }
    return url;
}
bool isExec(const QString &fileName) {
    QFileInfo check_file(fileName);
    if (check_file.exists() && check_file.isFile() && check_file.isExecutable())
        return true; // Found!
    return false;
}
QString avatarUrl(QString originalAvatarUrl) {
    return originalAvatarUrl.replace(QLatin1String("=s48-"), QLatin1String("=s128-"));
}
const QRegExp allowedDirsPattern("^[^\\.].*"); // Skip entries starting with . , this skips .channels and every other hidden dir
}

class Platform : public QObject {
    Q_OBJECT
public:
    enum Vendor
    {
        UNK = 0x0,
        YTB         // YouTube
    };
    Q_ENUM(Vendor)

    static QString toString(const Vendor &v) {
        QMetaEnum metaEnum = QMetaEnum::fromType<Platform::Vendor>();
        return metaEnum.valueToKey(v);
    }

    static Vendor toVendor(const QString &name) {
        static QMap<QString, Vendor> reverseLUT;
        if (reverseLUT.empty()) {
            // init
            for (unsigned int e = UNK; e <= YTB; ++e) {
                QMetaEnum metaEnum = QMetaEnum::fromType<Platform::Vendor>();
                reverseLUT[metaEnum.valueToKey(e)] = Vendor(e);
            }
        }
        auto res = reverseLUT.find(name);
        if (res != reverseLUT.end())
            return res.value();
        return UNK;
    }

    static Vendor urlToVendor(const QUrl &url) {
        static QMap<QString, Vendor> LUT;
        if (LUT.isEmpty()) {
            LUT["youtube.com"] = Platform::YTB;
        }
        QString host = url.host();
        if (host.startsWith("www."))
            host = host.mid(4); // = remove first 4 chars
        if (!LUT.contains(host))
            return Platform::UNK;
        return LUT.value(host);
    }

    static QString toUrl(const QString &key, qreal position = 0.) {
        auto vendor = Platform::toVendor(videoVendor(key));
        if (vendor == Platform::UNK) {
            return {}; // FixMe!
        }
        if (vendor == Platform::YTB) {
            const QString &type = videoType(key);
            const QString &id = videoID(key);
            if (type.startsWith('s')) {
                return shortsVideoPattern + id; // ToDo: position not supported for shorts yet
            } else if (type.startsWith('v')) {
                return standardVideoPattern + id + ((position > 0.)
                                                    ? "&t=" + QString::number(int(position)) +"s"
                                                    : "" );
            } else return {};
        }
    }
};

class YaycUtilities : public QObject {
    Q_OBJECT
public:
    YaycUtilities(QObject *parent) : QObject(parent), tcpSocket(new QTcpSocket(this)) {
        connect(tcpSocket, &QAbstractSocket::connected, this, &YaycUtilities::onSocketConnected);
        connect(tcpSocket, &QAbstractSocket::errorOccurred, this, &YaycUtilities::onSocketError);
    }
    ~YaycUtilities() override {}

    Q_INVOKABLE QUrl urlWithPosition(const QString &url,
                                 const int position) const {
        auto vendor = Platform::urlToVendor(url);
        if (vendor == Platform::UNK) {
            return {}; // FixMe!
        }
        if (vendor == Platform::YTB) {
            if (url.indexOf(QLatin1String("&t=")) >= 0
                    || position == 0)
                return url;
            auto id = getVideoID(url);
            if (id.isEmpty())
                return {};

            return url + QLatin1String("&t=")
                    + QString::number(int(position))
                    + QLatin1Char('s');
        }
        return {};
    }

    Q_INVOKABLE static bool isYoutubeVideoUrl(QUrl url) {
        url = removeWww(url);
        const QString surl = url.toString();
        return (isYoutubeStandardUrl(surl) || isYoutubeShortsUrl(surl));
    }

    Q_INVOKABLE static bool isYoutubeStandardUrl(QUrl url) {
        url = removeWww(url);
        const QString surl = url.toString();
        return isYoutubeStandardUrl(surl);
    }

    static bool isYoutubeStandardUrl(const QString &url) {
        return url.startsWith(standardVideoPattern);
    }

    Q_INVOKABLE static bool isYoutubeShortsUrl(QUrl url) {
        url = removeWww(url);
        const QString surl = url.toString();
        return isYoutubeShortsUrl(surl);
    }

    static bool isYoutubeShortsUrl(const QString &url) {
        return url.startsWith(shortsVideoPattern);
    }

    Q_INVOKABLE QString getVideoID(QUrl url) const {
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
                const QStringRef stripped = surl.midRef(standardVideoPattern.size());
                const int idx = stripped.indexOf("&");
                return "YTBv_" + stripped.mid(0, idx).toString();
            } else if (isYoutubeShortsUrl(surl)) {
                const QStringRef stripped = surl.midRef(shortsVideoPattern.size());
                const int idx = stripped.indexOf("?");
                return "YTBs_" + stripped.mid(0, idx).toString();
            } else {
                return {};
            }
        }
        Q_UNREACHABLE();
        return {}; // Error
    }

    Q_INVOKABLE QString getChangelog() {
        static QString changelog;
        if (changelog.isEmpty()) {
            QFile f(":/CHANGELOG.md");
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)){
                changelog = f.readAll();
                f.close();
            } else {
                changelog = QLatin1String("Error retrieving CHANGELOG");
            }
        }
        return changelog;
    }

    Q_INVOKABLE QString getDisclaimer() {
        static QString disclaimer;
        if (disclaimer.isEmpty()) {
            QFile f(":/DISCLAIMER.md");
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)){
                disclaimer = f.readAll();
                f.close();
            }
        }
        return disclaimer;
    }

    Q_INVOKABLE void checkConnectivity() {
        tcpSocket->connectToHost("github.com", 80);
    }

    Q_INVOKABLE void getLatestVersion() {
        QNetworkRequest request(latestReleaseVersionURL);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                           QNetworkRequest::AlwaysNetwork);
        QNetworkReply *reply = m_nam.get(std::move(request));
        connect(reply, &QNetworkReply::finished, this, &YaycUtilities::onReplyFinished);
        // Errors cause reply to finish in any case.
    }

    Q_INVOKABLE void getDonateEtag() {
        QNetworkRequest request(donateURL);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                           QNetworkRequest::AlwaysNetwork);
        QNetworkReply *reply = m_nam.head(std::move(request));
        connect(reply, &QNetworkReply::finished, this, &YaycUtilities::onDonateEtagReplyFinished);
    }

    Q_INVOKABLE void getDonateURL() {
        QNetworkRequest request(donateURL);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                           QNetworkRequest::AlwaysNetwork);
        QNetworkReply *reply = m_nam.get(std::move(request));
        connect(reply, &QNetworkReply::finished, this, &YaycUtilities::onDonateReplyFinished);
    }

    // ToDo: refactor these into separate utility class
    Q_INVOKABLE void printSettingsPath() {
        QLoggingCategory category("qmldebug");
        QSettings settings;
        qCInfo(category) << "Settings path: "<< settings.fileName();
    }

    Q_INVOKABLE void restartApp(int code = EXIT_CODE_REBOOT) {
        QTimer::singleShot(0, this,
            [code]() {
                auto app = QCoreApplication::instance();
                if (!app)
                    qFatal("QCoreAPplication::instance returned NULL");
                app->exit(code);
        });
    }

    Q_INVOKABLE void clearSettings() {
        restartApp(EXIT_CODE_ERASE_SETTINGS);
    }

    Q_INVOKABLE bool executableExists(const QString &exe) const {
        return isExec(exe);
    }

    Q_INVOKABLE void fetchMissingThumbnails();

    static bool isShortVideo(const QString &fkey) {
        if (!fkey.size())
            return false;
        return videoType(fkey) == QLatin1String("s_"); // strip first 3 chars
    }

    static void openInBrowser(const QString &key, const QString &extWorkingDirRoot) {
        if (!key.size())
            return;

        QDir d(extWorkingDirRoot);

        const bool exists = d.exists() && d.exists(key);
        if (exists) {
            if (isPlasma) {
                // Fix for https://bugs.kde.org/show_bug.cgi?id=442721
                // Note, xdg-open also fails.
                QProcess::startDetached(
                            QLatin1String("/usr/bin/dolphin"),
                            QStringList() << QUrl::fromLocalFile(d.filePath(key)).toString(),
                            extWorkingDirRoot);
            } else {
                QDesktopServices::openUrl( QUrl::fromLocalFile(d.filePath(key)) );
            }
        }
    }

signals:
    void youtubeUrlRequested(const QUrl &url);
    void networkFound();
    void latestVersion(const QString &);
    void donateETag(const QString &);
    void donateUrl(const QString &);

public slots:
    void onSocketConnected() {
        tcpSocket->close();
        emit networkFound();
    }

    void onSocketError() {
        tcpSocket->abort();
    }

    void onReplyFinished() {
        QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
        if (reply) {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError)
                emit latestVersion(QString(reply->readAll()).trimmed());
        }
    }

    void onDonateEtagReplyFinished() {
        static const QByteArray headerName{"ETag"};
        QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
        if (reply) {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError
                && reply->hasRawHeader(headerName)) {
                emit donateETag(reply->rawHeader(headerName));
            }
        }
    }

    void onDonateReplyFinished() {
        QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
        if (reply) {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                emit donateUrl(QString(reply->readAll()).trimmed());
            }
        }
    }

protected:
    QTcpSocket *tcpSocket;
    QNetworkAccessManager m_nam;
};

struct ChannelMetadata
{
    using KeyType = QPair<QString, Platform::Vendor>; // Id, vendor

    QString id;
    QDir channelsRoot;
    QString name;
    QByteArray thumbnailData;
    QDateTime creationDate;
    Platform::Vendor vendor{Platform::YTB};
    bool dirty{false};

    static ChannelMetadata create(const QString &id,
                                  const QString &name,
                                  const Platform::Vendor vendor,
                                  const QDir &parent) {
        ChannelMetadata res;
        res.id = id;
        res.name = name;
        res.vendor = vendor;
        res.creationDate = QDateTime::currentDateTimeUtc();
        res.channelsRoot = parent;
        res.dirty = true;
        return res;
    }
    ChannelMetadata() {}
    ChannelMetadata(const QString &key, const QDir &parent)
        : channelsRoot(parent) {
        const KeyType data = fromKeyString(key);
        id = data.first;
        vendor = data.second;
    }

    bool hasThumbnail() const {
        return !thumbnailData.isEmpty();
    }

    QString key() const  {
        return key(id, vendor);
    }
    static QString key(const QString &id, const Platform::Vendor vendor) {
        return Platform::toString(vendor) + "_" + id;
    }
    static KeyType fromKeyString(const QString &key) {
        const QString platformString = videoVendor(key);
        const QString id = channelID(key);
        return KeyType(id, Platform::toVendor(platformString));
    }

    QString filePath() const {
        return channelsRoot.absoluteFilePath(key() + "." + channelExtension);
    }

    // possibly useless
    void setName(const QString &n) {
        if (name == n)
            return;
        dirty = true;
        name = n;
    }

    void setThumbnail(const QByteArray &ba) {
        if (!ba.size())
            return;
        thumbnailData = ba;
        dirty = true;
    }

    void saveFile() {
        if (!dirty)
            return;
        dirty = false;

        QVariantMap m;
        m["name"] = name;
        m["id"] = id;  // FixMe! Redundant!
        m["creationDate"] = creationDate;
        m["vendor"] = Platform::toString(vendor);
        if (thumbnailData.size())
            m["thumbnail"] = QString::fromLatin1(thumbnailData.toBase64());

        QJsonDocument d = QJsonDocument::fromVariant(m);
        QFile f(filePath());

        f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
        f.write(d.toJson());
        f.close();
    }

    void loadFile() {
        QFile f(filePath());
        if (!f.exists())
            return;
        f.open(QIODevice::ReadOnly | QIODevice::Text);
        QString data = f.readAll();
        f.close();

        QJsonDocument d = QJsonDocument::fromJson(data.toUtf8());
        QVariant v = d.toVariant();
        QVariantMap m = v.toMap();

        name = m.value("name").toString();
        id = m.value("id").toString(); // FixMe! get it from filename!
        if (m.contains("creationDate")) {
            creationDate = m.value("creationDate").toDateTime();
        }
        if (!creationDate.isValid()) {
            QFileInfo check_file(f);
            creationDate = check_file.birthTime().toUTC();
        }
        if (m.contains("thumbnail")) {
            thumbnailData = QByteArray::fromBase64(m.value("thumbnail").toString().toLatin1());
        }
        if (m.contains("vendor")) {
            vendor = Platform::toVendor(m.value("vendor").toString());
        }

        dirty = false;
    }

};

struct VideoMetadata
{
    QString key;
    Platform::Vendor vendor;
    QDir parent;
    QString title;
    QString channelID;
    qreal duration{.0};
    qreal position{.0};
    bool viewed{false};
    bool starred{false};
    QByteArray thumbnailData;
    QDateTime creationDate;
    bool erased{false};

    ~VideoMetadata() {
        if (!erased)
            saveFile();
    }
    VideoMetadata(){}
    VideoMetadata(const QString &k, const QDir &p)
        : key(k), parent(p) {
        vendor = Platform::toVendor(videoVendor(key));
    }

    bool dirty{false};
    bool hasThumbnail() const { return thumbnailData.size(); }

    void setDuration(qreal d) {
        if (d == duration)
            return;
        duration = d;
        dirty = true;
    }

    void setPosition(qreal p) {
        if (p == position)
            return;
        auto oldPosition = position;

        // Don't rewind shorts so they look completed on the bookmark view
        if (isShorts(key) &&  viewed && p < oldPosition)
            return;

        position = p;
        dirty = true;
        const auto threshold = duration * 0.9;

        if (duration > 3. && position > threshold && oldPosition <= threshold) {
            // set only when trespassing threshold
            viewed = true;
        }
    }

    void setViewed(bool v) {
        if (viewed == v)
            return;
        viewed = v;
        dirty = true;
    }

    void setStarred(bool s) {
        if (starred == s)
            return;
        starred = s;
        dirty = true;
    }

    void setTitle(const QString &t) {
        if (title == t)
            return;
        title = t;
        dirty = true;
    }

    void setChannelID(const QString &cid) {
        if (channelID == cid)
            return;
        channelID = cid;
        dirty = true;
    }

    bool update(const QString &t,
                qreal p = 0.,
                qreal d = 0.) {
        bool res = false;
        if (p != position) {
            setPosition(p);
            res = true;
        }
        if (d != duration) {
            setDuration(d);
            res = true;
        }
        if (t != title) {
            setTitle(t);
            res = true;
        }
        return res;
    }

    // immediate, do not dirty
    bool moveLocation(const QDir &d) {
        if (parent == d)
            return true;
        const QString oldName = filePath();
        parent = d;
        const QString newName = filePath();
        QFile f(oldName);
        auto res = f.rename(newName);
        if (!res) {
            qWarning() << "Error moving " << oldName << " to " << newName << " : " << f.errorString();
        }
        return res;
    }

    bool eraseFile() {
        QFile f(filePath());
        erased = true;
        return f.remove();
    }

    void setThumbnail(const QByteArray &ba) {
        if (!ba.size())
            return;
        QImage image;
        if (!image.loadFromData(ba) || image.isNull() || image.size().isEmpty())
            return;

        QSize size = image.size();
        if (size.width() > 128 && size.height() > 128) {
            size.scale(128, 128, Qt::KeepAspectRatio);
            image = image.scaled(size, Qt::IgnoreAspectRatio);
        }

        QByteArray out;
        QBuffer buffer(&out);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG", 0);

        thumbnailData = out;

        dirty = true;
    }

    const QByteArray &thumbnail() const {
        return thumbnailData;
    }

    void saveFile() {
        if (!dirty)
            return;
        dirty = false;

        QVariantMap m;
        m["title"] = title;
        m["duration"] = duration;
        m["position"] = position;
        m["viewed"] = viewed;
        m["channel"] = channelID;
        m["starred"] = starred;
        m["creationDate"] = creationDate;
        if (thumbnailData.size())
            m["thumbnail"] = QString::fromLatin1(thumbnailData.toBase64());

        QJsonDocument d = QJsonDocument::fromVariant(m);
        QFile f(filePath());

        f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);
        f.write(d.toJson());
        f.close();
    }

    void loadFile() {
        QFile f(filePath());
        if (!f.exists())
            return;
        f.open(QIODevice::ReadOnly | QIODevice::Text);
        QString data = f.readAll();
        f.close();

        QJsonDocument d = QJsonDocument::fromJson(data.toUtf8());
        QVariant v = d.toVariant();
        QVariantMap m = v.toMap();

        title = m.value("title").toString();
        position = m.value("position").toReal();
        duration = m.value("duration").toReal();
        if (m.contains("viewed")) {
            viewed = m.value("viewed").toBool();
        } else if (duration > 0. && position > duration * 0.9) {
            viewed = true;
        }
        if (m.contains("starred")) {
            starred = m.value("starred").toBool();
        }
        if (m.contains("channel")) {
            channelID = m.value("channel").toString();
        }
        if (m.contains("thumbnail")) {
            thumbnailData = QByteArray::fromBase64(m.value("thumbnail").toString().toLatin1());
        }
        if (m.contains("creationDate")) {
            creationDate = m.value("creationDate").toDateTime();
        }
        if (!creationDate.isValid()) {
            QFileInfo check_file(f);
            creationDate = check_file.birthTime().toUTC();
        }
        dirty = false;
    }

    QString filePath() const {
        return parent.absoluteFilePath(key + "." + videoExtension);
    }

    QUrl url(bool startingTime = true) const {
        return Platform::toUrl(key, (position > 0. && startingTime) ? position : 0);
    }
};

namespace  {
bool isPlasmaSession() {
#ifdef Q_OS_LINUX
    QProcess plasmaRunning;
    QStringList arguments;
    arguments << "ksmserver";
    plasmaRunning.start("/usr/bin/pidof", arguments);
    plasmaRunning.setStandardOutputFile(QProcess::nullDevice());
    plasmaRunning.setStandardErrorFile(QProcess::nullDevice());

    if(!plasmaRunning.waitForFinished())
        return false; // Not found or pidof does not work

    return !plasmaRunning.exitCode();
#else
    return false;
#endif
}
bool findExecLinux(const QString &name) {
    QProcess findProcess;
    QStringList arguments;
    arguments << name;
    findProcess.start("which", arguments);
    findProcess.setReadChannel(QProcess::ProcessChannel::StandardOutput);

    if(!findProcess.waitForFinished())
        return false; // Not found or which does not work

    QString retStr(findProcess.readAll());

    retStr = retStr.trimmed();

    QFile file(retStr);
    QFileInfo check_file(file);
    if (check_file.exists() && check_file.isFile())
        return true; // Found!
    else
        return false; // Not found!
}
QString sizeString(const QFileInfo &fi)
{
    if (!fi.isFile())
        return QString();
    const qint64 size = fi.size();
    if (size > 1024 * 1024 * 10)
        return QString::number(size / (1024 * 1024)) + QLatin1Char('M');
    if (size > 1024 * 10)
        return QString::number(size / 1024) + QLatin1Char('K');
    return QString::number(size);
}
QString permissionString(const QFileInfo &fi)
{
    const QFile::Permissions permissions = fi.permissions();
    QString result = QLatin1String("----------");
    if (fi.isSymLink())
        result[0] = QLatin1Char('l');
    else if (fi.isDir())
        result[0] = QLatin1Char('d');
    if (permissions & QFileDevice::ReadUser)
        result[1] = QLatin1Char('r');
    if (permissions & QFileDevice::WriteUser)
        result[2] = QLatin1Char('w');
    if (permissions & QFileDevice::ExeUser)
        result[3] = QLatin1Char('x');
    if (permissions & QFileDevice::ReadGroup)
        result[4] = QLatin1Char('r');
    if (permissions & QFileDevice::WriteGroup)
        result[5] = QLatin1Char('w');
    if (permissions & QFileDevice::ExeGroup)
        result[6] = QLatin1Char('x');
    if (permissions & QFileDevice::ReadOther)
        result[7] = QLatin1Char('r');
    if (permissions & QFileDevice::WriteOther)
        result[8] = QLatin1Char('w');
    if (permissions & QFileDevice::ExeOther)
        result[9] = QLatin1Char('x');
    return result;
}
QFileInfoList findFile(const QString &fileName, const QDir &d) {
    QFileInfoList hitList; // Container for matches
    QDirIterator it(d, QDirIterator::Subdirectories);

    // Iterate through the directory using the QDirIterator
    while (it.hasNext()) {
        QString filename = it.next();
        QFileInfo file(filename);

        if (file.isDir()) { // Check if it's a dir
            continue;
        }

        // If the filename contains target string - put it in the hitlist
        if (file.fileName() == fileName) {
            hitList.append(file);
        }
    }
    return hitList;
}
QFileInfoList findFiles(const QDir &d, const QString &ext) {
    const QString nameEnd("." + ext);
    QFileInfoList hitList; // Container for matches
    QDirIterator it(d, QDirIterator::Subdirectories);

    // Iterate through the directory using the QDirIterator
    while (it.hasNext()) {
        QString filename = it.next();
        QFileInfo file(filename);

        if (file.isDir()) { // Check if it's a dir
            continue;
        }

        // If the filename contains target string - put it in the hitlist
        if (file.fileName().endsWith(nameEnd)) {
            hitList.append(file);
        }
    }
    return hitList;
}
QHash<QString, VideoMetadata> cacheRoot(const QDir &d) {
    QHash<QString, VideoMetadata> res;
    const auto &files = findFiles(d, videoExtension);
    for (const auto &f: files) {
        const QString &key = f.baseName();
        const QString &vtype = videoType(key);
        if (!((vtype == QLatin1String("s_")
             || vtype == QLatin1String("v_")) && f.fileName().endsWith(videoExtension))) {
            continue;
        }
        const QDir parent = f.dir();
        res.insert(key, VideoMetadata(key, parent));
        res[key].loadFile();
    }
    return res;
}
QHash<QString, ChannelMetadata> cacheChannels(QDir d) {
    d.cd(".channels");
    QHash<QString, ChannelMetadata> res;
    const auto &files = findFiles(d, channelExtension);
    for (const auto &f: files) {
        const QString &key = f.baseName();
        if (!f.fileName().endsWith(channelExtension)) {
            continue;
        }
        res.insert(key, ChannelMetadata(key, d));
        res[key].loadFile();
    }
    return res;
}
} // namespace

class RequestInterceptor;
class EasylistLoader : public QRunnable
{
public:
    EasylistLoader(const QString &path, RequestInterceptor *interceptor)
        :   m_path(path),
            m_interceptor(interceptor) { }

    void run();

private:
    QString m_path;
    RequestInterceptor *m_interceptor;
};

class RequestInterceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    RequestInterceptor(QObject *parent = nullptr) : QWebEngineUrlRequestInterceptor(parent)
    {
    }

    Q_INVOKABLE void setEasyListPath(QString newPath) {
        if (!m_easyListPath.isEmpty() || m_loading)
            return; // require restart for changes to existing paths to take effect

        if (newPath.startsWith("file://")) {
            newPath = newPath.mid(7);
#if defined(Q_OS_WINDOWS)
            if (newPath[0] == '/')
                newPath = newPath.mid(1); // Strip one more /
#endif
        }

        EasylistLoader *loader = new EasylistLoader(newPath, this);
        loader->setAutoDelete(true);
        m_loading.storeRelease(1);
        QThreadPool::globalInstance()->start(loader);
    }

    void interceptRequest(QWebEngineUrlRequestInfo &info)
    {
        if (m_loading.loadAcquire() == 1)
            return;

        if (client.matches(info.requestUrl().toString().toStdString().c_str(),
            FONoFilterOption, info.requestUrl().host().toStdString().c_str())) {
//                qWarning() << "Blocked: " << info.requestUrl();
                info.block(true);
//                qWarning() << "Blocked: " << ++blocked;
        }
    }

protected:
    QAtomicInt m_loading{0};
    QString m_easyListPath;
    AdBlockClient client;
//    unsigned int blocked{0};

friend class EasylistLoader;
};

void EasylistLoader::run()
{
    QFile file(m_path);
    QString easyListTxt;

    if(!file.exists()) {
        qWarning() << "No easylist.txt file found at " << m_path;
    } else {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)){
            easyListTxt = file.readAll();
        }
        file.close();
        m_interceptor->client.parse(easyListTxt.toStdString().c_str());
    }
    m_interceptor->m_loading.storeRelease(0);
}


class NoDirSortProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

    QString m_searchTerm;
    bool m_searchInTitles{true};
    bool m_searchInChannelNames{true};

    bool m_searchInStarred{true};
    bool m_searchInUnstarred{true};
    bool m_searchInOpened{true};
    bool m_searchInUnopened{true};
    bool m_searchInWatched{true};
    bool m_searchInUnwatched{true};
    bool m_searchInSaved{true};
    bool m_searchInUnsaved{true};
    bool m_searchInShorts{true};
    QString m_workingDirRoot;

    Q_PROPERTY(QString searchTerm READ searchTerm WRITE setSearchTerm NOTIFY searchTermChanged)
    Q_PROPERTY(bool searchInTitles READ searchInTitles WRITE setSearchInTitles NOTIFY searchInTitlesChanged)
    Q_PROPERTY(bool searchInChannelNames READ searchInChannelNames WRITE setSearchInChannelNames NOTIFY searchInChannelNamesChanged)

    Q_PROPERTY(bool searchInStarred MEMBER m_searchInStarred NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInUnstarred MEMBER m_searchInUnstarred NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInOpened MEMBER m_searchInOpened NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInUnopened MEMBER m_searchInUnopened NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInWatched MEMBER m_searchInWatched NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInUnwatched MEMBER m_searchInUnwatched NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInSaved MEMBER m_searchInSaved NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInUnsaved MEMBER m_searchInUnsaved NOTIFY searchParametersChanged)
    Q_PROPERTY(bool searchInShorts MEMBER m_searchInShorts NOTIFY searchParametersChanged)
    Q_PROPERTY(QString workingDirRoot MEMBER m_workingDirRoot NOTIFY searchParametersChanged)

public:
    QString searchTerm() const {
        return m_searchTerm;
    }

    void setSearchTerm(const QString &term) {
        if (term == m_searchTerm)
            return;

        m_searchTerm = term;
        updateSearchTerm();

        emit searchTermChanged();
    }

    bool searchInTitles() const {
        return m_searchInTitles;
    }

    void setSearchInTitles(bool enabled) {
        if (enabled == m_searchInTitles)
            return;

        m_searchInTitles = enabled;
        updateSearchTerm();
        emit searchInTitlesChanged();
    }

    bool searchInChannelNames() const {
        return m_searchInChannelNames;
    }

    void setSearchInChannelNames(bool enabled) {
        if (enabled == m_searchInChannelNames)
            return;

        m_searchInChannelNames = enabled;
        updateSearchTerm();

        emit searchInChannelNamesChanged();
    }


    NoDirSortProxyModel() : QSortFilterProxyModel() {
        connect(this, &NoDirSortProxyModel::searchParametersChanged, [&]() {
                    updateSearchTerm();
                });
    }
    ~NoDirSortProxyModel() override {};

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        QFileSystemModel *fsm = qobject_cast<QFileSystemModel*>(sourceModel());
        bool asc = sortOrder() == Qt::AscendingOrder ? true : false;

        QFileInfo leftFileInfo  = fsm->fileInfo(left);
        QFileInfo rightFileInfo = fsm->fileInfo(right);


        // If DotAndDot move in the beginning
        if (sourceModel()->data(left).toString() == "..")
            return asc;
        if (sourceModel()->data(right).toString() == "..")
            return !asc;

        // Move dirs up
        if (!leftFileInfo.isDir() && rightFileInfo.isDir()) {
            return !asc;
        }
        if (leftFileInfo.isDir() && !rightFileInfo.isDir()) {
            return asc;
        }

        if (leftFileInfo.isDir() && rightFileInfo.isDir()) {
            // Sort dirs alphabetically
            return leftFileInfo.fileName() < rightFileInfo.fileName();
        }

        // uses file modification date, i believe
        return QSortFilterProxyModel::lessThan(left, right);
    }

    void updateSearchTerm() {
        QString pattern;
        if (!m_searchTerm.isEmpty())
            pattern = "*" + m_searchTerm + "*";

        setFilterRegExp(QRegExp(pattern, Qt::CaseInsensitive,
                                              QRegExp::WildcardUnix));
    }

signals:
    void searchTermChanged();
    void searchInTitlesChanged();
    void searchInChannelNamesChanged();
    void searchParametersChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
};

class ThumbnailImageProvider : public QQuickImageProvider
{
    QBasicMutex m_mutex;
    QHash<QString, QImage> m_images;
public:
    ThumbnailImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }

    void insert(const QString &key, const QByteArray &thumb)
    {
        if (!thumb.size() || key.isEmpty())
            return;
        QMutexLocker locker(&m_mutex);
        auto img = QImage::fromData(thumb);
        m_images[key] = std::move(img);
    }

    QImage requestImage(const QString &id,
                        QSize */*size*/,
                        const QSize &/*requestedSize*/) override
    {
        QMutexLocker locker(&m_mutex);

        if (m_images.contains(id))
            return m_images.value(id);

        return emptyImage; // prevents error msg QML Image: Failed to get image from provider: image://videothumbnail/...
    }
};

class EmptyIconProvider : public QFileIconProvider {
public:
    EmptyIconProvider() {
        QImage image({32,32}, QImage::Format_RGB32);
        defaultIcon = QPixmap::fromImage(image);
    }

    ~EmptyIconProvider() override {

    }
    QIcon icon(IconType) const override {
        return defaultIcon;
    }
    QIcon icon(const QFileInfo &) const override {
        return defaultIcon;
    }

    QIcon defaultIcon;
};

class FileSystemModel;
class ThumbnailFetcher : public QObject
{
    Q_OBJECT
public:

    virtual ~ThumbnailFetcher() override {}

    static ThumbnailFetcher & GetInstance() {
        static ThumbnailFetcher instance;
        return instance;
    }

    static void registerModel(FileSystemModel &model) {
        auto &instance = GetInstance();
        instance.m_models.insert(&model);
    }

    static void unregisterModel(FileSystemModel &model) {
        auto &instance = GetInstance();
        instance.m_models.remove(&model);
    }

    static void fetch(const QString &key) {
        auto &instance = GetInstance();
        instance.fetchThumbnail(key);
    }

    static void fetchChannel(const QString &key) {
        auto &instance = GetInstance();
        instance.fetchChannelInternal(key);
    }

    static void fetchChannelAvatar(const QString &channelKey, const QString &url) {
        auto &instance = GetInstance();
        instance.fetchChannelAvatarInternal(channelKey, url);
    }

    static void fetchMissing() {
        printStats();
        auto &instance = GetInstance();
        instance.fetchMissingThumbnails();
    }

    static void printStats() {
        auto &instance = GetInstance();
        instance.fetchMissingThumbnails();
        QLoggingCategory category("qmldebug");
        qCInfo(category) << "Failed fetching "<< instance.m_failures << " thumbnail requests";
        return;
    }


private slots:
    void onThumbnailRequestFinished();
    void onVideoPageRequestFinished();
    void onFetchAvatarRequestFinished();
    void fetchMissingThumbnails();

private:
    explicit ThumbnailFetcher(QObject * parent = nullptr) : QObject(parent) {}
    // ToDo: support other platforms!
    void fetchThumbnail(const QString &key)
    {
        auto ytKey = videoID(key);

        QNetworkRequest req(QUrl(
            QString(QLatin1String("https://img.youtube.com/vi/%1/0.jpg")).arg(ytKey)));
        auto *reply = m_nam.get(req);
        if (!reply)
            qFatal("NULL QNetworkReply while retrieving thumbnails");
        QObject::connect(reply, &QNetworkReply::finished,
                         this, &ThumbnailFetcher::onThumbnailRequestFinished);
        reply->setProperty("key", key);
    }

    void fetchChannelInternal(const QString &key)
    {
        const QString sUrl = Platform::toUrl(key, 0);
//        const Platform::Vendor vendor = Platform::toVendor(videoVendor(key)); // FixMe: handle multiple vendors gracefully
        const QUrl url = sUrl;

        QNetworkRequest req(url);
        m_nam.setCookieJar(new QNetworkCookieJar);
        req.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36");
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
        req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        req.setRawHeader("COOKIE" , "CONSENT=YES+42" );
        auto *reply = m_nam.get(req);
        if (!reply)
            qFatal("NULL QNetworkReply while retrieving thumbnails");
        QObject::connect(reply, &QNetworkReply::finished,
                         this, &ThumbnailFetcher::onVideoPageRequestFinished);
        reply->setProperty("key", key);
    }

    void fetchChannelAvatarInternal(const QString &channelKey, QString url) {
        if (url.isEmpty())
            return;
        url = avatarUrl(url);

        const QUrl u(url);
        QNetworkRequest req(u);
        auto *reply = m_nam.get(req);
        if (!reply)
            qFatal("NULL QNetworkReply while retrieving channel avatar");

        QObject::connect(reply, &QNetworkReply::finished,
                         this, &ThumbnailFetcher::onFetchAvatarRequestFinished);
        ChannelMetadata::KeyType k = ChannelMetadata::fromKeyString(channelKey);
        reply->setProperty("channelKey", channelKey); // embeds channel ID and vendor
    }

private:
    QNetworkAccessManager m_nam;
    QSet<FileSystemModel*> m_models;
    int m_failures = 0;
    int m_channelIdFailures = 0;
};

class FileSystemModel : public QFileSystemModel {
    Q_OBJECT

    bool m_ready{false};
    bool m_bookmarksModel{false};
    QHash<QString, VideoMetadata> m_cache;
    QHash<QString, ChannelMetadata> m_channelCache;
    QModelIndex m_rootPathIndex;
    QScopedPointer<NoDirSortProxyModel> m_proxyModel;
    QString m_contextPropertyName;

    EmptyIconProvider m_emptyIconProvider;
    QDir m_root;

    Q_PROPERTY(QVariant sortFilterProxyModel READ sortFilterProxyModel NOTIFY sortFilterProxyModelChanged)
    Q_PROPERTY(QVariant rootPathIndex READ rootPathIndex NOTIFY rootPathIndexChanged)

public:
    QVariant rootPathIndex() const {
        return QVariant::fromValue(m_rootPathIndex);
    }

    QVariant sortFilterProxyModel() const {
        return QVariant::fromValue(m_proxyModel.get());
    }


    explicit FileSystemModel(QString contextPropertyName,
                             bool bookmarks,
                             QObject *parent = nullptr)
    : QFileSystemModel(parent),
      m_bookmarksModel(bookmarks),
      m_contextPropertyName(contextPropertyName)
    {
        if (m_contextPropertyName.isEmpty()) {
            qFatal("Empty contextPropertyName not supported");
        }
        QStringList filters;
        filters << "*." + videoExtension;

        setNameFilters(filters);
        setNameFilterDisables(false); // false = activate filtering
        setFilter(QDir::AllEntries
                  | QDir::NoDotAndDotDot
                  | QDir::AllDirs
                  //| QDir::Hidden
                  );
        sort(3);
        QScopedPointer<NoDirSortProxyModel> pm(new NoDirSortProxyModel);
        pm->setObjectName("ProxyModel");
        m_proxyModel.swap(pm);
        ThumbnailFetcher::registerModel(*this);
    }

    ~FileSystemModel() override {
        ThumbnailFetcher::unregisterModel(*this);
    }

    inline bool ready() const {
        return m_ready;
    }

    enum Roles  {
        SizeRole = Qt::UserRole + 4,
        DisplayableFilePermissionsRole = Qt::UserRole + 5,
        LastModifiedRole = Qt::UserRole + 6,
        UrlStringRole = Qt::UserRole + 7,
        ContentNameRole = Qt::UserRole + 8,
        TitleRole = Qt::UserRole + 9,
        ChannelNameRole = Qt::UserRole + 10,
        ChannelIdRole = Qt::UserRole + 11,
    };
    Q_ENUM(Roles)

    Q_INVOKABLE QModelIndex setRoot(QString newPath) {
        if (newPath.startsWith("file://")) {
            newPath = newPath.mid(7);
#if defined(Q_OS_WINDOWS)
            newPath = newPath.mid(1); // Strip one more /
#endif
        }

        QQmlApplicationEngine *engine = qobject_cast<QQmlApplicationEngine*>(parent());
        if (!engine) {
            qFatal("Unable to retrieve QQmlApplicationEngine");
        }
        if (!m_proxyModel) {
            qFatal("NULL sortfilter proxy model");
        }
        if (!rootPath().isEmpty() && (rootPath() != ".")) {
            // create a new one
            FileSystemModel *fsmodel = new FileSystemModel(m_contextPropertyName,
                                                           m_bookmarksModel,
                                                           engine);

            this->deleteLater();
            return fsmodel->setRoot(newPath);
        }

        setIconProvider(&m_emptyIconProvider);
        if (newPath.isEmpty()) { // clearing the model
            return {};
        }
        // validate newPath
        m_root = QDir(newPath);
        if (!m_root.exists()) {
            qFatal("Trying to set root directory to non-existent %s\n", newPath.toStdString().c_str());
            return {};
        }

        m_cache = cacheRoot(m_root);
        if (m_bookmarksModel) {
            m_root.mkdir(".channels");
            m_channelCache = cacheChannels(m_root);
        } else {
            qDebug() << "setRoot history model";
        }
        ThumbnailImageProvider *provider = static_cast<ThumbnailImageProvider*>(engine->imageProvider(QLatin1String("videothumbnail")));
        if (!provider) {
            qFatal("Unable to retrieve ThumbnailImageProvider");
        }
        for (const auto &e: qAsConst(m_cache)) {
            if (e.hasThumbnail())
                provider->insert(e.key, e.thumbnailData);
        }
        setResolveSymlinks(true);


        auto res = this->QFileSystemModel::setRootPath(newPath);
        if (res.isValid()) {
            m_proxyModel->setSourceModel(this);

            m_proxyModel->setDynamicSortFilter(true);
            //m_proxyModel->setFilterRegularExpression("^[^\\.].*"); // Skip entries starting with . , this skips .channels and every other hidden dir
            m_proxyModel->setSortRole(LastModifiedRole);
            m_proxyModel->sort(3);

            m_rootPathIndex = m_proxyModel->mapFromSource(res);
            if (!m_rootPathIndex.isValid()) {
                qFatal("Failure mapping FileSystemModel root path index to proxy model");
            }
            engine->rootContext()->setContextProperty(m_contextPropertyName, this);

//            updateSearchTerm();

            if (!m_ready)
                emit firstInitializationCompleted(m_root.path());
            m_ready = true;
            emit sortFilterProxyModelChanged();
            emit rootPathIndexChanged();
            return m_rootPathIndex;
        } else {
            qFatal("Critical failure in QFileSystemModel::setRootPath");
        }
        return QModelIndex();
    }

    Q_INVOKABLE QString key(const QModelIndex &item) const {
        if (!m_ready)
            return QLatin1String("");
        auto index = m_proxyModel->mapToSource(item);
        if (isDir(index)) {
            return QLatin1String("");
        }
        const QString &key = itemKey(index);
        return key;
    }

    Q_INVOKABLE QString title(const QModelIndex &item) const {
        if (!m_ready)
            return QLatin1String("");
        const QString &key = keyFromViewItem(item);
        if (!key.size() || !m_cache.contains(key))
            return QLatin1String("");

        return m_cache.value(key).title;
    }

    Q_INVOKABLE bool isVideoBookmarked(const QString &key) {
        return m_cache.contains(key);
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (index.isValid() && role >= Qt::UserRole) {
            switch (role) {
            case SizeRole:
                return QVariant(sizeString(fileInfo(index)));
            case DisplayableFilePermissionsRole:
                return QVariant(permissionString(fileInfo(index)));
            case LastModifiedRole:
                return QVariant(fileInfo(index).lastModified().toString(QStringLiteral("yyyyMMddhhmmss")));
            case UrlStringRole: {
                if (isDir(index)) {
                    return {};
                }
                const QString &key = itemKey(index);
                if (!m_cache.contains(key)) {
                    qWarning() << "key not found "<<key << " "  << fileInfo(index).baseName();
                    return {};
                }
                return m_cache.value(key).url();
            }
            case ContentNameRole:
            case QFileSystemModel::FileNameRole:
            case Qt::DisplayRole: {
                /*          QFileSystemModel:
                            case 0: return d->displayName(index);
                            case 1: return d->size(index);
                            case 2: return d->type(index);
                            case 3: return d->time(index);
                */
                switch (index.column()) {
                case 0: {
                    if (!isDir(index)) {
                        const QString &key = itemKey(index);
                        if (!m_cache.contains(key)) {
                            return QFileSystemModel::data(index, role);
                        }
                        return key;
                    }
                    return QFileSystemModel::data(index, role);
                }
                case 3: {
                    return QVariant(fileInfo(index).lastModified().toString(QStringLiteral("yyyy.MM.dd hh:mm:ss")));
                }
                default:
                    return QFileSystemModel::data(index, role);
                }
            }
            case TitleRole: {
                if (!isDir(index)) {
                    const QString &key = itemKey(index);
                    if (!m_cache.contains(key)) {
                        return {};
                    }
                    const auto &title = m_cache.value(key).title;
                    if (title.size())
                        return title;
                } else {
                    return QFileSystemModel::data(index, role);
                }
            }
            case ChannelNameRole: {
                if (!isDir(index)) {
                    const QString &key = itemKey(index);
                    if (!m_cache.contains(key)) {
                        return {};
                    }
                    auto cid = m_cache.value(key).channelID;
                    auto cVendor = m_cache.value(key).vendor;
                    auto cKey = ChannelMetadata::key(cid, cVendor);
                    if (m_channelCache.contains(cKey)) {
                        return m_channelCache.value(cKey).name;
                    }
                    return {};
                }
                return {};
            }
            case ChannelIdRole: {
                if (!isDir(index)) {
                    const QString &key = itemKey(index);
                    if (!m_cache.contains(key)) {
                        return {};
                    }
                    return m_cache.value(key).channelID;
                }
                return {};
            }
            default:
                break;
            }
        }
        return QFileSystemModel::data(index, role);
    }
    QHash<int,QByteArray> roleNames() const override
    {
         QHash<int, QByteArray> result = QFileSystemModel::roleNames();
         result.insert(SizeRole, QByteArrayLiteral("size"));
         result.insert(DisplayableFilePermissionsRole, QByteArrayLiteral("displayableFilePermissions"));
         result.insert(LastModifiedRole, QByteArrayLiteral("lastModified"));
         result.insert(ContentNameRole, QByteArrayLiteral("contentName"));
         return result;
    }

public slots:
    QString keyFromViewItem(const QModelIndex &item) const {
        return key(item); // ToDo: Deduplicate!
    }

    QVariant videoUrl(QModelIndex item) {
        if (!m_ready)
            return QVariant();
        item = m_proxyModel->mapToSource(item);

        if (!filePath(item).size()) {
            qWarning() << "invalid input";
            return QVariant();
        }
        if (isDir(item)) {
            qWarning() << "No URL for categories";
            return QVariant();
        }

        return data(item, UrlStringRole);
    }

    void openInBrowser(QModelIndex item, const QString &extWorkingDirRoot) {
        if (!m_ready)
            return ;
        const QString &key = keyFromViewItem(item);
        if (!key.size() || !m_cache.contains(key))
            return;

        return YaycUtilities::openInBrowser(key, extWorkingDirRoot);
    }

    // ToDo: move to utilities
    void openInExternalApp(QModelIndex item
                           , const QString &extCommand
                           , const QString &extWorkingDirRoot) { // ToDo: refactor this, use qfuture, track ongoing processes
        if (!m_ready)
            return ;
        const QString &key = keyFromViewItem(item);
        if (!key.size() || !m_cache.contains(key))
            return ;

        QDir d(extWorkingDirRoot);

        if (!d.exists()) {
            QLoggingCategory category("qmldebug");
            qCInfo(category) << "openInExternalApp: not existing working dir "<<extWorkingDirRoot;
            return;
        }

        if (!d.exists(key)) {
            if (!d.mkdir(key)) {
                QLoggingCategory category("qmldebug");
                qCInfo(category) << "openInExternalApp: failed creating "<<d.filePath(key);
                return;
            }
        }

        QString url = m_cache.value(key).url(false).toString();
        QProcess process;
        process.setProgram(extCommand);
        process.setArguments(QStringList() << url);
        process.setWorkingDirectory(d.filePath(key));
        process.setStandardOutputFile(QProcess::nullDevice());
        process.setStandardErrorFile(QProcess::nullDevice());
        qint64 pid;
        if (!process.startDetached(&pid)) {
            QLoggingCategory category("qmldebug");
            qCInfo(category) << "openInExternalApp: failed QProcess::startDetached";
        } else {
            auto idx = index(m_cache.value(key).filePath());
            emit dataChanged(idx, idx);
        }
    }

    bool deleteEntry(QModelIndex item) {
        if (!m_ready)
            return false;
        item = m_proxyModel->mapToSource(item);

        if (!filePath(item).size()) {
            qWarning() << "invalid input";
            return false;
        }
        bool res = false;
        if (isDir(item)) {
            // do not allow to delete non-empty directories
            QDir d(filePath(item));

            if (d.isEmpty()) { // FixMe: remove entries from cache!
                res = d.removeRecursively();
                remove(item);
            } else {
                qWarning() << "Category not empty! " << fileInfo(item).baseName();
                res = false;
            }
            return res;
        } else {
            const QString &key = itemKey(item);
            if (!m_cache.contains(key)) {
                qWarning() << "Not present in cache: " << key;
                return false;
            }

            auto entry = m_cache.take(key);
            res = entry.eraseFile();
            return res;
        }
    }
    void sync() {
        for (auto &e: m_cache) {
            e.saveFile();
        }
        for (auto &c: m_channelCache) {
            c.saveFile();
        }
    }
    qreal progress(const QString &key) const {
        if (!m_ready)
            return 0;

        if (!m_cache.contains(key)) {
            qWarning() << "FileSystemModel::progress: Key "<<key<<" not present!";
            return 0;
        }

        if (!key.size())
            return 0.;

        const auto &position = m_cache.value(key).position;
        const auto &duration = m_cache.value(key).duration;
        if (duration == 0.)
            return 0.;
        return position / duration;
    }
    qreal duration(const QModelIndex &item) const {
        if (!m_ready)
            return 0;
        const QString &key = keyFromViewItem(item);
        return duration(key);
    }
    qreal duration(const QString &key) const {
        if (!m_ready)
            return 0;


        if (!key.size())
            return 0.;

        if (!m_cache.contains(key)) {
            qWarning() << "FileSystemModel::duration: Key "<<key<<" not present!";
            return 0;
        }

        return m_cache.value(key).duration;
    }
    bool isShortVideo(const QModelIndex &item) const {
        if (!m_ready)
            return false;
        const QString &key = keyFromViewItem(item);
        return YaycUtilities::isShortVideo(key);
    }
    bool isViewed(const QModelIndex &item) const {
        if (!m_ready)
            return false;
        const QString &key = keyFromViewItem(item);
        return isViewed(key);
    }
    bool isViewed(const QString &key) const {
        if (!m_ready)
            return false;
        if (!key.size() || !m_cache.contains(key))
            return false;
        return m_cache.value(key).viewed;
    }
    void viewEntry(const QModelIndex &item, bool viewed) {
        if (!m_ready)
            return;
        const bool currentValue = isViewed(item);
        if (viewed == currentValue)
            return;
        const QString &key = keyFromViewItem(item);
        viewEntry(key, viewed);
    }
    void viewEntry(const QString &key, bool viewed) {
        if (!m_ready)
            return;

        if (!key.size() || !m_cache.contains(key))
            return;

        m_cache[key].setViewed(viewed);
        auto idx = index(m_cache[key].filePath());
        emit dataChanged(idx, idx);
    }

    bool isStarred(const QModelIndex &item) const {
        if (!m_ready)
            return false;
        const QString &key = keyFromViewItem(item);
        return isStarred(key);
    }
    bool isStarred(const QString &key) const {
        if (!key.size() || !m_cache.contains(key))
            return false;
        return m_cache.value(key).starred;
    }
    // ToDo: check if directory is empty?
    bool hasWorkingDir(const QModelIndex &item, const QString &extWorkingDirRoot) const {
        if (!m_ready)
            return false;
        const QString &key = keyFromViewItem(item);
        return hasWorkingDir(key, extWorkingDirRoot);
    }
    bool hasWorkingDir(const QString &key, const QString &extWorkingDirRoot) const {
        if (!m_ready || !key.size() || !m_cache.contains(key))
            return false;

        QDir d(extWorkingDirRoot);

        const bool exists = d.exists() && d.exists(key);
        return exists;
    }
    void starEntry(const QModelIndex &item, bool starred) {
        if (!m_ready)
            return;
        const bool currentValue = isStarred(item);
        if (starred == currentValue)
            return;
        const QString &key = keyFromViewItem(item);
        starEntry(key, starred);
    }
    void starEntry(const QString &key, bool starred) {
        if (!key.size() || !m_cache.contains(key))
            return;

        m_cache[key].setStarred(starred);
        auto idx = index(m_cache[key].filePath());
        emit dataChanged(idx, idx);
    }
    QString videoIconUrl(const QModelIndex &item) const {
        if (!m_ready)
            return QLatin1String("");
        const bool shortVideo = isShortVideo(item);
        const bool viewed = isViewed(item);
        if (shortVideo) {
            if (viewed)
                return QLatin1String("qrc:/images/shortChecked.png");
            else
                return QLatin1String("qrc:/images/short.png");
        } else if (viewed) {
            return QLatin1String("qrc:/images/videoChecked.png");
        }
        return QLatin1String("qrc:/images/video.png");
    }
    bool moveVideo(const QString &key, QModelIndex destinationDir) {
        if (!m_ready)
            return false;

        destinationDir = m_proxyModel->mapToSource(destinationDir);

        if (!m_cache.contains(key)
                || !m_cache[key].filePath().size()
                || !filePath(destinationDir).size()
                || !isDir(destinationDir)) {
            qWarning() << "Invalid input trying to move " << key << " into " << filePath(destinationDir);
            return false;
        }

        QDir d(filePath(destinationDir));
        if (!d.exists()) {
            qWarning() << "Destination directory doesn't exist";
            return false;
        }

        QTimer::singleShot(0, this,
            [this, key, d]() { moveEntry(key, d); });
        return true;
    }
    bool moveEntry(QModelIndex item, QModelIndex destinationDir) {
        if (!m_ready)
            return false;

        item = m_proxyModel->mapToSource(item);
        destinationDir = m_proxyModel->mapToSource(destinationDir);

        if (!filePath(item).size() || !filePath(destinationDir).size() || !isDir(destinationDir)) {
            qWarning() << "invalid input";
            return false;
        }

        QDir d(filePath(destinationDir));
        if (!d.exists()) {
            qWarning() << "Destination directory doesn't exist";
            return false;
        }

        if (isDir(item)) { // moving a category
            QDir f(filePath(item));
            if (!f.exists()) {
                qWarning() << "directory to move doesn't exist";
                return false;
            }
            QString newName = d.absoluteFilePath(fileName(item));


            const bool res = f.rename(f.absoluteFilePath(""), newName); // this one nicely fails if parent is moved into child
            if (res) {
                m_cache = cacheRoot(rootDirectory()); // regenerate cache
            }
            return res;
        } else {
            const QString &key = itemKey(item);
            if (!m_cache.contains(key)) {
                qWarning() << "Not present in cache: " << key;
                return false;
            }

            QTimer::singleShot(0, this,
                [this, key, d]() { moveEntry(key, d); });
            return true;
        }
    }

    void moveEntry(const QString &key, const QDir &d) {
        if (!m_ready)
            return;
        if (!m_cache.contains(key))
            return;
        m_cache[key].moveLocation(d);
    }

    bool addCategory(const QString &name) {
        if (!m_ready)
            return false;
        QDir d(rootPath());
        return d.mkdir(name);
    }

    bool updateEntry(const QString &key,
                     const QString &title,
                     const QString &channelURL,
                     const QString &channelAvatarURL,
                     const QString &channelName,
                     const qreal duration = 0.,
                     const qreal position = 0.) {
        if (!m_ready)
            return false;
        if (!m_cache.contains(key)) {
            return false;
        }

        auto channelID = channelURL.mid(24); // strip https://www.youtube.com/
        m_cache[key].setChannelID(channelID);
        if (!channelID.isEmpty() && m_bookmarksModel) {
            addChannel(channelID,
                       Platform::YTB,
                       channelName,
                       channelAvatarURL);
        }
        bool updated = m_cache[key].update(title, position, duration);
        if (updated) {
            auto idx = index(m_cache[key].filePath());
            emit dataChanged(idx, idx);
        }
        return true;
    }

    void updateChannelID(const QString &key,
                         const QString &channelID) {
        if (!m_ready || !m_bookmarksModel)
            return;
        if (!m_cache.contains(key)) {
            return;
        }
        m_cache[key].setChannelID(channelID); // vendor is embedded in the key/Video already
    }

    void updateChannelAvatar(const QString &channelKey,
                             const QByteArray avatar) {
        if (!m_ready || !m_bookmarksModel)
            return;
        if (!m_channelCache.contains(channelKey)) {
            return;
        }
        m_channelCache[channelKey].setThumbnail(avatar);
        m_channelCache[channelKey].saveFile();
    }


    bool addEntry(const QString &key,
                  const QString &title,
                  const QString &channelURL,
                  const QString &channelAvatarURL,
                  const QString &channelName,
                  const qreal duration = 0.,
                  const qreal position = 0.)
    {
        if (!m_ready)
            return false;

        if (!m_cache.contains(key))
            m_cache.insert(key, VideoMetadata(key, rootDirectory()));
        if (!m_cache.value(key).hasThumbnail()) {
            fetchThumbnail(key);
        }
        // ToDo: handle more platforms!
        m_cache[key].update(title, position, duration);
        m_cache[key].creationDate = QDateTime::currentDateTimeUtc();

        if (channelURL.isEmpty()) {
            ThumbnailFetcher::fetchChannel(key);
        } else {
            auto channelID = channelURL.mid(24); // strip https://www.youtube.com/
            m_cache[key].setChannelID(channelID);
            if (!channelID.isEmpty() && m_bookmarksModel) {
                addChannel(channelID,
                           Platform::YTB,
                           channelName,
                           channelAvatarURL);
            }
        }

        if (YaycUtilities::isShortVideo(key) && !channelURL.isEmpty()) {
            m_cache[key].viewed = true;
        }

        m_cache[key].saveFile(); // so it pops up on the view
        return true;
    }

signals:
    void filesAdded(const QVariantList & addedPaths);
    void rootPathIndexChanged();
    void sortFilterProxyModelChanged();
    void searchTermChanged();
    void searchInTitlesChanged();
    void searchInChannelNamesChanged();
    void firstInitializationCompleted(const QString &rootPath);

private slots:


private:
    void addThumbnail(const QString &key, const QByteArray &thumbnailData) {
        if (m_cache.contains(key) && !m_cache[key].hasThumbnail()) {
            m_cache[key].setThumbnail(thumbnailData);
        }
    }

    void updateChannel(const QString &key,
                       const QString &channelId,
                       const QString &channelName) {
        if (m_cache.contains(key)) {
            m_cache[key].channelID = channelId;
        }
    }

    void addChannel(const QString &channelId,
                    const Platform::Vendor vendor,
                    const QString &channelName,
                    const QString &channelAvatarURL) {
        if (!m_bookmarksModel || !m_ready)
            return;
        const QString &key = ChannelMetadata::key(channelId, vendor);
        bool avatarNeedsFetch = true;
        bool persist = false;
        if (m_channelCache.contains(key)) {
            avatarNeedsFetch = !m_channelCache[key].hasThumbnail();
            m_channelCache[key].setName(channelName);
        } else {
            QDir d(m_root);
            d.cd(".channels");
            m_channelCache[key] = ChannelMetadata::create(channelId,
                                                          channelName,
                                                          vendor,
                                                          d);
            persist = true;
        }
        if (avatarNeedsFetch)
            ThumbnailFetcher::fetchChannelAvatar(key, channelAvatarURL);

    }
    // this one needs the QModelIndex in FileSystemModel space!
    QString itemKey(const QModelIndex &index) const {
        if (!m_ready)
            return QLatin1String("");
        const auto &fi = fileInfo(index);
        const QString &baseName = fi.baseName();
        return baseName;
    }

    void fetchThumbnail(const QString &key) {
        ThumbnailFetcher::fetch(key);
    }

friend class ThumbnailFetcher;
}; // FileSysyemModel

bool NoDirSortProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QRegExp re = filterRegExp();

    FileSystemModel *fsm = qobject_cast<FileSystemModel *>(sourceModel());

    QModelIndex nameIndex = fsm->index(sourceRow, 0, sourceParent);
    const bool isDir = fsm->hasChildren(nameIndex);

    const QString key = fsm->data(nameIndex, FileSystemModel::FileNameRole).toString();

    if (isDir) { // pass all directories (categories), filter out .* directories
                 // otherwise, if parent is filtered out, children won't be processed
        return key.contains(allowedDirsPattern);
    }

    QString title = fsm->data(nameIndex, FileSystemModel::TitleRole).toString();

//    QLoggingCategory category("qmldebug");
//    qCInfo(category) << "Analyzing "<< title << " k "<<key;

    const bool starred = fsm->isStarred(key);
    if ((starred && !m_searchInStarred) || (!starred && !m_searchInUnstarred))
        return false;

    const bool shortVideo = YaycUtilities::isShortVideo(key);
    if (shortVideo && !m_searchInShorts)
        return false;

    const bool opened = !shortVideo && fsm->duration(key) > 0.;
    if ((opened && !m_searchInOpened) || (!opened && !m_searchInUnopened))
        return false;


    const bool viewed = !shortVideo && fsm->isViewed(key);
    if ((viewed && !m_searchInWatched) || (!viewed && !m_searchInUnwatched))
        return false;

    const bool hasWorkingDir = m_workingDirRoot.isEmpty() || fsm->hasWorkingDir(key, m_workingDirRoot);
    if ((!hasWorkingDir && !m_searchInUnsaved) || (hasWorkingDir && !m_searchInSaved))
        return false;

    if (re.isEmpty())
        return true;

    const QString channelName =
            fsm->data(nameIndex, FileSystemModel::ChannelNameRole).toString();

    const QString channelId =
            fsm->data(nameIndex, FileSystemModel::ChannelIdRole).toString();


    bool searchInTitles = m_searchInTitles || (!m_searchInTitles && !m_searchInChannelNames);

    bool res = false;
    if (searchInTitles)
        res |= title.contains(re);
    if (m_searchInChannelNames) {
        res |= channelName.contains(re);
        res |= channelId.contains(re);
    }

    return res;
}

void ThumbnailFetcher::onThumbnailRequestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        qFatal("NULL QNetworkReply while retrieving thumbnails");

    if (reply->error() == QNetworkReply::NoError ) {
        QString key = reply->property("key").toString();
        QByteArray networkContent = reply->readAll();
        if (networkContent.size()) {
            for (auto &m: qAsConst(m_models)) {
                m->addThumbnail(key, networkContent);
            }
            if (m_models.size()) {
                QQmlApplicationEngine *engine =
                        qobject_cast<QQmlApplicationEngine*>((*m_models.begin())->parent());
                ThumbnailImageProvider *provider = static_cast<ThumbnailImageProvider*>(engine->imageProvider(QLatin1String("videothumbnail")));
                provider->insert(key, networkContent);
            }
        } else {
            ++m_failures;
        }
    } else {
        qWarning() << "Error while retrieving thumbnail: " << reply->errorString() << " : "
                  << reply->url() ;
        ++m_failures;
    }
    reply->deleteLater();
}

void ThumbnailFetcher::onVideoPageRequestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        qFatal("NULL QNetworkReply while retrieving thumbnails");
    if (reply->error() == QNetworkReply::NoError ) {
        const QString key = reply->property("key").toString();

        QByteArray networkContent = reply->readAll();
        if (networkContent.size()) {
            // parse content,  update key with parsed channel id
            QString sData = QString::fromLatin1(networkContent);
            QRegularExpression re("<span itemprop=\"author\" itemscope itemtype=\"http://schema.org/Person\"><link itemprop=\"url\" href=\"http://www.youtube.com/(.+?)\"><link itemprop=\"name\" content=\"(.+?)\">");
            QRegularExpressionMatch match = re.match(sData);
            if (match.hasMatch()) {
                const QString channelId = match.captured(1);
                const QString channelName = match.captured(2);

                QRegularExpression re2;
                if (!isShorts(key)) {
                    re2 = QRegularExpression("channelAvatar\":\\{\"thumbnails\":\\[\\{\"url\":\"(https://.*?)\"");
                } else {
                    re2 = QRegularExpression("canonicalBaseUrl\":\"/"+channelId+"\"\\}\\}\\}\\]\\},\"channelThumbnail\":\\{\"thumbnails\":\\[\\{\"url\":\"(https://.*?)\"");
                }

                QRegularExpressionMatch match2 = re2.match(sData);

                QString channelAvatarURL;
                if (match2.hasMatch())
                    channelAvatarURL = match2.captured(1);

                for (auto &m: qAsConst(m_models)) {
                    m->addChannel(channelId,
                                  Platform::toVendor(videoVendor(key)),
                                  channelName,
                                  channelAvatarURL);
                    m->updateChannelID(key, channelId);
                }
            } else {
                ++m_channelIdFailures;
            }
        } else {
            ++m_channelIdFailures;
        }
    } else {
        qWarning() << "Error while retrieving video page: " << reply->errorString() << " : "
                  << reply->url() ;
        ++m_channelIdFailures;
    }
    reply->deleteLater();
}

void ThumbnailFetcher::onFetchAvatarRequestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        qFatal("NULL QNetworkReply while retrieving thumbnails");
    const QString channelKey = reply->property("channelKey").toString();

    if (reply->error() == QNetworkReply::NoError ) {
        const QByteArray &networkContent = reply->readAll();
        for (auto &m: qAsConst(m_models)) {
            m->updateChannelAvatar(channelKey, networkContent);
        }
    } else {
        qWarning() << "Error while retrieving channel avatar: " << reply->errorString() << " : "
                  << reply->url() ;
    }
    reply->deleteLater();
}

void ThumbnailFetcher::fetchMissingThumbnails() {
    QSet<QString> missingKeys;
    for (auto &m : qAsConst(m_models)) {
        for (auto i = m->m_cache.begin(); i != m->m_cache.end(); ++i) {
            if (!i.value().hasThumbnail())
                missingKeys.insert(i.key());
        }
    }
    for (const auto &k: missingKeys) {
        fetchThumbnail(k);
    }
}

void YaycUtilities::fetchMissingThumbnails()
{
    ThumbnailFetcher::fetchMissing();
}

int main(int argc, char *argv[])
{
    int currentExitCode = 0;
    QStringList args;
    {
        auto appstartTS = QDateTime::currentDateTimeUtc();
        QCoreApplication::setOrganizationName("YAYC");
        QCoreApplication::setApplicationName("yayc");

        QSettings settings;
#if defined(Q_OS_LINUX)
        qputenv("QT_QPA_PLATFORMTHEME", QByteArrayLiteral("gtk3"));
#endif
        qputenv("QT_QUICK_CONTROLS_STYLE", QByteArrayLiteral("Material"));


        if (!settings.contains("darkMode") || settings.value("darkMode").toBool()) {
            // https://chromium.googlesource.com/chromium/src/+/821cfffb54899797c86ca3eb351b73b91c2c5879/third_party/blink/web_tests/VirtualTestSuites
            qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
                    QByteArrayLiteral("--dark-mode-settings=ImagePolicy=1 --blink-settings=forceDarkModeEnabled=true"));  // Current Chromium
            qputenv("QT_QUICK_CONTROLS_MATERIAL_THEME", QByteArrayLiteral("Dark")); // ToDo: fix text color
        }
        qputenv("QT_QUICK_CONTROLS_MATERIAL_PRIMARY", QByteArrayLiteral("#3d3d3d"));
        qputenv("QT_QUICK_CONTROLS_MATERIAL_ACCENT", QByteArrayLiteral("Red"));
        qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", QByteArrayLiteral("Dense")); // ToDo: add setting

        qInfo("Starting YAYC v%s ...", appVersion().data());
#ifdef QT_NO_DEBUG_OUTPUT
//        QLoggingCategory::setFilterRules(QStringLiteral("*=false\n"
//                                                        "qmldebug=true\n"
//                                                        "*.fatal=true\n"
//                                                        ));
#endif

        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
        QtWebEngine::initialize();

        QGuiApplication app(argc, argv);
        app.setWindowIcon(QIcon(":/images/yayc-alt.png"));
        args = app.arguments();


        QQmlApplicationEngine engine;
        ThumbnailImageProvider *imageProvider = new ThumbnailImageProvider();
        engine.addImageProvider(QLatin1String("videothumbnail"), imageProvider);
        const QUrl url(QStringLiteral("qrc:/main.qml"));
        QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                         &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);


        FileSystemModel *historyModel =
               new FileSystemModel("historyModel", false, &engine);
        FileSystemModel *fsmodel =
               new FileSystemModel("fileSystemModel", true, &engine);

        engine.rootContext()->setContextProperty("fileSystemModel", fsmodel);
        engine.rootContext()->setContextProperty("historyModel", historyModel);

        // for the roles enums
        qmlRegisterUncreatableType<FileSystemModel>("yayc", 1, 0,
                                                    "FileSystemModel", "Cannot create a FileSystemModel instance.");


        RequestInterceptor *interceptor = new RequestInterceptor(&engine);
        YaycUtilities *utilities = new YaycUtilities(&engine);

        engine.rootContext()->setContextProperty("utilities", utilities);
        engine.rootContext()->setContextProperty("requestInterceptor", interceptor);
        engine.rootContext()->setContextProperty("appVersion", QString(appVersion()) );
        engine.rootContext()->setContextProperty("repositoryURL", repositoryURL );

        isPlasma = isPlasmaSession();

        QObject::connect(fsmodel, &FileSystemModel::firstInitializationCompleted,
                         [fsmodel, appstartTS, &settings](const QString &path) {
            if (!fsmodel->ready()) {
                if (!settings.contains("debugMode") || !settings.value("debugMode").toBool())
                    return;
                auto modelReadyTS = QDateTime::currentDateTimeUtc();
                auto msecs = appstartTS.msecsTo(modelReadyTS);
                QLoggingCategory category("qmldebug");
                qCInfo(category) << "Starting time for "
                                 << path<< " : " << msecs << " ms";
            }
        });
        engine.load(url);

        QTimer::singleShot(1000,[&engine, interceptor](){
            QObject *view = engine.rootObjects().first()->findChild<QObject *>("webEngineView");
            QQuickWebEngineProfile *profile = qvariant_cast<QQuickWebEngineProfile *>(view->property("profile"));

            profile->setUrlRequestInterceptor(interceptor);
        });

        currentExitCode = app.exec();
    }
    if ( currentExitCode <=  EXIT_CODE_ERASE_SETTINGS) {
        if (currentExitCode == EXIT_CODE_ERASE_SETTINGS) {
            QLoggingCategory category("qmldebug");
            qCInfo(category) << "Erasing settings...";

            QSettings settings;
            settings.clear();
            settings.sync();
        }

        QProcess::startDetached(args[0], args); //application restart
        return 0;
    }
    return currentExitCode;
}

#include "main.moc"
