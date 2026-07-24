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

#ifndef YAYCUTILITIES_H
#define YAYCUTILITIES_H

#include <QObject>
#include <QUrl>
#include <QString>
#include <QStringList>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QDateTime>

class QQuickItem;

class YaycUtilities : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString currentLanguage READ currentLanguage WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QStringList availableLanguages READ availableLanguages CONSTANT)
    Q_PROPERTY(bool keepForegroundIllusion READ keepForegroundIllusion
               WRITE setKeepForegroundIllusion NOTIFY keepForegroundIllusionChanged)

public:
    // Exit codes
    static constexpr int EXIT_CODE_REBOOT = -123456789;
    static constexpr int EXIT_CODE_ERASE_SETTINGS = -123456788;

    // Global state (initialized in main.cpp)
    static bool isPlasma;
    static QDateTime appstartTS;
    static QString settingsFileToDelete;

    explicit YaycUtilities(QObject *parent = nullptr);
    ~YaycUtilities() override;

    // When true, swallows QEvent::HoverLeave events destined for QtWebEngine's
    // internal render-widget item whenever the real cursor is still physically
    // over it. Works around window managers/compositors that deliver a
    // QEvent::Leave to the window on focus loss or virtual-desktop switch
    // (independent of the cursor moving) - Qt Quick turns that into a
    // HoverLeave for the hovered item, which QtWebEngine forwards into
    // Chromium as a real DOM mouseleave, pausing hover-preview media even
    // though the pointer never moved.
    bool keepForegroundIllusion() const;
    void setKeepForegroundIllusion(bool enabled);
    bool eventFilter(QObject *watched, QEvent *event) override;

    Q_INVOKABLE QUrl urlWithPosition(const QString &url, const int position) const;
    Q_INVOKABLE void yDebug(const QString &s);
    // Q_INVOKABLE void addRequestInterceptor(QObject *webEngineView);

    Q_INVOKABLE static bool isYoutubeVideoUrl(QUrl url);
    Q_INVOKABLE static bool isYoutubeStandardUrl(QUrl url);
    static bool isYoutubeStandardUrl(const QString &url);
    Q_INVOKABLE static bool isYoutubeChannelPage(QUrl url);
    static bool isYoutubeChannelPage(const QString &url);
    Q_INVOKABLE static bool isYoutubeHomepage(QUrl url);
    static bool isYoutubeHomepage(const QString &url);
    Q_INVOKABLE static bool isYoutubeShortsUrl(QUrl url);
    static bool isYoutubeShortsUrl(const QString &url);

    Q_INVOKABLE QString getVideoID(QUrl url) const;
    Q_INVOKABLE QString getVideoID(const QString &key, const QString &sVendor, bool isShorts) const;
    Q_INVOKABLE QString normalizeVideoUrl(QUrl url) const;
    Q_INVOKABLE void resolveAndNormalizeUrl(QUrl url);

    Q_INVOKABLE QString getChangelog();
    Q_INVOKABLE QString getDisclaimer();

    Q_INVOKABLE void checkConnectivity();
    Q_INVOKABLE void getLatestVersion();
    Q_INVOKABLE void getDonateEtag();
    Q_INVOKABLE void getDonateURL();

    Q_INVOKABLE void printSettingsPath();
    Q_INVOKABLE void restartApp(int code);
    Q_INVOKABLE void restartApp();
    Q_INVOKABLE void clearSettings(const QString &settingsUrl);

    Q_INVOKABLE bool executableExists(const QString &exe) const;
    Q_INVOKABLE static bool directoryExists(const QString &path);
    Q_INVOKABLE void fetchMissingThumbnails();

    Q_INVOKABLE int compareSemver(const QString &version1, const QString &version2);
    Q_INVOKABLE static void setNetworkProxy(const QString &proxyType,
                                            const QString &proxyHost,
                                            int proxyPort);
    Q_INVOKABLE static void setColorScheme(bool darkMode);

    // Fallback for QtWebEngine builds without the trusted-mouse-injection
    // patch (see WebView.qml's hasTrustedMouseInjection). Only works on
    // Linux: Chromium's renderer lives in a native child window/view on
    // Windows and macOS that a QMouseEvent posted to the QQuickWindow never
    // reaches. No-op on other platforms.
    Q_INVOKABLE void simulateClick(QQuickItem *item, double x, double y);

    Q_INVOKABLE QString languageDisplayName(const QString &lang) const;
    QString currentLanguage() const;
    QStringList availableLanguages() const;
    void setLanguage(const QString &lang);

    static bool isShortVideo(const QString &fkey);
    static void openInBrowser(const QString &key, const QString &extWorkingDirRoot);

signals:
    void languageChanged(const QString &lang);
    void youtubeUrlRequested(const QUrl &url);
    void networkFound();
    void latestVersion(const QString &);
    void donateETag(const QString &);
    void donateUrl(const QString &);
    void videoUrlResolved(const QString &normalizedUrl);
    void keepForegroundIllusionChanged(bool enabled);

public slots:
    void onSocketConnected();
    void onSocketError();
    void onReplyFinished();
    void onDonateEtagReplyFinished();
    void onDonateReplyFinished();
    void onUrlResolveFinished();

protected:
    QTcpSocket *tcpSocket;
    QNetworkAccessManager m_nam;

private:
    QString m_currentLanguage{"en"};
    mutable QStringList m_availableLanguages;
    bool m_keepForegroundIllusion{false};
};

#endif // YAYCUTILITIES_H
