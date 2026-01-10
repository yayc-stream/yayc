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

#ifndef FILESYSTEMMODEL_H
#define FILESYSTEMMODEL_H

#include "Platform.h"
#include "VideoMetadata.h"
#include "ChannelMetadata.h"
#include "EmptyIconProvider.h"
#include "NoDirSortProxyModel.h"

#include <QFileSystemModel>
#include <QHash>
#include <QDir>
#include <QScopedPointer>

class ThumbnailFetcher;

// Helper functions
QString sizeString(const QFileInfo &fi);
QString permissionString(const QFileInfo &fi);
QFileInfoList findFile(const QString &fileName, const QDir &d);
QFileInfoList findFiles(const QDir &d, const QString &ext);
QHash<QString, VideoMetadata> cacheRoot(const QDir &d);
QHash<QString, ChannelMetadata> cacheChannels(QDir d);

class FileSystemModel : public QFileSystemModel {
    Q_OBJECT

    bool m_ready{false};
    bool m_bookmarksModel{false};
    QHash<QString, VideoMetadata> m_cache;
    QHash<QString, ChannelMetadata> m_channelCache;
    QModelIndex m_rootPathIndex;
    QScopedPointer<NoDirSortProxyModel> m_proxyModel;
    QString m_contextPropertyName;
    QModelIndex m_nullIndex;

    EmptyIconProvider m_emptyIconProvider;
    QDir m_root;

    Q_PROPERTY(QVariant sortFilterProxyModel READ sortFilterProxyModel NOTIFY sortFilterProxyModelChanged)
    Q_PROPERTY(QVariant rootPathIndex READ rootPathIndex NOTIFY rootPathIndexChanged)
    Q_PROPERTY(QVariant nullIndex MEMBER m_nullIndex CONSTANT)

public:
    QVariant rootPathIndex() const;
    QVariant sortFilterProxyModel() const;

    explicit FileSystemModel(QString contextPropertyName,
                             bool bookmarks,
                             QObject *parent = nullptr);
    ~FileSystemModel() override;

    inline bool ready() const { return m_ready; }

    enum Roles {
        SizeRole = Qt::UserRole + 4,
        DisplayableFilePermissionsRole = Qt::UserRole + 5,
        LastModifiedRole = Qt::UserRole + 6,
        UrlStringRole = Qt::UserRole + 7,
        ContentNameRole = Qt::UserRole + 8,
        TitleRole = Qt::UserRole + 9,
        ChannelNameRole = Qt::UserRole + 10,
        ChannelIdRole = Qt::UserRole + 11,
        CreatedRole = Qt::UserRole + 12,
        KeyRole = Qt::UserRole + 13,
    };
    Q_ENUM(Roles)

    Q_INVOKABLE QModelIndex setRoot(QString newPath, FileSystemModel *oldModel = nullptr);
    Q_INVOKABLE QString key(const QModelIndex &item) const;
    Q_INVOKABLE QString title(const QModelIndex &item) const;
    Q_INVOKABLE QString title(const QString &key) const;
    Q_INVOKABLE bool isVideoBookmarked(const QString &key);
    Q_INVOKABLE QString creationDate(const QString &key) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

public slots:
    QString keyFromViewItem(const QModelIndex &item) const;
    QVariant videoUrl(QModelIndex item);
    QVariant videoUrl(const QString &key);
    void openInBrowser(QModelIndex item, const QString &extWorkingDirRoot);
    void openInBrowser(const QString &key, const QString &extWorkingDirRoot);
    void openInExternalApp(QModelIndex item, const QString &extCommand, const QString &extWorkingDirRoot);
    void openInExternalApp(const QString &key, const QString &extCommand, const QString &extWorkingDirRoot);
    bool deleteEntry(QModelIndex item, const QString &extWorkingDirRoot, bool deleteStorage_);
    bool deleteEntry(const QString &key
                     ,const QString &extWorkingDirRoot = "" // only for videos, not categories
                     ,bool deleteStorage_ = false); // same
    void deleteStorage(QModelIndex item, const QString &extWorkingDirRoot);
    void deleteStorage(const QString &key, const QString &extWorkingDirRoot);
    void sync();
    qreal progress(const QString &key) const;
    qreal duration(const QModelIndex &item) const;
    qreal duration(const QString &key) const;
    bool isShortVideo(const QModelIndex &item) const;
    bool isShortVideo(const QString &key) const;
    bool isViewed(const QModelIndex &item) const;
    bool isViewed(const QString &key) const;
    void viewEntry(const QModelIndex &item, bool viewed);
    void viewEntry(const QString &key, bool viewed);
    bool isStarred(const QModelIndex &item) const;
    bool isStarred(const QString &key) const;
    bool hasWorkingDir(const QModelIndex &item, const QString &extWorkingDirRoot) const;
    int hasWorkingDir(const QString &key, const QString &extWorkingDirRoot) const;
    bool hasSummary(const QString &key, const QString &extWorkingDirRoot) const;
    void starEntry(const QModelIndex &item, bool starred);
    void starEntry(const QString &key, bool starred);
    QString videoIconUrl(const QModelIndex &item) const;
    QString videoIconUrl(const QString &key) const;
    bool moveVideo(const QString &key, QModelIndex destinationDir);
    bool moveEntry(QModelIndex item, QModelIndex destinationDir);
    void moveEntry(const QString &key, const QDir &d);
    bool addCategory(const QString &name);
    bool updateEntry(const QString &key, const QString &title, const QString &channelURL,
                     const QString &channelAvatarURL, const QString &channelName,
                     const qreal duration = 0., const qreal position = 0.);
    void updateChannelID(const QString &key, const QString &channelID);
    void updateTitle(const QString &key, const QString &title);
    void updateChannelAvatar(const QString &channelKey, const QByteArray avatar);
    bool addEntry(const QString &key, const QString &title, const QString &channelURL,
                  const QString &channelAvatarURL, const QString &channelName,
                  const qreal duration = 0., const qreal position = 0.);

signals:
    void filesAdded(const QVariantList &addedPaths);
    void rootPathIndexChanged();
    void sortFilterProxyModelChanged();
    void searchTermChanged();
    void searchInTitlesChanged();
    void searchInChannelNamesChanged();
    void firstInitializationCompleted(const QString &rootPath);

private:
    void addThumbnail(const QString &key, const QByteArray &thumbnailData);
    void updateChannel(const QString &key, const QString &channelId, const QString &channelName);
    void addChannel(const QString &channelId, const Platform::Vendor vendor,
                    const QString &channelName, const QString &channelAvatarURL);
    QString itemKey(const QModelIndex &index) const;
    void fetchThumbnail(const QString &key);

    friend class ThumbnailFetcher;
};

#endif // FILESYSTEMMODEL_H
