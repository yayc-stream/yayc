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

#ifndef YAYCUTILITIES_H
#define YAYCUTILITIES_H

#include <QObject>
#include <QUrl>
#include <QString>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QDateTime>

class YaycUtilities : public QObject {
    Q_OBJECT
public:
    // Exit codes
    static constexpr int EXIT_CODE_REBOOT = -123456789;
    static constexpr int EXIT_CODE_ERASE_SETTINGS = -123456788;

    // Global state (initialized in main.cpp)
    static bool isPlasma;
    static QDateTime appstartTS;

    explicit YaycUtilities(QObject *parent = nullptr);
    ~YaycUtilities() override;

    Q_INVOKABLE QUrl urlWithPosition(const QString &url, const int position) const;
    Q_INVOKABLE void yDebug(const QString &s);
    Q_INVOKABLE void addRequestInterceptor(QObject *webEngineView);

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

    Q_INVOKABLE QString getChangelog();
    Q_INVOKABLE QString getDisclaimer();

    Q_INVOKABLE void checkConnectivity();
    Q_INVOKABLE void getLatestVersion();
    Q_INVOKABLE void getDonateEtag();
    Q_INVOKABLE void getDonateURL();

    Q_INVOKABLE void printSettingsPath();
    Q_INVOKABLE void restartApp(int code);
    Q_INVOKABLE void restartApp();
    Q_INVOKABLE void clearSettings();

    Q_INVOKABLE bool executableExists(const QString &exe) const;
    Q_INVOKABLE void fetchMissingThumbnails();

    Q_INVOKABLE int compareSemver(const QString &version1, const QString &version2);

    static bool isShortVideo(const QString &fkey);
    static void openInBrowser(const QString &key, const QString &extWorkingDirRoot);

signals:
    void youtubeUrlRequested(const QUrl &url);
    void networkFound();
    void latestVersion(const QString &);
    void donateETag(const QString &);
    void donateUrl(const QString &);

public slots:
    void onSocketConnected();
    void onSocketError();
    void onReplyFinished();
    void onDonateEtagReplyFinished();
    void onDonateReplyFinished();

protected:
    QTcpSocket *tcpSocket;
    QNetworkAccessManager m_nam;
};

#endif // YAYCUTILITIES_H
