/*
Copyright (C) 2023- YAYC team <yaycteam@gmail.com>

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

#ifndef VIDEOMETADATA_H
#define VIDEOMETADATA_H

#include "Platform.h"

#include <QString>
#include <QDir>
#include <QUrl>
#include <QByteArray>
#include <QDateTime>

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
    bool dirty{false};

    ~VideoMetadata();
    VideoMetadata();
    VideoMetadata(const QString &k, const QDir &p);

    bool hasThumbnail() const { return thumbnailData.size(); }

    void setDuration(qreal d);
    void setPosition(qreal p);
    void setViewed(bool v);
    void setStarred(bool s);
    bool setTitle(const QString &t);
    bool setChannelID(const QString &cid);
    bool update(const QString &t, qreal p = 0., qreal d = 0.);
    bool moveLocation(const QDir &d);
    bool eraseFile();
    void setThumbnail(const QByteArray &ba);
    const QByteArray &thumbnail() const;
    void saveFile();
    void loadFile();
    QString filePath() const;
    QUrl url(bool startingTime = true) const;
};

#endif // VIDEOMETADATA_H
