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
const QString videoExtension{"yayc"};
const QString channelExtension{"yaycc"};
const QString shortsVideoPattern{"https://youtube.com/shorts/"};
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
QString videoVendor(const QString &key) {
    return key.mid(0,3);
}
QString videoID(const QString &key) {
    return key.mid(5);
}
QString channelID(const QString &key) {
    return key.mid(4);
}
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
        const QString host = url.host();
        if (!LUT.contains(host))
            return Platform::UNK;
        return LUT.value(host);
    }
};

struct ChannelMetadata
{
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
        auto data = fromKey(key);
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
    QPair<QString, Platform::Vendor> fromKey(const QString &key) {
        const QString platformString = videoVendor(key);
        const QString id = channelID(key);
        return QPair<QString, Platform::Vendor>(id, Platform::toVendor(platformString));
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
        position = p;
        dirty = true;
        const auto threshold = duration * 0.9;
        if (duration > 5 && position > threshold && oldPosition <= threshold) {
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

    QUrl url() const {
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

namespace  {
bool findExec(const QString &name) {
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
QUrl removeWww(QUrl url) {
    QString host = url.host();
    if (host.startsWith(QLatin1String("www."))) {
        host = host.mid(4); // remove first 4 chars
        url.setHost(host);
    }
    return url;
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
    RequestInterceptor(QObject *parent = nullptr) : QWebEngineUrlRequestInterceptor(parent), tcpSocket(new QTcpSocket(this))
    {
        connect(tcpSocket, &QAbstractSocket::connected, this, &RequestInterceptor::onSocketConnected);
        connect(tcpSocket, &QAbstractSocket::errorOccurred, this, &RequestInterceptor::onSocketError);
    }

    Q_INVOKABLE void setEasyListPath(QString newPath) {
        if (!m_easyListPath.isEmpty() || m_loading)
            return; // require restart for changes to existing paths to take effect

        if (newPath.startsWith("file://")) {
            newPath = newPath.mid(7);
#if defined(Q_OS_WINDOWS)
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

    Q_INVOKABLE bool isYoutubeVideoUrl(QUrl url) {
        url = removeWww(url);
        const QString surl = url.toString();
        return (isYoutubeStandardUrl(surl) || isYoutubeShortsUrl(surl));
    }

    Q_INVOKABLE bool isYoutubeStandardUrl(QUrl url) {
        url = removeWww(url);
        const QString surl = url.toString();
        return isYoutubeStandardUrl(surl);
    }

    bool isYoutubeStandardUrl(const QString &url) {
        return url.startsWith(standardVideoPattern);
    }

    Q_INVOKABLE bool isYoutubeShortsUrl(QUrl url) {
        url = removeWww(url);
        const QString surl = url.toString();
        return isYoutubeShortsUrl(surl);
    }

    bool isYoutubeShortsUrl(const QString &url) {
        return url.startsWith(shortsVideoPattern);
    }

    Q_INVOKABLE QString getVideoID(QUrl url) {
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
    }

    Q_INVOKABLE void checkConnectivity() {
        tcpSocket->connectToHost("github.com", 80);
    }

    Q_INVOKABLE void getLatestVersion() {
        QNetworkRequest request(latestReleaseVersionURL);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                           QNetworkRequest::AlwaysNetwork);
        QNetworkReply *reply = m_nam.get(std::move(request));
        connect(reply, &QNetworkReply::finished, this, &RequestInterceptor::onReplyFinished);
        // Errors cause reply to finish in any case.
    }

    Q_INVOKABLE void getDonateEtag() {
        QNetworkRequest request(donateURL);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                           QNetworkRequest::AlwaysNetwork);
        QNetworkReply *reply = m_nam.head(std::move(request));
        connect(reply, &QNetworkReply::finished, this, &RequestInterceptor::onDonateEtagReplyFinished);
    }

    Q_INVOKABLE void getDonateURL() {
        QNetworkRequest request(donateURL);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                           QNetworkRequest::AlwaysNetwork);
        QNetworkReply *reply = m_nam.get(std::move(request));
        connect(reply, &QNetworkReply::finished, this, &RequestInterceptor::onDonateReplyFinished);
    }

    // ToDo: move this and the above functions to a utility object
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
    // ToDO: same
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

signals:
    void youtubeUrlRequested(const QUrl &url);
    void networkFound();
    void latestVersion(const QString &);
    void donateETag(const QString &);
    void donateUrl(const QString &);

protected:
    QAtomicInt m_loading{0};
    QString m_easyListPath;
    AdBlockClient client;
    unsigned int blocked{0};
    QTcpSocket *tcpSocket;
    QNetworkAccessManager m_nam;

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
public:
    NoDirSortProxyModel() = default;
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
        m_images[key] = QImage::fromData(thumb);
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

class FileSystemModel : public QFileSystemModel {
    Q_OBJECT

    bool m_ready{false};
    bool m_bookmarksModel{false};
    QHash<QString, VideoMetadata> m_cache;
    QHash<QString, ChannelMetadata> m_channelCache;
    QModelIndex m_rootPathIndex;
    QScopedPointer<NoDirSortProxyModel> m_proxyModel;
    QString m_contextPropertyName;
    QNetworkAccessManager m_nam;
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

    explicit FileSystemModel(QString contextPropertyName, bool bookmarks, QObject *parent = nullptr)
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
                  // | QDir::Hidden
                  );
        sort(3);
        QScopedPointer<NoDirSortProxyModel> pm(new NoDirSortProxyModel);
        pm->setObjectName("ProxyModel");
        m_proxyModel.swap(pm);
    }

    ~FileSystemModel()
    {
    }

    enum Roles  {
        SizeRole = Qt::UserRole + 4,
        DisplayableFilePermissionsRole = Qt::UserRole + 5,
        LastModifiedRole = Qt::UserRole + 6,
        UrlStringRole = Qt::UserRole + 7,
        ContentNameRole = Qt::UserRole + 8,
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
            FileSystemModel *fsmodel = new FileSystemModel(m_contextPropertyName, true, engine);
            fsmodel->m_bookmarksModel = m_bookmarksModel;
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
        m_ready = true;
        m_cache = cacheRoot(m_root);
        if (m_bookmarksModel) {
            m_root.mkdir(".channels");
            m_channelCache = cacheChannels(m_root);
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
            m_proxyModel->setFilterRegularExpression("^[^\\.].*"); // Skip entries starting with .
            m_proxyModel->setSortRole(LastModifiedRole);
            m_proxyModel->sort(3);

            m_rootPathIndex = m_proxyModel->mapFromSource(res);
            if (!m_rootPathIndex.isValid()) {
                qFatal("Failure mapping FileSystemModel root path index to proxy model");
            }
            engine->rootContext()->setContextProperty(m_contextPropertyName, this);

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
                switch (index.column()) {
                case 0: {
                    if (!isDir(index)) {
                        const QString &key = itemKey(index);
                        if (!m_cache.contains(key))
                            return QFileSystemModel::data(index, role);
//                        const auto &title = m_cache.value(key).title;
//                        if (title.size())
//                            return title;
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

    static bool isShortVideo(const QString &fkey) {
        if (!fkey.size())
            return false;
        return videoType(fkey) == QLatin1String("s_"); // strip first 3 chars
    }
public slots:
    QString keyFromViewItem(const QModelIndex &item) const {
        if (!m_ready)
            return QLatin1String("");
        auto index = m_proxyModel->mapToSource(item);
        if (isDir(index)) {
            return "";
        }
        return itemKey(index);
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

    bool popen(QModelIndex item) {
        if (!m_ready)
            return false;
        item = m_proxyModel->mapToSource(item);

        return false;
        // ToDo: finish me. Make sure the workdir is appropriate.
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
        if (!m_cache.contains(key)) {
            qWarning() << "FileSystemModel::duration: Key "<<key<<" not present!";
            return 0;
        }

        if (!key.size())
            return 0.;

        return m_cache.value(key).duration;
    }
    bool isShortVideo(const QModelIndex &item) const {
        if (!m_ready)
            return false;
        const QString &key = keyFromViewItem(item);
        return isShortVideo(key);
    }
    bool isViewed(const QModelIndex &item) const {
        if (!m_ready)
            return false;
        const QString &key = keyFromViewItem(item);
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
        if (!key.size() || !m_cache.contains(key))
            return false;
        return m_cache.value(key).starred;
    }
    void starEntry(const QModelIndex &item, bool starred) {
        if (!m_ready)
            return;
        const bool currentValue = isStarred(item);
        if (starred == currentValue)
            return;
        const QString &key = keyFromViewItem(item);
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

    QString avatarUrl(QString originalAvatarUrl) {
        return originalAvatarUrl.replace(QLatin1String("=s48-"), QLatin1String("=s128-"));
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
        if (!m_cache.value(key).thumbnail().size()) {
            fetchThumbnail(key);
        }
        // ToDo: handle more platforms!
        auto channelID = channelURL.mid(24); // strip https://www.youtube.com/
        m_cache[key].update(title, position, duration);
        m_cache[key].setChannelID(channelID);
        m_cache[key].creationDate = QDateTime::currentDateTimeUtc();

        if (!channelID.isEmpty() && m_bookmarksModel) {
            addChannel(channelID,
                       Platform::YTB,
                       channelName,
                       channelAvatarURL);
        }

        if (isShortVideo(key) && !channelURL.isEmpty()) {
            m_cache[key].viewed = true;
        }

        m_cache[key].saveFile(); // so it pops up on the view
        return true;
    }

signals:
    void filesAdded(const QVariantList & addedPaths);
    void rootPathIndexChanged();
    void sortFilterProxyModelChanged();

private slots:
    void onThumbnailRequestFinished()
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (!reply)
            qFatal("NULL QNetworkReply while retrieving thumbnails");

        if (reply->error() == QNetworkReply::NoError ) {
            QString key = reply->property("key").toString();
            if (m_cache.contains(key)) {

                QByteArray networkContent = reply->readAll();

                m_cache[key].setThumbnail(networkContent);
                QQmlApplicationEngine *engine = qobject_cast<QQmlApplicationEngine*>(parent());
                ThumbnailImageProvider *provider = static_cast<ThumbnailImageProvider*>(engine->imageProvider(QLatin1String("videothumbnail")));
                provider->insert(key, m_cache[key].thumbnail());
            }
        } else {
            qWarning() << "Error while retrieving thumbnail: " << reply->errorString() << " : "
                      << reply->url() ;
        }
        reply->deleteLater();
    }

    void onChannelAvatarRequestFinished()
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (!reply)
            qFatal("NULL QNetworkReply while retrieving channel avatar");

        if (reply->error() == QNetworkReply::NoError ) {
            QString key = reply->property("key").toString();
            if (m_channelCache.contains(key)) {
                QByteArray networkContent = reply->readAll();
                m_channelCache[key].setThumbnail(networkContent);
                if (reply->property("persist").toBool()) {
                    m_channelCache[key].saveFile();
                }
            }
        } else {
            qWarning() << "Error while retrieving channel avatar: " << reply->errorString() << " : "
                      << reply->url() ;
        }
        reply->deleteLater();
    }

private:
    void addChannel(const QString &channelId,
                    const Platform::Vendor vendor,
                    const QString &channelName,
                    const QString &channelAvatarURL) {
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
            fetchAvatar(key, channelAvatarURL, persist);

    }
    // this one needs the QModelIndex in FileSystemModel space!
    QString itemKey(const QModelIndex &index) const {
        if (!m_ready)
            return QLatin1String("");
        return fileInfo(index).baseName();
    }

    void fetchAvatar(const QString &key, QString url, const bool persist) {
        if (url.isEmpty())
            return;
        url = avatarUrl(url);

        const QUrl u(url);
        QNetworkRequest req(u);
        auto *reply = m_nam.get(req);
        if (!reply)
            qFatal("NULL QNetworkReply while retrieving channel avatar");
        m_channelCache[key].setThumbnail(QByteArrayLiteral("0")); // placeholder to skip subsequent calls while reply hasn't arrived.
        QObject::connect(reply, &QNetworkReply::finished,
                         this, &FileSystemModel::onChannelAvatarRequestFinished);
        reply->setProperty("key", key);
        reply->setProperty("persist", persist);
    }

    void fetchThumbnail(const QString &key)
    {
        auto ytKey = videoID(key);
        // ToDo: support other platforms!
        QNetworkRequest req(QUrl(
            QString(QLatin1String("https://img.youtube.com/vi/%1/0.jpg")).arg(ytKey)));
        auto *reply = m_nam.get(req);
        if (!reply)
            qFatal("NULL QNetworkReply while retrieving thumbnails");
        QObject::connect(reply, &QNetworkReply::finished,
                         this, &FileSystemModel::onThumbnailRequestFinished);
        reply->setProperty("key", key);
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("YAYC");
    QCoreApplication::setApplicationName("yayc");

    QSettings settings;
#if defined(Q_OS_LINUX)
    qputenv("QT_QPA_PLATFORMTHEME", QByteArrayLiteral("gtk3"));
#endif
    qputenv("QT_QUICK_CONTROLS_STYLE", QByteArrayLiteral("Material"));


    if (!settings.contains("darkMode") || settings.value("darkMode").toBool()) {
//        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--blink-settings=darkModeEnabled=true"); // QTBUG-84484
//        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--blink-settings=darkMode=4,darkModeImagePolicy=2");
//     https://chromium.googlesource.com/chromium/src/+/821cfffb54899797c86ca3eb351b73b91c2c5879/third_party/blink/web_tests/VirtualTestSuites
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
                QByteArrayLiteral("--dark-mode-settings=ImagePolicy=1 --blink-settings=forceDarkModeEnabled=true"));  // Current Chromium
        qputenv("QT_QUICK_CONTROLS_MATERIAL_THEME", QByteArrayLiteral("Dark")); // ToDo: fix text color
    }
    qputenv("QT_QUICK_CONTROLS_MATERIAL_PRIMARY", QByteArrayLiteral("#3d3d3d"));
    qputenv("QT_QUICK_CONTROLS_MATERIAL_ACCENT", QByteArrayLiteral("Red"));

    qInfo("Starting YAYC v%s ...", appVersion().data());
#ifdef QT_NO_DEBUG_OUTPUT
//    QLoggingCategory::setFilterRules(QStringLiteral("*.fatal=true\n*=false"));
#endif

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QtWebEngine::initialize();

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/images/yayc-alt.png"));



    QQmlApplicationEngine engine;
    ThumbnailImageProvider *imageProvider = new ThumbnailImageProvider();
    engine.addImageProvider(QLatin1String("videothumbnail"), imageProvider);
    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);


    FileSystemModel *fsmodel = new FileSystemModel("fileSystemModel", true, &engine);
    FileSystemModel *historyModel = new FileSystemModel("historyModel", false, &engine);

    engine.rootContext()->setContextProperty("fileSystemModel", fsmodel);
    engine.rootContext()->setContextProperty("historyModel", historyModel);

    // for the roles enums
    qmlRegisterUncreatableType<FileSystemModel>("yayc", 1, 0,
                                                "FileSystemModel", "Cannot create a FileSystemModel instance.");

    RequestInterceptor *interceptor = new RequestInterceptor(&engine);
    engine.rootContext()->setContextProperty("requestInterceptor", interceptor);
    engine.rootContext()->setContextProperty("appVersion", QString(appVersion()) );
    engine.rootContext()->setContextProperty("repositoryURL", repositoryURL );

    engine.load(url);

    QTimer::singleShot(1000,[&engine, interceptor](){
        QObject *view = engine.rootObjects().first()->findChild<QObject *>("webEngineView");
        QQuickWebEngineProfile *profile = qvariant_cast<QQuickWebEngineProfile *>(view->property("profile"));

        profile->setUrlRequestInterceptor(interceptor);
    });

    return app.exec();
}

#include "main.moc"
