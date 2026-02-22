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

#include "FileSystemModel.h"
#include "ThumbnailFetcher.h"
#include "ThumbnailImageProvider.h"
#include "YaycUtilities.h"

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QProcess>
#include <QLoggingCategory>
#include <QDirIterator>
#include <QIdentityProxyModel>

// Helper functions
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

QFileInfoList findFile(const QString &fileName, const QDir &d)
{
    QFileInfoList hitList;
    QDirIterator it(d, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QString filename = it.next();
        QFileInfo file(filename);

        if (file.isDir())
            continue;

        if (file.fileName() == fileName) {
            hitList.append(file);
        }
    }
    return hitList;
}

QFileInfoList findFiles(const QDir &d, const QString &ext)
{
    const QString nameEnd("." + ext);
    QFileInfoList hitList;
    QDirIterator it(d, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QString filename = it.next();
        QFileInfo file(filename);

        if (file.isDir())
            continue;

        if (file.fileName().endsWith(nameEnd)) {
            hitList.append(file);
        }
    }
    return hitList;
}

QHash<QString, VideoMetadata> cacheRoot(const QDir &d)
{
    QHash<QString, VideoMetadata> res;
    const auto &files = findFiles(d, videoExtension);
    for (const auto &f : files) {
        const QString &key = f.baseName();
        const QString &vtype = videoType(key);
        if (!((vtype == QLatin1String("s_") || vtype == QLatin1String("v_")) &&
              f.fileName().endsWith(videoExtension))) {
            continue;
        }
        const QDir parent = f.dir();
        res.insert(key, VideoMetadata(key, parent));
        res[key].loadFile();
    }
    return res;
}

QHash<QString, ChannelMetadata> cacheChannels(QDir d)
{
    QHash<QString, ChannelMetadata> res;
    if (!d.cd(".channels")) {
        qWarning() << "Failed cd into .channels!";
        return res;
    }
    const auto &files = findFiles(d, channelExtension);
    for (const auto &f : files) {
        const QString &key = f.fileName().chopped(channelExtension.length() + 1);
        if (!f.fileName().endsWith(channelExtension)) {
            continue;
        }
        res.insert(key, ChannelMetadata(key, d));
        res[key].loadFile();
    }
    return res;
}

// FileSystemModel implementation
QVariant FileSystemModel::rootPathIndex() const {
    return QVariant::fromValue(m_rootPathIndex);
}

QVariant FileSystemModel::sortFilterProxyModel() const {
    return QVariant::fromValue(m_proxyModel.get());
}

FileSystemModel::FileSystemModel(QString contextPropertyName,
                                 bool bookmarks,
                                 QObject *parent)
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
    setNameFilterDisables(false);
    setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);
    sort(3);
    QScopedPointer<NoDirSortProxyModel> pm(new NoDirSortProxyModel);
    auto pmName = m_contextPropertyName + "_ProxyModel";
    pm->setObjectName(pmName.toStdString().c_str());
    m_proxyModel.swap(pm);
    ThumbnailFetcher::registerModel(*this);
}

FileSystemModel::~FileSystemModel() {
    ThumbnailFetcher::unregisterModel(*this);
}

QModelIndex FileSystemModel::setRoot(QString newPath, FileSystemModel *oldModel) {
    if (newPath.startsWith("file://")) {
        newPath = newPath.mid(7);
#if defined(Q_OS_WINDOWS)
        newPath = newPath.mid(1);
#endif
    }

    QQmlApplicationEngine *engine = qobject_cast<QQmlApplicationEngine *>(parent());
    if (!engine) {
        qFatal("Unable to retrieve QQmlApplicationEngine");
    }
    if (!m_proxyModel) {
        qFatal("NULL sortfilter proxy model");
    }
    if (!rootPath().isEmpty() && (rootPath() != ".")) { // if this is the current fsmodel
        FileSystemModel *fsmodel = new FileSystemModel(m_contextPropertyName,
                                                       m_bookmarksModel,
                                                       engine);
        // Do not delete this later here, make it delete by the nested call, after
        // the context property has been updated with the new model object
        return fsmodel->setRoot(newPath, this);
    }

    setIconProvider(&m_emptyIconProvider);
    if (newPath.isEmpty()) { // clear the model
        m_ready = true;
        // TODO: deduplicate, through an object destructor?
        engine->rootContext()->setContextProperty(m_contextPropertyName, this);
        if (oldModel)
            oldModel->deleteLater();
        emit sortFilterProxyModelChanged();
        emit rootPathIndexChanged();
        return {};
    }
    m_root = QDir(newPath);
    if (!m_root.exists()) {
        qFatal("Trying to set root directory to non-existent %s\n", newPath.toStdString().c_str());
        return {};
    }

    m_cache = cacheRoot(m_root);
    if (m_bookmarksModel) {
        m_root.mkdir(".channels");
        m_channelCache = cacheChannels(m_root);
    }

    ThumbnailImageProvider *provider =
        static_cast<ThumbnailImageProvider *>(engine->imageProvider(QLatin1String("videothumbnail")));
    if (!provider) {
        qFatal("Unable to retrieve ThumbnailImageProvider");
    }
    for (const auto &e : std::as_const(m_cache)) {
        if (e.hasThumbnail())
            provider->insert(e.key, e.thumbnailData);
    }
    setResolveSymlinks(true);

    auto res = this->QFileSystemModel::setRootPath(newPath);
    if (res.isValid()) {
        m_proxyModel->setSourceModel(this);
        m_proxyModel->setDynamicSortFilter(true);
//        m_proxyModel->setSortRole(LastModifiedRole);
//        m_proxyModel->sort(3);
        m_proxyModel->setSortRole(CreatedRole);
        m_proxyModel->sort(0);


        m_rootPathIndex = m_proxyModel->mapFromSource(res);
        if (!m_rootPathIndex.isValid()) {
            qFatal("Failure mapping FileSystemModel root path index to proxy model");
        }
        engine->rootContext()->setContextProperty(m_contextPropertyName, this);
        if (oldModel)
            oldModel->deleteLater();
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

QString FileSystemModel::key(const QModelIndex &item) const {
    if (!m_ready)
        return QString();
    auto index = m_proxyModel->mapToSource(item);
    if (!index.isValid()) {
        qWarning() << "Failure mapping ProxyModel "<< item << " to fsmodel";
    }
    if (isDir(index)) {
        return QString();
    }
    const QString &key = itemKey(index);
    return key;
}

QString FileSystemModel::title(const QModelIndex &item) const {
    if (!m_ready)
        return QString();
    const QString &key = keyFromViewItem(item);
    if (!key.size() || !m_cache.contains(key))
        return QString();
    return title(key);
}

QString FileSystemModel::title(const QString &key) const {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return QString();
    return m_cache.value(key).title;
}

bool FileSystemModel::isVideoBookmarked(const QString &key) {
    if (!m_ready || !key.size())
        return false;
    return m_cache.contains(key);
}

QString FileSystemModel::creationDate(const QString &key) const {
    if (!m_ready)
        return QString();
    if (!key.size() || !m_cache.contains(key))
        return QString();
    return m_cache.value(key).creationDate.toString(QStringLiteral("yyyy.MM.dd hh:mm"));
}

QVariant FileSystemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        qWarning() << "FileSystemModel::data for role "<<role<< " : invalid index "<<index;
        return QVariant();
    }
    if (index.isValid() && role >= Qt::UserRole) {
        switch (role) {
        case SizeRole:
            return QVariant(sizeString(fileInfo(index)));
        case DisplayableFilePermissionsRole:
            return QVariant(permissionString(fileInfo(index)));
        case LastModifiedRole:
            return QVariant(fileInfo(index).lastModified().toString(QStringLiteral("yyyyMMddhhmmss")));
        case CreatedRole: {
            if (isDir(index))
                return QVariant(fileInfo(index).birthTime().toString(QStringLiteral("yyyyMMddhhmmss")));
            const QString &key = itemKey(index);
            if (!m_cache.contains(key)) {
                qWarning() << "key not found " << key << " " << fileInfo(index).baseName();
                return {};
            }
            return m_cache.value(key).creationDate.toString(QStringLiteral("yyyy.MM.dd hh:mm"));
        }
        case UrlStringRole: {
            if (isDir(index))
                return {};
            const QString &key = itemKey(index);
            if (!m_cache.contains(key)) {
                qWarning() << "key not found " << key << " " << fileInfo(index).baseName();
                return {};
            }
            return m_cache.value(key).url();
        }
        case KeyRole: {
            if (!isDir(index)) {
                const QString &key = itemKey(index);
                if (!m_cache.contains(key)) {
                    return QFileSystemModel::data(index, role);
                }
                return key;
            }
            return {};
        }
        case IsDirRole:
            return isDir(index);
        case ContentNameRole:
        case QFileSystemModel::FileNameRole:
        case Qt::DisplayRole: {
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
            case 3:
                return QVariant(fileInfo(index).lastModified().toString(QStringLiteral("yyyy.MM.dd hh:mm:ss")));
            default:
                return QFileSystemModel::data(index, role);
            }
        }
        case TitleRole: {
            if (!isDir(index)) {
                const QString &key = itemKey(index);
                if (!m_cache.contains(key))
                    return {};
                return m_cache.value(key).title;
            } else {
                return QFileSystemModel::data(index, role);
            }
        }
        case ChannelNameRole: {
            if (!isDir(index)) {
                const QString &key = itemKey(index);
                if (!m_cache.contains(key))
                    return {};
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
                if (!m_cache.contains(key))
                    return {};
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

QHash<int, QByteArray> FileSystemModel::roleNames() const
{
    QHash<int, QByteArray> result = QFileSystemModel::roleNames();
    result.insert(SizeRole, QByteArrayLiteral("size"));
    result.insert(DisplayableFilePermissionsRole, QByteArrayLiteral("displayableFilePermissions"));
    result.insert(LastModifiedRole, QByteArrayLiteral("lastModified"));
    result.insert(CreatedRole, QByteArrayLiteral("created"));
    result.insert(ContentNameRole, QByteArrayLiteral("contentName"));
    result.insert(KeyRole, QByteArrayLiteral("key"));
    result.insert(IsDirRole, QByteArrayLiteral("isDirectory"));
    return result;
}

QString FileSystemModel::keyFromViewItem(const QModelIndex &item) const {
    //qDebug() << "item model:" << item.model() << "proxy model:" << m_proxyModel.get();
    return key(item);
}

QVariant FileSystemModel::videoUrl(QModelIndex item) {
    if (!m_ready)
        return QString();
    const QString &key = keyFromViewItem(item);
    if (!key.size() || !m_cache.contains(key))
        return QString();
    return videoUrl(key);
}

QVariant FileSystemModel::videoUrl(const QString &key) {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return QString();
    return m_cache.value(key).url();
}

void FileSystemModel::openInBrowser(QModelIndex item, const QString &extWorkingDirRoot) {
    if (!m_ready)
        return;
    const QString &key = keyFromViewItem(item);
    if (!key.size() || !m_cache.contains(key))
        return;
    return openInBrowser(key, extWorkingDirRoot);
}

void FileSystemModel::openInBrowser(const QString &key, const QString &extWorkingDirRoot) {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return;
    return YaycUtilities::openInBrowser(key, extWorkingDirRoot);
}

void FileSystemModel::openInExternalApp(QModelIndex item,
                                        const QString &extCommand,
                                        const QString &extWorkingDirRoot) {
    if (!m_ready)
        return;
    const QString &key = keyFromViewItem(item);
    if (!key.size() || !m_cache.contains(key))
        return;
    return openInExternalApp(key, extCommand, extWorkingDirRoot);
}

void FileSystemModel::openInExternalApp(const QString &key,
                                        const QString &extCommand,
                                        const QString &extWorkingDirRoot) {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return;

    QDir d(extWorkingDirRoot);

    if (!d.exists()) {
        QLoggingCategory category("qmldebug");
        qCInfo(category) << "openInExternalApp: not existing working dir " << extWorkingDirRoot;
        return;
    }

    if (!d.exists(key)) {
        if (!d.mkdir(key)) {
            QLoggingCategory category("qmldebug");
            qCInfo(category) << "openInExternalApp: failed creating " << d.filePath(key);
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

void FileSystemModel::enqueueExternalApp(const QString &key,
                                         const QString &extCommand,
                                         const QString &extWorkingDirRoot) {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return;
    m_extAppQueue.enqueue({key, extCommand, extWorkingDirRoot});
    m_extAppTotal = m_extAppQueue.size() + m_extAppCompleted;
    emit extAppProgressChanged();
    if (!m_extAppRunning)
        processNextExtAppRequest();
}

void FileSystemModel::enqueueCategoryExternalApp(
        QModelIndex categoryItem,
        const QString &extCommand,
        const QString &extWorkingDirRoot) {
    if (!m_ready)
        return;
    auto index = m_proxyModel->mapToSource(categoryItem);
    if (!index.isValid())
        return;
    QDir dir(filePath(index));
    if (!dir.exists())
        return;

    const auto files = findFiles(dir, videoExtension);
    for (const auto &f : files) {
        const QString &key = f.baseName();
        if (m_cache.contains(key))
            m_extAppQueue.enqueue({key, extCommand, extWorkingDirRoot});
    }
    m_extAppTotal = m_extAppQueue.size();
    m_extAppCompleted = 0;
    emit extAppProgressChanged();
    if (!m_extAppRunning)
        processNextExtAppRequest();
}

void FileSystemModel::processNextExtAppRequest() {
    if (m_extAppQueue.isEmpty()) {
        m_extAppRunning = false;
        emit extAppProgressChanged();
        return;
    }
    m_extAppRunning = true;
    auto job = m_extAppQueue.dequeue();

    QDir d(job.workingDir);
    if (!d.exists()) return processNextExtAppRequest();
    if (!d.exists(job.key) && !d.mkdir(job.key)) return processNextExtAppRequest();

    QString url = m_cache.value(job.key).url(false).toString();

    if (!m_extAppProcess) {
        m_extAppProcess = new QProcess(this);
        connect(m_extAppProcess, &QProcess::finished,
                this, &FileSystemModel::onExtAppFinished);
    }
    m_extAppProcess->setWorkingDirectory(d.filePath(job.key));
    m_extAppProcess->setStandardOutputFile(QProcess::nullDevice());
    m_extAppProcess->setStandardErrorFile(QProcess::nullDevice());
    m_extAppProcess->start(job.command, {url});

    auto idx = index(m_cache.value(job.key).filePath());
    emit dataChanged(idx, idx);
}

void FileSystemModel::onExtAppFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(exitCode)
    Q_UNUSED(status)
    m_extAppCompleted++;
    emit extAppProgressChanged();
    processNextExtAppRequest();
}

bool FileSystemModel::deleteEntry(QModelIndex item,
                                  const QString &extWorkingDirRoot,
                                  bool deleteStorage_) {
    if (!m_ready)
        return false;
    auto index = m_proxyModel->mapToSource(item);
    if (!index.isValid()) {
        qWarning() << "Failure mapping ProxyModel "<< item << " to fsmodel";
    }

    if (!filePath(index).size()) {
        qWarning() << "invalid input";
        return false;
    }
    bool res = false;
    if (isDir(index)) {
        QDir d(filePath(index));
        if (d.isEmpty()) {
            res = remove(index); // it does removeRecursively internally
        } else {
            qWarning() << "Category not empty! " << fileInfo(index).baseName();
            res = false;
        }
        return res;
    } else {
        const QString &key = itemKey(index);
        return deleteEntry(key, extWorkingDirRoot, deleteStorage_);
    }
}

bool FileSystemModel::deleteEntry(const QString &key,
                                  const QString &extWorkingDirRoot,
                                  bool deleteStorage_) {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return false;

    if (deleteStorage_)
        deleteStorage(key, extWorkingDirRoot);

    auto entry = m_cache.take(key);
    return entry.eraseFile();
}

void FileSystemModel::deleteStorage(QModelIndex item,
                                    const QString &extWorkingDirRoot) {
    if (!m_ready)
        return;
    const QString &key = keyFromViewItem(item);
    if (!key.size() || !m_cache.contains(key))
        return;
    deleteStorage(key, extWorkingDirRoot);
}

void FileSystemModel::deleteStorage(const QString &key,
                                    const QString &extWorkingDirRoot) {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return;

    if (!extWorkingDirRoot.isEmpty()) {
        QDir d(extWorkingDirRoot);
        if (d.exists() && d.exists(key)) {
            QDir(d.filePath(key)).removeRecursively();
        }
    }
}

void FileSystemModel::sync() {
    for (auto &e : m_cache) {
        e.saveFile();
    }
    for (auto &c : m_channelCache) {
        c.saveFile();
    }
}

qreal FileSystemModel::progress(const QString &key) const {
    if (!m_ready)
        return 0;
    if (!key.size() || !m_cache.contains(key)) {
        qWarning() << "FileSystemModel::progress: Key " << key << " not present!";
        return 0;
    }

    const auto &position = m_cache.value(key).position;
    const auto &duration = m_cache.value(key).duration;
    if (duration == 0.)
        return 0.;
    return position / duration;
}

qreal FileSystemModel::duration(const QModelIndex &item) const {
    if (!m_ready)
        return 0;
    const QString &key = keyFromViewItem(item);
    return duration(key);
}

qreal FileSystemModel::duration(const QString &key) const {
    if (!m_ready || !key.size())
        return 0.;
    if (!m_cache.contains(key)) {
        qWarning() << "FileSystemModel::duration: Key " << key << " not present!";
        return 0;
    }
    return m_cache.value(key).duration;
}

bool FileSystemModel::isShortVideo(const QModelIndex &item) const {
    if (!m_ready)
        return false;
    const QString &key = keyFromViewItem(item);
    return isShortVideo(key);
}

bool FileSystemModel::isShortVideo(const QString &key) const {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return false;
    return YaycUtilities::isShortVideo(key);
}

bool FileSystemModel::isViewed(const QModelIndex &item) const {
    if (!m_ready)
        return false;
    const QString &key = keyFromViewItem(item);
    return isViewed(key);
}

bool FileSystemModel::isViewed(const QString &key) const {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return false;
    return m_cache.value(key).viewed;
}

void FileSystemModel::viewEntry(const QModelIndex &item, bool viewed) {
    if (!m_ready)
        return;
    const bool currentValue = isViewed(item);
    if (viewed == currentValue)
        return;
    const QString &key = keyFromViewItem(item);
    viewEntry(key, viewed);
}

void FileSystemModel::viewEntry(const QString &key, bool viewed) {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return;
    m_cache[key].setViewed(viewed);
    auto idx = index(m_cache[key].filePath());
    emit dataChanged(idx, idx);
}

bool FileSystemModel::isStarred(const QModelIndex &item) const {
    if (!m_ready)
        return false;
    const QString &key = keyFromViewItem(item);
    return isStarred(key);
}

bool FileSystemModel::isStarred(const QString &key) const {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return false;
    return m_cache.value(key).starred;
}

bool FileSystemModel::hasWorkingDir(const QModelIndex &item, const QString &extWorkingDirRoot) const {
    if (!m_ready)
        return false;
    const QString &key = keyFromViewItem(item);
    return hasWorkingDir(key, extWorkingDirRoot);
}

int FileSystemModel::hasWorkingDir(const QString &key, const QString &extWorkingDirRoot) const {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return false;
    QDir d(extWorkingDirRoot);
    const bool exists = d.exists() && d.exists(key);
    if (!exists || !d.cd(key))
        return 0;
    return 1 + int(!d.isEmpty());
}

bool FileSystemModel::hasSummary(const QString &key, const QString &extWorkingDirRoot) const {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return false;

    if (!hasWorkingDir(key, extWorkingDirRoot))
        return false;

    QDirIterator it(extWorkingDirRoot + "/" + key, QStringList() << "*summary*",
                    QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext();
}

void FileSystemModel::starEntry(const QModelIndex &item, bool starred) {
    if (!m_ready)
        return;
    const bool currentValue = isStarred(item);
    if (starred == currentValue)
        return;
    const QString &key = keyFromViewItem(item);
    starEntry(key, starred);
}

void FileSystemModel::starEntry(const QString &key, bool starred) {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return;
    m_cache[key].setStarred(starred);
    auto idx = index(m_cache[key].filePath());
    emit dataChanged(idx, idx);
}

QString FileSystemModel::videoIconUrl(const QModelIndex &item) const {
    if (!m_ready)
        return QString();
    const QString &key = keyFromViewItem(item);
    return videoIconUrl(key);
}

QString FileSystemModel::videoIconUrl(const QString &key) const {
    if (!m_ready || !key.size() || !m_cache.contains(key))
        return QString();
    const bool shortVideo = isShortVideo(key);
    const bool viewed = isViewed(key);
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

bool FileSystemModel::moveVideo(const QString &key, QModelIndex destinationDir) {
    if (!m_ready)
        return false;
    auto index = m_proxyModel->mapToSource(destinationDir);
    if (!index.isValid()) {
        qWarning() << "Failure mapping ProxyModel "<< destinationDir << " to fsmodel";
        return false;
    }
    if (!m_cache.contains(key) || !m_cache[key].filePath().size() ||
        !filePath(index).size() || !isDir(index)) {
        qWarning() << "Invalid input trying to move " << key << " into " << filePath(index);
        return false;
    }
    QDir d(filePath(index));
    if (!d.exists()) {
        qWarning() << "Destination directory doesn't exist";
        return false;
    }

    if (m_lastDestination != d.path()) {
        m_lastDestination = d.path();
        m_lastDestinationName = d.dirName();
        emit lastDestinationCategoryChanged();
    }
    QTimer::singleShot(0, this, [this, key, d]() { moveEntry(key, d); });
    return true;
}

bool FileSystemModel::moveEntry(QModelIndex item, QModelIndex destinationDir) {
    if (!m_ready)
        return false;
    auto index = m_proxyModel->mapToSource(item);
    if (!index.isValid()) {
        qWarning() << "Failure mapping ProxyModel "<< item << " to fsmodel";
    }
    destinationDir = m_proxyModel->mapToSource(destinationDir);
    if (!filePath(index).size() || !filePath(destinationDir).size() || !isDir(destinationDir)) {
        qWarning() << "invalid input";
        return false;
    }
    QDir d(filePath(destinationDir));
    if (!d.exists()) {
        qWarning() << "Destination directory doesn't exist";
        return false;
    }
    if (m_lastDestination != d.path()) {
        m_lastDestination = d.path();
        m_lastDestinationName = d.dirName();
        emit lastDestinationCategoryChanged();
    }
    if (isDir(index)) {
        QDir f(filePath(index));
        if (!f.exists()) {
            qWarning() << "directory to move doesn't exist";
            return false;
        }
        QString newName = d.absoluteFilePath(fileName(index));
        const bool res = f.rename(f.absoluteFilePath(""), newName);
        if (res) {
            m_cache = cacheRoot(rootDirectory());
        }
        return res;
    } else {
        const QString &key = itemKey(index);
        if (!m_cache.contains(key)) {
            qWarning() << "Not present in cache: " << key;
            return false;
        }
        QTimer::singleShot(0, this, [this, key, d]() { moveEntry(key, d); });
        return true;
    }
}

// as of 2024.11.17 used only in BookmarkContextMenu.Move to (last dest)
void FileSystemModel::moveEntry(const QString &key, const QString &ds) {
    if (!m_ready || !m_cache.contains(key))
        return;

    QDir d(ds);
    if (!d.exists()) {
        qWarning() << "Destination directory doesn't exist";
        return;
    }
    m_cache[key].moveLocation(d);
}

void FileSystemModel::moveEntry(const QString &key, const QDir &d) {
    if (!m_ready || !m_cache.contains(key))
        return;
    m_cache[key].moveLocation(d);
}

bool FileSystemModel::addCategory(const QString &name) {
    if (!m_ready)
        return false;
    QDir d(rootPath());
    return d.mkdir(name);
}

bool FileSystemModel::updateEntry(const QString &key,
                                  const QString &title,
                                  const QString &channelURL,
                                  const QString &channelAvatarURL,
                                  const QString &channelName,
                                  const qreal duration,
                                  const qreal position) {
    if (!hasValidRoot() || !m_cache.contains(key))
        return false;

    auto channelID = QUrl(channelURL).path().mid(1);
    if (!channelID.startsWith('@')) {
        qWarning() << "Invalid channel parsed: "<<channelID;
        channelID.clear();
    }
    m_cache[key].setChannelID(channelID);
    if (!channelID.isEmpty() && m_bookmarksModel) {
        addChannel(channelID, Platform::YTB, channelName, channelAvatarURL);
    }
    bool updated = m_cache[key].update(title, position, duration);
    if (updated) {
        auto idx = index(m_cache[key].filePath());
        emit dataChanged(idx, idx);
    }
    return true;
}

void FileSystemModel::updateChannelID(const QString &key, const QString &channelID) {
    if (!m_ready || !m_bookmarksModel || !m_cache.contains(key))
        return;
    const bool updated = m_cache[key].setChannelID(channelID);
    if (updated) {
        auto idx = index(m_cache[key].filePath());
        emit dataChanged(idx, idx);
    }
}

void FileSystemModel::updateTitle(const QString &key, const QString &title) {
    if (!m_ready || !m_cache.contains(key) || title.isEmpty())
        return;
    const bool updated = m_cache[key].setTitle(title);
    if (updated) {
        auto idx = index(m_cache[key].filePath());
        emit dataChanged(idx, idx);
    }
}

void FileSystemModel::updateChannelAvatar(const QString &channelKey, const QByteArray avatar) {
    if (!m_ready || !m_bookmarksModel || !m_channelCache.contains(channelKey))
        return;
    m_channelCache[channelKey].setThumbnail(avatar);
    m_channelCache[channelKey].saveFile();
}

bool FileSystemModel::addEntry(const QString &key,
                               const QString &title,
                               const QString &channelURL,
                               const QString &channelAvatarURL,
                               const QString &channelName,
                               const qreal duration,
                               const qreal position) {
    if (!hasValidRoot())
        return false;

    if (!m_cache.contains(key))
        m_cache.insert(key, VideoMetadata(key, rootDirectory()));
    if (!m_cache.value(key).hasThumbnail()) {
        fetchThumbnail(key);
    }
    m_cache[key].update(title, position, duration);

    if (channelURL.isEmpty()) {
        ThumbnailFetcher::fetchChannel(key);
    } else {
        auto channelID = QUrl(channelURL).path().mid(1);
        if (!channelID.startsWith('@'))
            channelID.clear();
        m_cache[key].setChannelID(channelID);
        if (!channelID.isEmpty() && m_bookmarksModel) {
            addChannel(channelID, Platform::YTB, channelName, channelAvatarURL);
        }
    }

    if (YaycUtilities::isShortVideo(key) && !channelURL.isEmpty()) {
        m_cache[key].viewed = true;
    }

    m_cache[key].saveFile();
    return true;
}

void FileSystemModel::addThumbnail(const QString &key, const QByteArray &thumbnailData) {
    if (m_cache.contains(key) && !m_cache[key].hasThumbnail()) {
        m_cache[key].setThumbnail(thumbnailData);
    }
}

void FileSystemModel::updateChannel(const QString &key,
                                    const QString &channelId,
                                    const QString &channelName) {
    Q_UNUSED(channelName)
    if (m_cache.contains(key)) {
        m_cache[key].channelID = channelId;
    }
}

void FileSystemModel::addChannel(const QString &channelId,
                                 const Platform::Vendor vendor,
                                 const QString &channelName,
                                 const QString &channelAvatarURL) {
    if (!m_bookmarksModel || !m_ready) {
        qWarning() << "FileSystemModel not ready!";
        return;
    }
    const QString &key = ChannelMetadata::key(channelId, vendor);
    bool avatarNeedsFetch = true;
    if (m_channelCache.contains(key)) {
        avatarNeedsFetch = !m_channelCache[key].hasThumbnail();
        m_channelCache[key].setName(channelName);
    } else {
        QDir d(m_root);
        d.cd(".channels");
        m_channelCache[key] = ChannelMetadata::create(channelId, channelName, vendor, d);
    }
    if (avatarNeedsFetch)
        ThumbnailFetcher::fetchChannelAvatar(key, channelAvatarURL);
}

QString FileSystemModel::itemKey(const QModelIndex &index) const {
    if (!m_ready) {
        qWarning() << "FileSystemModel not ready!";
        return QString();
    }
    const auto &fi = fileInfo(index);
    return fi.baseName();
}

void FileSystemModel::fetchThumbnail(const QString &key) {
    ThumbnailFetcher::fetch(key);
}
