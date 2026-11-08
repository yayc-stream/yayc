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

#ifndef UPDATEFETCHER_H
#define UPDATEFETCHER_H

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QObject;

// Downloads one release package, or re-verifies a downloaded one. The update
// feed (the site) and the release's SHA256SUMS must give the same SHA-256: a
// forged package has to get past both. Callbacks run only while the context
// object lives
class UpdateFetcher
{
public:
    // How a fetch ended. Transport = network outage, may work later;
    // Refused = the server or TLS said no (no "install anyway"); NotListed and
    // Mismatch = this artifact can never be trusted; Local = our side.
    // Unpublished = nothing usable for this version yet: the next check retries.
    enum class Outcome { Ok, Transport, Refused, Unpublished, NotListed, Mismatch, Local };
    using Done = std::function<void(Outcome outcome, const QString &reason)>;

    struct Package {
        QString version;     // the feed must be about this one
        QString name;        // as in the feed and in SHA256SUMS
        QString path;        // verified bytes only; .part and .part.meta next to it
        QUrl updateFeedUrl;
        QString releasesUrl; // UpdateConfig::releasesUrl
        // Taken from the feed
        QUrl checksumsUrl;   // the release's SHA256SUMS
        QUrl assetUrl;
    };

    // What the feed says about one package. Refused: the feed is unusable
    struct FeedEntry {
        Outcome outcome{Outcome::Refused};
        QString reason;
        QString version;     // the feed's, once it is a valid X.Y.Z
        QUrl assetUrl;
        QUrl checksumsUrl;
        QString sha256;      // lower case
    };

    // Validators of a .part download, for an If-Range resume
    struct PartMeta {
        QByteArray etag;
        QByteArray lastModified;
        // If-Range needs a strong ETag, or else a date
        QByteArray ifRange() const;
    };

    UpdateFetcher(QNetworkAccessManager &nam, QObject *context,
                  std::function<void(qreal)> progress);

    // \a done runs once. A download resumes its .part
    void download(const Package &pkg, Done done);
    void verify(const Package &pkg, Done done);
    void cancel(); // no callback; .part stays
    bool active() const { return m_reply != nullptr || m_hashing; }
    // After an Ok outcome: the SHA-256 feed and SHA256SUMS agree on, which the
    // package matched
    QString verifiedSha256() const { return m_expectedSha256; }
    // After any outcome: the feed's version; empty if unread or invalid
    QString feedVersion() const { return m_feedVersion; }

    // The entry of \a asset in the update feed \a json, which must be about
    // \a version
    static FeedEntry parseUpdateFeed(const QByteArray &json, const QString &version,
                                     const QString &asset, const QString &releasesUrl);

    static PartMeta readPartMeta(const QString &path);
    static bool writePartMeta(const QString &path, const PartMeta &meta);
    // Runs \a work on the thread pool, then \a done with its result on the
    // calling thread, only while \a context lives
    static void runAsync(QObject *context, std::function<QString()> work,
                         std::function<void(const QString &)> done);
    // Hex SHA-256 of a file; empty on a read error
    static QString sha256File(const QString &path);
    // AlwaysNetwork also sends "Cache-Control: no-cache": no proxy may
    // answer with a stale APPVERSION, update feed or SHA256SUMS
    static QNetworkRequest freshRequest(const QUrl &url);

private:
    enum class Mode { Download, Verify };
    static Outcome classify(const QNetworkReply *reply);
    void start(const Package &pkg, Mode mode, Done done);
    void fetchUpdateFeed();
    void onUpdateFeedFinished();
    void fetchChecksums();
    void onChecksumsFinished();
    void hashThen(const QString &path, std::function<void(const QString &)> next);
    void fetchAsset();
    void requestAsset(const QByteArray &ifRange);
    void finishPart();
    bool acceptResponse(QNetworkReply *reply);
    bool writeChunk(const QByteArray &chunk);
    void onAssetReadyRead();
    void onAssetFinished();
    void done(Outcome outcome, const QString &reason);
    void reset();
    QString partPath() const;
    QString metaPath() const;

    QNetworkAccessManager &m_nam;
    QObject *m_context;
    std::function<void(qreal)> m_progress;
    Mode m_mode{Mode::Download};
    Package m_pkg;
    Done m_done;
    QString m_expectedSha256;        // the feed's; SHA256SUMS must agree
    QString m_feedVersion;
    QNetworkReply *m_reply{nullptr}; // feed, checksums, then asset
    QFile m_output;                  // .part while downloading; kept on error
    qint64 m_offset{0};              // bytes already in .part (resume)
    bool m_statusChecked{false};     // first response of this request seen
    bool m_bodyIsAsset{false};       // 200/206: the body goes to .part
    QString m_writeError;            // set: stop, keep .part
    bool m_hashing{false};           // a file is hashed off-thread
    quint64 m_job{0};                // drops results of cancelled runs
};

#endif // UPDATEFETCHER_H
