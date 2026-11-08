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

#include "UpdateFetcher.h"
#include "UpdateManager.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QThreadPool>

#include <utility>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace {

// Any https host: the two hashes guard the package. Plain http only under
// releasesUrl (test servers)
bool allowedUrl(const QJsonValue &value, const QString &releasesUrl, QUrl *url)
{
    const QString text = value.toString();
    const QUrl parsed(text, QUrl::StrictMode);
    if (!parsed.isValid() || parsed.host().isEmpty())
        return false;
    if (parsed.scheme() != QLatin1String("https")
            && !text.startsWith(releasesUrl + QLatin1Char('/')))
        return false;
    *url = parsed;
    return true;
}

} // namespace

UpdateFetcher::UpdateFetcher(QNetworkAccessManager &nam, QObject *context,
                             std::function<void(qreal)> progress)
    : m_nam(nam),
      m_context(context),
      m_progress(std::move(progress))
{
}

QByteArray UpdateFetcher::PartMeta::ifRange() const
{
    if (!etag.isEmpty() && !etag.startsWith("W/"))
        return etag;
    return lastModified;
}

UpdateFetcher::PartMeta UpdateFetcher::readPartMeta(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    return {o.value(QLatin1String("etag")).toString().toUtf8(),
            o.value(QLatin1String("lastModified")).toString().toUtf8()};
}

bool UpdateFetcher::writePartMeta(const QString &path, const PartMeta &meta)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    QJsonObject o;
    o.insert(QLatin1String("etag"), QString::fromUtf8(meta.etag));
    o.insert(QLatin1String("lastModified"), QString::fromUtf8(meta.lastModified));
    const QByteArray data = QJsonDocument(o).toJson(QJsonDocument::Compact);
    return f.write(data) == data.size();
}

void UpdateFetcher::runAsync(QObject *context, std::function<QString()> work,
                             std::function<void(const QString &)> done)
{
    // The worker posts only to this relay, which nobody else deletes. qApp is
    // no target: at exit it is gone before the pool is drained.
    // Leaks once if the app exits before delivery.
    auto *relay = new QObject;
    QThreadPool::globalInstance()->start(
                [relay, guard = QPointer<QObject>(context),
                 work = std::move(work), done = std::move(done)]() {
        const QString result = work();
        QMetaObject::invokeMethod(relay, [relay, guard, done, result]() {
            relay->deleteLater();
            if (guard)
                done(result);
        }, Qt::QueuedConnection);
    });
}

QString UpdateFetcher::sha256File(const QString &path)
{
    QFile f(path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!f.open(QIODevice::ReadOnly) || !hash.addData(&f))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

QNetworkRequest UpdateFetcher::freshRequest(const QUrl &url)
{
    QNetworkRequest request{url};
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    return request;
}

UpdateFetcher::FeedEntry UpdateFetcher::parseUpdateFeed(const QByteArray &json,
                                                        const QString &version,
                                                        const QString &asset,
                                                        const QString &releasesUrl)
{
    FeedEntry e;
    const auto invalid = [&e](const QString &what) {
        e.reason = QLatin1String("invalid update feed: ") + what;
        return e; // still Refused
    };
    const auto answer = [&e](Outcome outcome, const QString &reason) {
        e.outcome = outcome;
        e.reason = reason;
        return e;
    };

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (!doc.isObject())
        return invalid(error.error != QJsonParseError::NoError ? error.errorString()
                                                              : QStringLiteral("not an object"));
    const QJsonObject feed = doc.object();
    if (feed.value(QLatin1String("schema")).toInt() != 1)
        return invalid(QStringLiteral("schema"));
    // Before the comparison: garbage means an unusable feed, not another version
    const QString feedVersion = feed.value(QLatin1String("version")).toString();
    if (!UpdateManager::isValidVersion(feedVersion)
            || feed.value(QLatin1String("tag")).toString() != QLatin1String("yayc-v") + feedVersion)
        return invalid(QStringLiteral("version or tag"));
    e.version = feedVersion;
    if (feedVersion != version)
        return answer(Outcome::Unpublished, QLatin1String("update feed is at ") + feedVersion
                      + QLatin1String(", not ") + version);

    const QJsonValue checksums = feed.value(QLatin1String("checksums_url"));
    if (checksums.isNull() || checksums.isUndefined())
        return answer(Outcome::Unpublished, QLatin1String("no checksums for ") + version
                      + QLatin1String(" in the update feed"));
    if (!allowedUrl(checksums, releasesUrl, &e.checksumsUrl))
        return invalid(QStringLiteral("checksums_url"));

    const QJsonValue assets = feed.value(QLatin1String("assets"));
    if (!assets.isArray())
        return invalid(QStringLiteral("assets"));
    const QJsonArray list = assets.toArray();
    for (const QJsonValue &item : list) {
        const QJsonObject entry = item.toObject();
        if (entry.value(QLatin1String("name")).toString() != asset)
            continue;
        if (!allowedUrl(entry.value(QLatin1String("url")), releasesUrl, &e.assetUrl))
            return invalid(QLatin1String("url of ") + asset);
        static const QRegularExpression hexRe(
                    QRegularExpression::anchoredPattern(QStringLiteral("[0-9a-f]{64}")));
        const QString sha256 = entry.value(QLatin1String("sha256")).toString().toLower();
        if (!hexRe.match(sha256).hasMatch())
            return answer(Outcome::NotListed, QLatin1String("no checksum for ") + asset
                          + QLatin1String(" in the update feed"));
        e.sha256 = sha256;
        return answer(Outcome::Ok, QString());
    }
    return answer(Outcome::NotListed, asset + QLatin1String(" is not in the update feed"));
}

UpdateFetcher::Outcome UpdateFetcher::classify(const QNetworkReply *reply)
{
    const QNetworkReply::NetworkError e = reply->error();
    if (e == QNetworkReply::NoError)
        return Outcome::Ok;
    // The release does not have it (any more): retrying cannot help
    if (e == QNetworkReply::ContentNotFoundError || e == QNetworkReply::ContentGoneError)
        return Outcome::NotListed;
    // Busy or failing server: may pass
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 408 || status == 429 || status >= 500)
        return Outcome::Transport;
    switch (e) {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError: // transfer timeout
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyTimeoutError:
        return Outcome::Transport;
    default:
        // TLS, access denied, proxy auth, bad request: no network outage
        return Outcome::Refused;
    }
}

QString UpdateFetcher::partPath() const
{
    return m_pkg.path + QLatin1String(".part");
}

QString UpdateFetcher::metaPath() const
{
    return partPath() + QLatin1String(".meta");
}

void UpdateFetcher::download(const Package &pkg, Done done)
{
    start(pkg, Mode::Download, std::move(done));
}

void UpdateFetcher::verify(const Package &pkg, Done done)
{
    start(pkg, Mode::Verify, std::move(done));
}

void UpdateFetcher::start(const Package &pkg, Mode mode, Done done)
{
    ++m_job;
    m_mode = mode;
    m_pkg = pkg;
    m_done = std::move(done);
    m_expectedSha256.clear();
    m_feedVersion.clear();
    fetchUpdateFeed();
}

void UpdateFetcher::fetchUpdateFeed()
{
    m_reply = m_nam.get(freshRequest(m_pkg.updateFeedUrl));
    QObject::connect(m_reply, &QNetworkReply::finished,
                     m_context, [this]() { onUpdateFeedFinished(); });
}

void UpdateFetcher::onUpdateFeedFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();

    FeedEntry feed; // Refused until parsed
    if (reply->error() != QNetworkReply::NoError)
        feed.reason = QLatin1String("update feed unavailable: ") + reply->errorString();
    else
        feed = parseUpdateFeed(reply->readAll(), m_pkg.version, m_pkg.name, m_pkg.releasesUrl);
    m_feedVersion = feed.version;
    if (feed.outcome == Outcome::Refused) {
        // Unreadable: no word on the package. A verify keeps it, as in an
        // outage; a download waits for the next check
        done(m_mode == Mode::Verify ? Outcome::Transport : Outcome::Unpublished, feed.reason);
        return;
    }
    if (feed.outcome != Outcome::Ok) {
        done(feed.outcome, feed.reason);
        return;
    }
    m_pkg.checksumsUrl = feed.checksumsUrl;
    m_pkg.assetUrl = feed.assetUrl;
    m_expectedSha256 = feed.sha256;
    fetchChecksums();
}

void UpdateFetcher::fetchChecksums()
{
    m_reply = m_nam.get(freshRequest(m_pkg.checksumsUrl));
    QObject::connect(m_reply, &QNetworkReply::finished,
                     m_context, [this]() { onChecksumsFinished(); });
}

void UpdateFetcher::onChecksumsFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        Outcome outcome = classify(reply);
        // Every published release has one: this release is not out yet
        if (outcome == Outcome::NotListed)
            outcome = Outcome::Unpublished;
        done(outcome, QLatin1String("checksums unavailable: ") + reply->errorString());
        return;
    }

    // "<64 hex chars>  <filename>" per line (sha256sum format)
    QString listed;
    const QString sums = QString::fromUtf8(reply->readAll());
    static const QRegularExpression lineRe(
                QStringLiteral("^([0-9a-fA-F]{64})[ \\t*]+(\\S.*)$"),
                QRegularExpression::MultilineOption);
    auto it = lineRe.globalMatch(sums);
    while (it.hasNext()) {
        const auto match = it.next();
        if (match.captured(2).trimmed() == m_pkg.name) {
            listed = match.captured(1).toLower();
            break;
        }
    }
    if (listed.isEmpty()) {
        done(Outcome::NotListed, QLatin1String("no checksum listed for ") + m_pkg.name);
        return;
    }
    if (listed != m_expectedSha256) {
        done(Outcome::NotListed, QLatin1String("update feed and SHA256SUMS disagree for ")
             + m_pkg.name);
        return;
    }

    // Verify: the package must match. Download: it may be there already
    const QString dest = m_pkg.path;
    if (m_mode == Mode::Download && !QFileInfo::exists(dest)) {
        fetchAsset();
        return;
    }
    hashThen(dest, [this, dest](const QString &sha256) {
        if (sha256 == m_expectedSha256) {
            done(Outcome::Ok, QString());
        } else if (m_mode == Mode::Download) {
            QFile::remove(dest); // a completed download that mismatches
            fetchAsset();
        } else if (sha256.isEmpty()) {
            done(Outcome::Local, QLatin1String("cannot read update package: ") + dest);
        } else {
            done(Outcome::Mismatch, QLatin1String("checksum mismatch for ") + m_pkg.name);
        }
    });
}

void UpdateFetcher::hashThen(const QString &path, std::function<void(const QString &)> next)
{
    // Off-thread: hashing a package this big stalls the GUI
    m_hashing = true;
    const quint64 job = m_job;
    runAsync(m_context, [path]() { return sha256File(path); },
             [this, job, next = std::move(next)](const QString &sha256) {
        if (job != m_job)
            return; // cancelled or restarted meanwhile
        m_hashing = false;
        next(sha256);
    });
}

void UpdateFetcher::fetchAsset()
{
    // Firefox-style resume: only with a validator from the first response,
    // sent as If-Range, so a changed file comes back whole (200), not mixed
    const PartMeta meta = readPartMeta(metaPath());
    const qint64 have = QFileInfo(partPath()).size();
    if (have > 0 && !meta.ifRange().isEmpty()) {
        m_offset = have;
        requestAsset(meta.ifRange());
        return;
    }
    m_offset = 0;
    requestAsset({});
}

void UpdateFetcher::requestAsset(const QByteArray &ifRange)
{
    const QString part = partPath();
    m_output.setFileName(part);
    // Drop anything past the resume point (the whole file when offset 0)
    if (!m_output.open(QIODevice::ReadWrite) || !m_output.resize(m_offset)
            || !m_output.seek(m_offset)) {
        done(Outcome::Local, QLatin1String("cannot write to ") + part);
        return;
    }
    m_statusChecked = false;
    m_writeError.clear();

    QNetworkRequest request = freshRequest(m_pkg.assetUrl);
    if (m_offset > 0) {
        request.setRawHeader("Range", "bytes=" + QByteArray::number(m_offset) + '-');
        request.setRawHeader("If-Range", ifRange);
    }
    m_reply = m_nam.get(request);
    QObject::connect(m_reply, &QNetworkReply::readyRead,
                     m_context, [this]() { onAssetReadyRead(); });
    QObject::connect(m_reply, &QNetworkReply::downloadProgress,
                     m_context, [this](qint64 received, qint64 total) {
        if (total > 0)
            m_progress(qreal(m_offset + received) / qreal(m_offset + total));
    });
    QObject::connect(m_reply, &QNetworkReply::finished,
                     m_context, [this]() { onAssetFinished(); });
}

bool UpdateFetcher::acceptResponse(QNetworkReply *reply)
{
    // Decided once per request: an HTTP error body must never reach .part,
    // or the next resume would append to it
    if (m_statusChecked)
        return m_bodyIsAsset;
    m_statusChecked = true;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    m_bodyIsAsset = status == 200 || status == 206;
    if (status == 200) {
        // A whole file (fresh, or the server ignored / rejected If-Range):
        // start .part over and remember this response's validators
        m_offset = 0;
        if (!m_output.resize(0) || !m_output.seek(0)) {
            m_writeError = m_output.errorString();
            m_bodyIsAsset = false;
            return false;
        }
        writePartMeta(metaPath(), {reply->rawHeader("ETag"), reply->rawHeader("Last-Modified")});
    }
    return m_bodyIsAsset;
}

bool UpdateFetcher::writeChunk(const QByteArray &chunk)
{
    if (!m_writeError.isEmpty())
        return false;
    if (m_output.write(chunk) != chunk.size()) {
        m_writeError = m_output.errorString();
        return false;
    }
    return true;
}

void UpdateFetcher::onAssetReadyRead()
{
    QNetworkReply *reply = m_reply;
    const QByteArray chunk = reply->readAll();
    if (!acceptResponse(reply))
        return;
    if (!writeChunk(chunk))
        reply->abort(); // runs onAssetFinished() now; reply is gone after this
}

void UpdateFetcher::onAssetFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    // The retry from zero sends no Range: a second 416 cannot come here
    if (status == 416 && m_offset > 0) {
        // Nothing left to send: .part may be complete already (stopped before
        // the rename). Otherwise it is no prefix of the asset: start over once
        m_output.close();
        hashThen(partPath(), [this](const QString &sha256) {
            if (sha256 == m_expectedSha256) {
                finishPart();
                return;
            }
            m_offset = 0;
            requestAsset({});
        });
        return;
    }

    // Drain what arrived after the last readyRead
    if (reply->error() == QNetworkReply::NoError && acceptResponse(reply))
        writeChunk(reply->readAll());
    if (m_writeError.isEmpty() && !m_output.flush())
        m_writeError = m_output.errorString();
    m_output.close();

    // On any failure .part stays: the next attempt resumes from its bytes
    if (!m_writeError.isEmpty()) {
        done(Outcome::Local, QLatin1String("cannot write download: ") + m_writeError);
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        Outcome outcome = classify(reply);
        // SHA256SUMS lists it: a 404 here is the server's. An error with
        // Retry, not "no package" for the session
        if (outcome == Outcome::NotListed)
            outcome = Outcome::Transport;
        done(outcome, QLatin1String("download failed: ") + reply->errorString());
        return;
    }

    // Hash what is on disk, not what came over the wire: the final name
    // must always mean "these bytes matched"
    const QString part = partPath();
    hashThen(part, [this, part](const QString &sha256) {
        if (sha256 != m_expectedSha256) {
            // A completed download that mismatches: the one case that deletes
            QFile::remove(part);
            QFile::remove(metaPath());
            done(Outcome::Mismatch, QLatin1String("checksum mismatch for ") + m_pkg.name);
            return;
        }
        finishPart();
    });
}

void UpdateFetcher::finishPart()
{
    // Only called once the bytes of .part on disk matched both sources
#ifdef Q_OS_UNIX
    // On disk before the final name vouches for them: after a power loss,
    // "Install anyway" (no hash) must not find a truncated package
    {
        QFile part(partPath());
        if (!part.open(QIODevice::ReadOnly) || ::fsync(part.handle()) != 0) {
            done(Outcome::Local, QLatin1String("cannot write download: sync failed"));
            return;
        }
    }
#endif
    QFile::remove(m_pkg.path);
    if (!QFile::rename(partPath(), m_pkg.path)) {
        done(Outcome::Local, QLatin1String("cannot move download into place"));
        return;
    }
    QFile::remove(metaPath());
    done(Outcome::Ok, QString());
}

void UpdateFetcher::done(Outcome outcome, const QString &reason)
{
    // Reset first: the callback may start the next fetch at once
    const Done callback = std::move(m_done);
    reset();
    if (callback)
        callback(outcome, reason);
}

void UpdateFetcher::cancel()
{
    ++m_job;
    if (m_reply) {
        QObject::disconnect(m_reply, nullptr, m_context, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    reset(); // .part stays: the next attempt resumes from it
}

void UpdateFetcher::reset()
{
    if (m_output.isOpen())
        m_output.close();
    m_output.setFileName(QString());
    m_offset = 0;
    m_hashing = false;
    m_done = nullptr;
}
