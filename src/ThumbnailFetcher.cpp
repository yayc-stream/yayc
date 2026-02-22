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

#include "ThumbnailFetcher.h"
#include "FileSystemModel.h"
#include "ThumbnailImageProvider.h"
#include "Platform.h"
#include "ChannelMetadata.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QTextDocument>

ThumbnailFetcher::ThumbnailFetcher(QObject *parent) : QObject(parent) {
    m_nam.setCookieJar(new QNetworkCookieJar);
}

ThumbnailFetcher &ThumbnailFetcher::GetInstance() {
    static ThumbnailFetcher instance;
    return instance;
}

void ThumbnailFetcher::registerModel(FileSystemModel &model) {
    auto &instance = GetInstance();
    instance.m_models.insert(&model);
}

void ThumbnailFetcher::unregisterModel(FileSystemModel &model) {
    auto &instance = GetInstance();
    instance.m_models.remove(&model);
}

void ThumbnailFetcher::fetch(const QString &key) {
    auto &instance = GetInstance();
    instance.fetchThumbnail(key);
}

void ThumbnailFetcher::fetchChannel(const QString &key) {
    auto &instance = GetInstance();
    instance.fetchChannelInternal(key);
}

void ThumbnailFetcher::fetchChannelAvatar(const QString &channelKey, const QString &url) {
    auto &instance = GetInstance();
    instance.fetchChannelAvatarInternal(channelKey, url);
}

void ThumbnailFetcher::fetchMissing() {
    printStats();
    auto &instance = GetInstance();
    instance.fetchMissingThumbnails();
}

void ThumbnailFetcher::printStats() {
    auto &instance = GetInstance();
    QLoggingCategory category("qmldebug");
    qCInfo(category) << "Failed fetching " << instance.m_failures << " thumbnail requests";
}

void ThumbnailFetcher::fetchThumbnail(const QString &key) {
    auto ytKey = videoID(key);

    QNetworkRequest req(
        QUrl(QString(QLatin1String("https://img.youtube.com/vi/%1/0.jpg")).arg(ytKey)));
    auto *reply = m_nam.get(req);
    if (!reply) {
        qWarning("NULL QNetworkReply while retrieving thumbnails");
        return;
    }
    QObject::connect(reply, &QNetworkReply::finished, this,
                     &ThumbnailFetcher::onThumbnailRequestFinished);
    reply->setProperty("key", key);
}

void ThumbnailFetcher::fetchChannelInternal(const QString &key) {
    const QString sUrl = Platform::toUrl(key, 0);
    const QUrl url = sUrl;

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent",
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                     "(KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36");
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::UserVerifiedRedirectPolicy);
    req.setRawHeader("COOKIE", "CONSENT=YES+42");
    auto *reply = m_nam.get(req);
    if (!reply) {
        qWarning("NULL QNetworkReply while retrieving thumbnails");
        return;
    }
    QObject::connect(reply, &QNetworkReply::finished, this,
                     &ThumbnailFetcher::onVideoPageRequestFinished);
    QObject::connect(reply, &QNetworkReply::redirected, reply, &QNetworkReply::redirectAllowed);
    reply->setProperty("key", key);
}

void ThumbnailFetcher::fetchChannelAvatarInternal(const QString &channelKey, QString url) {
    if (url.isEmpty())
        return;
    url = avatarUrl(url);

    const QUrl u(url);
    QNetworkRequest req(u);
    auto *reply = m_nam.get(req);
    if (!reply) {
        qWarning("NULL QNetworkReply while retrieving channel avatar");
        return;
    }

    QObject::connect(reply, &QNetworkReply::finished, this,
                     &ThumbnailFetcher::onFetchAvatarRequestFinished);
    reply->setProperty("channelKey", channelKey);
}

FileSystemModel *ThumbnailFetcher::bookmarksModel() {
    for (auto m : std::as_const(m_models))
        if (m->m_bookmarksModel)
            return m;
    return nullptr;
}

void ThumbnailFetcher::onThumbnailRequestFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        qWarning("NULL QNetworkReply while retrieving thumbnails");
        return;
    }

    const QString key = reply->property("key").toString();
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray networkContent = reply->readAll();
        if (networkContent.size()) {
            for (auto &m : std::as_const(m_models)) {
                m->addThumbnail(key, networkContent);
            }
            if (m_models.size()) {
                QQmlApplicationEngine *engine =
                    qobject_cast<QQmlApplicationEngine *>((*m_models.begin())->parent());
                Q_ASSERT(engine);
                if (!engine)
                    qFatal("ThumbnailFetcher: failed to retrieve QQmlApplicationEngine");
                ThumbnailImageProvider *provider = static_cast<ThumbnailImageProvider *>(
                    engine->imageProvider(QLatin1String("videothumbnail")));
                Q_ASSERT(provider);
                if (!provider)
                    qFatal("ThumbnailFetcher: failed to retrieve ThumbnailImageProvider");
                provider->insert(key, networkContent);
            }
        } else {
            ++m_failures;
        }
    } else {
        if (auto m = bookmarksModel())
            qWarning() << "Error while retrieving thumbnail: " << reply->errorString() << " : "
                       << reply->url() << " Channel: " << m->m_cache.value(key).channelID
                       << " Video " << m->m_cache.value(key).title;
        ++m_failures;
    }
    reply->deleteLater();
}

void ThumbnailFetcher::onVideoPageRequestFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        qFatal("NULL QNetworkReply while retrieving thumbnails");
        return;
    }
    if (reply->error() == QNetworkReply::NoError) {
        const QString key = reply->property("key").toString();

        QByteArray networkContent = reply->readAll();
        if (networkContent.size()) {
            QString sData = QString::fromUtf8(networkContent);
            QRegularExpression re(
                "<span itemprop=\"author\" itemscope itemtype=\"http://schema.org/Person\">"
                "<link itemprop=\"url\" href=\"http://www.youtube.com/(.+?)\">"
                "<link itemprop=\"name\" content=\"(.+?)\">");
            QRegularExpressionMatch match = re.match(sData);
            if (match.hasMatch()) {
                const QString channelId = match.captured(1);
                const QString channelName = match.captured(2);

                QRegularExpression re2;
                if (!isShorts(key)) {
                    re2 = QRegularExpression(
                        "channelAvatar\":\\{\"thumbnails\":\\[\\{\"url\":\"(https://.*?)\"");
                } else {
                    re2 = QRegularExpression("canonicalBaseUrl\":\"/" + channelId +
                                             "\"\\}\\}\\}\\]\\},\"channelThumbnail\":\\{"
                                             "\"thumbnails\":\\[\\{\"url\":\"(https://.*?)\"");
                }

                QRegularExpressionMatch match2 = re2.match(sData);

                QString channelAvatarURL;
                if (match2.hasMatch())
                    channelAvatarURL = match2.captured(1);

                QRegularExpression re3("<title>(.*?)</title>");
                QString title;
                QRegularExpressionMatch match3 = re3.match(sData);

                if (match3.hasMatch()) {
                    title = match3.captured(1).replace(QRegularExpression(" - YouTube$"), "");
                    QTextDocument td;
                    td.setHtml(title);
                    title = td.toPlainText();
                }

                for (auto &m : std::as_const(m_models)) {
                    m->addChannel(channelId, Platform::toVendor(videoVendor(key)), channelName,
                                  channelAvatarURL);
                    m->updateChannelID(key, channelId);
                    if (!title.isEmpty())
                        m->updateTitle(key, title);
                }
            } else {
                ++m_channelIdFailures;
            }
        } else {
            ++m_channelIdFailures;
        }
    } else {
        qWarning() << "Error while retrieving video page: " << reply->errorString() << " : "
                   << reply->url();
        ++m_channelIdFailures;
    }
    reply->deleteLater();
}

void ThumbnailFetcher::onFetchAvatarRequestFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        qFatal("NULL QNetworkReply while retrieving thumbnails");
        return;
    }
    const QString channelKey = reply->property("channelKey").toString();

    if (reply->error() == QNetworkReply::NoError) {
        const QByteArray &networkContent = reply->readAll();
        for (auto &m : std::as_const(m_models)) {
            m->updateChannelAvatar(channelKey, networkContent);
        }
    } else {
        qWarning() << "Error while retrieving channel avatar: " << reply->errorString() << " : "
                   << reply->url();
    }
    reply->deleteLater();
}

void ThumbnailFetcher::fetchMissingThumbnails() {
    QSet<QString> missingKeys;
    qDebug() << "Missing Thumbs:";
    for (auto &m : std::as_const(m_models)) {
        for (auto i = m->m_cache.begin(); i != m->m_cache.end(); ++i) {
            if (!i.value().hasThumbnail()) {
                missingKeys.insert(i.key());
                qDebug() << i.key() << " " << i.value().title;
            }
        }
    }
    for (const auto &k : missingKeys) {
        fetchThumbnail(k);
    }
    missingKeys.clear();
    qDebug() << "Missing channel:";
    if (auto m = bookmarksModel()) {
        for (auto i = m->m_cache.begin(); i != m->m_cache.end(); ++i) {
            if (i.value().channelID.isEmpty()) {
                missingKeys.insert(i.key());
                qDebug() << i.key() << " " << i.value().title;
            }
        }
    } else {
        qFatal("m_bookmarksModel is NULL!");
    }
    for (const auto &k : missingKeys) {
        fetchChannelInternal(k);
    }
}
