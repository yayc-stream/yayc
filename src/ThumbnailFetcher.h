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

#ifndef THUMBNAILFETCHER_H
#define THUMBNAILFETCHER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QSet>

class FileSystemModel;

class ThumbnailFetcher : public QObject
{
    Q_OBJECT
public:
    virtual ~ThumbnailFetcher() override {}

    static ThumbnailFetcher &GetInstance();
    static void registerModel(FileSystemModel &model);
    static void unregisterModel(FileSystemModel &model);
    static void fetch(const QString &key);
    static void fetchChannel(const QString &key);
    static void fetchChannelAvatar(const QString &channelKey, const QString &url);
    static void fetchMissing();
    static void printStats();

private slots:
    void onThumbnailRequestFinished();
    void onVideoPageRequestFinished();
    void onFetchAvatarRequestFinished();
    void fetchMissingThumbnails();

private:
    explicit ThumbnailFetcher(QObject *parent = nullptr);
    void fetchThumbnail(const QString &key);
    void fetchChannelInternal(const QString &key);
    void fetchChannelAvatarInternal(const QString &channelKey, QString url);
    FileSystemModel *bookmarksModel();

private:
    QNetworkAccessManager m_nam;
    QSet<FileSystemModel *> m_models;
    int m_failures = 0;
    int m_channelIdFailures = 0;
};

#endif // THUMBNAILFETCHER_H
