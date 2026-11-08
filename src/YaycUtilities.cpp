/*
Copyright (C) 2023- YAYC team <info@yayc.stream>

This work is licensed under the terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/ or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

In addition to the above,
- The use of this work for training, fine-tuning, or otherwise feeding artificial intelligence systems is prohibited for both commercial and non-commercial use.
  This includes, but is not limited to, the ingestion of this work into large language models (LLMs), code generation models,
  Retrieval-Augmented Generation (RAG) systems, embedding databases, vector stores, or any other AI-assisted system.
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
#include <QJsonDocument>
#include <QJsonObject>
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
#include <QNetworkProxy>
#include <QStyleHints>
#include <QGuiApplication>
#include <QClipboard>
#include <QQuickItem>
#include <QQuickWindow>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QCursor>
#include <QSocketNotifier>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

volatile sig_atomic_t s_quitWriteFd = -1;
// Set in the handler itself: no re-arm even before the event loop sees it
volatile sig_atomic_t s_quitSignalled = 0;
// One self-pipe per process: the handler and the fd are process-wide.
// The rest belongs to the owner, and its destructor resets it
YaycUtilities *s_quitOwner = nullptr;
QSocketNotifier *s_quitNotifier = nullptr;
int s_quitFds[2]{-1, -1}; // [0] written by the signal handler, [1] read here
bool s_quitRequested = false;
bool s_quitArmed = false; // s_previousQuitAction holds what to restore
struct sigaction s_previousQuitAction {};

void onQuitSignal(int)
{
    // Async-signal-safe calls only. SA_RESETHAND already restored SIG_DFL,
    // so a second SIGTERM kills if the clean quit hangs
    const int savedErrno = errno;
    s_quitSignalled = 1;
    const char byte = 1;
#ifdef MSG_NOSIGNAL
    [[maybe_unused]] const ssize_t n = ::send(s_quitWriteFd, &byte, 1, MSG_NOSIGNAL);
#else
    [[maybe_unused]] const ssize_t n = ::write(s_quitWriteFd, &byte, 1); // SO_NOSIGPIPE set
#endif
    errno = savedErrno;
}

} // namespace
#endif
#include <QtWebEngineQuick/qquickwebengineprofile.h>

// Static member definitions
bool YaycUtilities::isPlasma = false;
QDateTime YaycUtilities::appstartTS;
QString YaycUtilities::settingsFileToDelete;

YaycUtilities::YaycUtilities(QObject *parent)
    : QObject(parent)
    , tcpSocket(new QTcpSocket(this))
{
    connect(tcpSocket, &QAbstractSocket::connected, this, &YaycUtilities::onSocketConnected);
    connect(tcpSocket, &QAbstractSocket::errorOccurred, this, &YaycUtilities::onSocketError);
}

YaycUtilities::~YaycUtilities()
{
#ifdef Q_OS_UNIX
    if (s_quitOwner == this) {
        if (s_quitArmed)
            ::sigaction(SIGTERM, &s_previousQuitAction, nullptr);
        s_quitWriteFd = -1;
        s_quitSignalled = 0;
        s_quitOwner = nullptr;
        s_quitArmed = false;
        s_quitRequested = false;
        delete s_quitNotifier; // before its fd closes
        s_quitNotifier = nullptr;
        // Write end stays open: a handler racing this on another thread must
        // never hit a closed, reused fd. One fd, only when this object dies
        ::close(s_quitFds[1]);
        s_quitFds[0] = s_quitFds[1] = -1;
    }
#endif
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

// // FIXME: update the API to accept a WebEngineProfile instead
// void YaycUtilities::addRequestInterceptor(QObject *webEngineView)
// {
//     QQmlEngine *engine = qmlEngine(webEngineView);
//     if (!engine)
//         qFatal("NULL engine for view");
//     QQuickWebEngineProfile *profile =
//         qvariant_cast<QQuickWebEngineProfile *>(webEngineView->property("profile"));
//     if (!profile)
//         qFatal("NULL profile for view");

//     RequestInterceptor *interceptor = new RequestInterceptor(engine);
//     profile->setUrlRequestInterceptor(interceptor);
//     engine->rootContext()->setContextProperty("requestInterceptor", interceptor); // for the file dialog
// }

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
    return url.startsWith(youtubeChannelPattern);
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
        qWarning() << "Unknown Video platform for :" << url;
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
    qWarning() << "getVideoID error: "<<url;
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

// youtu.be short links are handled asynchronously via resolveAndNormalizeUrl(),
// because we can't tell from the URL alone whether the video is a standard video
// or a Short. A HEAD request to youtu.be follows the redirect to the actual
// youtube.com URL (watch?v= or shorts/), giving us the correct form.
QString YaycUtilities::normalizeVideoUrl(QUrl url) const
{
    url = removeWww(url);
    const QString surl = url.toString();
    if (isYoutubeStandardUrl(surl)) {
        const QStringView stripped = QStringView{surl}.mid(standardVideoPattern.size());
        const int idx = stripped.indexOf('&');
        return standardVideoPattern + stripped.mid(0, idx).toString();
    } else if (isYoutubeShortsUrl(surl)) {
        const QStringView stripped = QStringView{surl}.mid(shortsVideoPattern.size());
        const int idx = stripped.indexOf('?');
        return shortsVideoPattern + stripped.mid(0, idx).toString();
    }
    return url.toString();
}

static bool isYoutuBeShortLink(const QUrl &url)
{
    const QString host = url.host().toLower();
    return (host == QLatin1String("youtu.be") || host == QLatin1String("www.youtu.be"));
}

void YaycUtilities::resolveAndNormalizeUrl(QUrl url)
{
    if (isYoutuBeShortLink(url)) {
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                             QNetworkRequest::AlwaysNetwork);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::UserVerifiedRedirectPolicy);
        QNetworkReply *reply = m_nam.head(std::move(request));
        connect(reply, &QNetworkReply::redirected, reply, &QNetworkReply::redirectAllowed);
        connect(reply, &QNetworkReply::finished, this, &YaycUtilities::onUrlResolveFinished);
        return;
    }
    emit videoUrlResolved(normalizeVideoUrl(url));
}

void YaycUtilities::onUrlResolveFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply) {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            emit videoUrlResolved(normalizeVideoUrl(reply->url()));
        } else {
            qWarning() << "youtu.be resolve failed:" << reply->errorString();
            emit videoUrlResolved({});
        }
    }
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
    // 443: a proxy may allow CONNECT only there (Squid's default)
    tcpSocket->connectToHost("github.com", 443);
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

void YaycUtilities::copyText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
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

void YaycUtilities::clearSettings(const QString &settingsUrl)
{
    if (!settingsUrl.isEmpty())
        settingsFileToDelete = QUrl(settingsUrl).toLocalFile();
    restartApp(YaycUtilities::EXIT_CODE_ERASE_SETTINGS);
}

void YaycUtilities::watchQuitSignals()
{
#ifdef Q_OS_UNIX
    if (s_quitOwner && s_quitOwner != this)
        return;
    if (s_quitSignalled)
        return; // keep the "second SIGTERM kills" fallback
    // Self-pipe: the handler only writes a byte, the event loop does the rest
    if (!s_quitNotifier) {
        // Not inherited by tar, dpkg, or the restarted app
#ifdef SOCK_CLOEXEC
        if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, s_quitFds) != 0)
            return;
#else
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, s_quitFds) != 0)
            return;
        ::fcntl(s_quitFds[0], F_SETFD, FD_CLOEXEC);
        ::fcntl(s_quitFds[1], F_SETFD, FD_CLOEXEC);
#endif
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
        const int on = 1;
        ::setsockopt(s_quitFds[0], SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
        s_quitNotifier = new QSocketNotifier(s_quitFds[1], QSocketNotifier::Read, this);
        connect(s_quitNotifier, &QSocketNotifier::activated, this, [this]() {
            char byte;
            [[maybe_unused]] const ssize_t n = ::read(s_quitFds[1], &byte, 1);
            if (s_quitRequested)
                return; // one quit per process
            s_quitRequested = true;
            emit quitRequested();
        });
        s_quitOwner = this;
    }
    s_quitWriteFd = s_quitFds[0];
    struct sigaction sa {};
    sa.sa_handler = onQuitSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_RESETHAND;
    // The first arm remembers what to restore (e.g. QtTest's own handler)
    if (!s_quitArmed) {
        if (::sigaction(SIGTERM, &sa, &s_previousQuitAction) == 0)
            s_quitArmed = true;
        return;
    }
    ::sigaction(SIGTERM, &sa, nullptr);
#endif
}

bool YaycUtilities::executableExists(const QString &exe) const
{
    return isExec(exe);
}

bool YaycUtilities::directoryExists(const QString &path)
{
    const QUrl url(path);
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : path;
    return QFileInfo(localPath).isDir();
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

void YaycUtilities::setNetworkProxy(const QString &proxyType, const QString &proxyHost, int proxyPort) {
    QNetworkProxy::ProxyType type =
        ((proxyType == QLatin1String("none")) ? QNetworkProxy::NoProxy :
             ((proxyType == QLatin1String("http")) ? QNetworkProxy::HttpProxy
                                                   : QNetworkProxy::Socks5Proxy));

    QNetworkProxy proxy;
    QUrl host = QUrl::fromUserInput(proxyHost);
    if (!host.isValid() || proxyPort < 1 || proxyPort > 65535)
        type = QNetworkProxy::NoProxy;
    proxy.setType(type);
    proxy.setHostName(host.host());
    proxy.setPort(proxyPort);
    QNetworkProxy::setApplicationProxy(proxy);
}

void YaycUtilities::setColorScheme(bool darkMode)
{
    QGuiApplication::styleHints()->setColorScheme(
        darkMode ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
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

void YaycUtilities::simulateClick(QQuickItem *item, double x, double y)
{
#if defined(Q_OS_LINUX)
    if (!item || !item->window())
        return;
    QPointF localPos(x, y);
    QPointF scenePos = item->mapToScene(localPos);
    QPointF globalPos = item->window()->mapToGlobal(scenePos.toPoint());

    QCoreApplication::postEvent(item->window(),
        new QMouseEvent(QEvent::MouseButtonPress, scenePos, globalPos,
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
    QCoreApplication::postEvent(item->window(),
        new QMouseEvent(QEvent::MouseButtonRelease, scenePos, globalPos,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier));
#else
    Q_UNUSED(item);
    Q_UNUSED(x);
    Q_UNUSED(y);
    // No reliable OS-level input injection without the trusted-mouse-injection
    // QtWebEngine patch (see WebView.qml's hasTrustedMouseInjection). Chromium's
    // renderer lives in a native child window/view on Windows/macOS that a
    // QMouseEvent posted to the QQuickWindow never reaches.
#endif
}

bool YaycUtilities::keepForegroundIllusion() const
{
    return m_keepForegroundIllusion;
}

void YaycUtilities::setKeepForegroundIllusion(bool enabled)
{
    if (m_keepForegroundIllusion == enabled)
        return;
    m_keepForegroundIllusion = enabled;
    emit keepForegroundIllusionChanged(enabled);
}

QPointF YaycUtilities::cursorPosIn(QQuickItem *item) const
{
    if (!item)
        return QPointF(-1, -1);
    const QPointF local = item->mapFromGlobal(QCursor::pos());
    return item->contains(local) ? local : QPointF(-1, -1);
}

bool YaycUtilities::freezeWebViewHover() const
{
    return m_freezeWebViewHover;
}

void YaycUtilities::setFreezeWebViewHover(bool enabled)
{
    if (m_freezeWebViewHover == enabled)
        return;
    m_freezeWebViewHover = enabled;
    emit freezeWebViewHoverChanged(enabled);
}

namespace {
// Render item class is private (QtWebEngineCore::RenderWidgetHostViewQtDelegateItem),
// so we cannot link it. Match by class name instead.
bool isWebEngineRenderItem(const QQuickItem *item)
{
    return QLatin1String(item->metaObject()->className())
            .contains(QLatin1String("RenderWidgetHostViewQtDelegateItem"));
}

// Hidden or unexposed window (minimize, other desktop, tray): QtWebEngine frees its
// last frame on sceneGraphAboutToStop, and nothing redraws it on return - a still page
// stays black until it repaints. Cut that link to keep the frame (sceneGraphInvalidated
// still frees it). Redone each time: the item reconnects on every scene change.
void keepWebEngineFrames(QQuickWindow *win)
{
    QList<QQuickItem *> stack{win->contentItem()};
    while (!stack.isEmpty()) {
        QQuickItem *item = stack.takeLast();
        if (isWebEngineRenderItem(item))
            QObject::disconnect(win, &QQuickWindow::sceneGraphAboutToStop, item, nullptr);
        stack.append(item->childItems());
    }
}
} // namespace

bool YaycUtilities::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type type = event->type();
    if (type == QEvent::Expose || type == QEvent::Hide) {
        if (auto *win = qobject_cast<QQuickWindow *>(watched))
            keepWebEngineFrames(win);
        return QObject::eventFilter(watched, event);
    }

    if (!m_keepForegroundIllusion && !m_freezeWebViewHover)
        return QObject::eventFilter(watched, event);

    const bool isHover = (type == QEvent::HoverLeave || type == QEvent::HoverMove
                          || type == QEvent::HoverEnter);
    if (!isHover && type != QEvent::MouseButtonPress && type != QEvent::FocusOut)
        return QObject::eventFilter(watched, event);

    auto *item = qobject_cast<QQuickItem *>(watched);
    if (!item || !isWebEngineRenderItem(item))
        return QObject::eventFilter(watched, event);

    if (type == QEvent::MouseButtonPress) {
        // Left button only. Right click opens the menu with Unpin/swap, so it must
        // not unpin by itself.
        auto *me = static_cast<QMouseEvent *>(event);
        if (m_freezeWebViewHover && me->button() == Qt::LeftButton)
            emit webViewPressedWhileFrozen();
        return QObject::eventFilter(watched, event); // never swallow the click
    }

    // In Chromium FocusOut becomes SetActive(false) + LostFocus() (handleFocusEvent in
    // render_widget_host_view_qt_delegate_client.cpp). Two bad results:
    //  - frames stop reaching the screen (picture freezes, audio keeps playing)
    //  - page gets 'blur', so YouTube closes the preview
    // So drop every reason, popup focus too. Using only ActiveWindowFocusReason was
    // tried: the blur came back. The context-menu stall (tasks/todo.md) is another bug.
    if (type == QEvent::FocusOut)
        return m_freezeWebViewHover;

    // Pinned: drop all hover events. Hover stays where the pin started.
    if (m_freezeWebViewHover)
        return true;

    if (m_keepForegroundIllusion && type == QEvent::HoverLeave) {
        // Other window has focus. So this leave comes from that, not from the user
        // moving off the thumbnail. Cursor position does not matter here.
        const QWindow *win = item->window();
        if (win && !win->isActive())
            return true;
        // Window is active, but leave arrived while cursor is still inside: WM bug.
        if (item->contains(item->mapFromGlobal(QCursor::pos())))
            return true;
    }
    return QObject::eventFilter(watched, event);
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

QString YaycUtilities::currentLanguage() const
{
    return m_currentLanguage;
}

void YaycUtilities::setLanguage(const QString &lang)
{
    qDebug() << "[i18n] setLanguage:" << lang << "(current:" << m_currentLanguage << ")";
    if (m_currentLanguage == lang)
        return;
    m_currentLanguage = lang;
    emit languageChanged(lang);
}

QStringList YaycUtilities::availableLanguages() const
{
    if (m_availableLanguages.isEmpty()) {
        QDir dir(":/assets/i18n/");
        for (const QString &f : dir.entryList({"*.json"}, QDir::Files))
            m_availableLanguages << f.left(f.length() - 5);
        m_availableLanguages.sort();
    }
    return m_availableLanguages;
}

QString YaycUtilities::languageDisplayName(const QString &lang) const
{
    QFile file(":/assets/i18n/" + lang + ".json");
    if (!file.open(QIODevice::ReadOnly))
        return lang;
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    return obj.value("lang.display_name").toString(lang);
}
