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

#include "ChannelMetadata.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

ChannelMetadata ChannelMetadata::create(const QString &id,
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

ChannelMetadata::ChannelMetadata() {}

ChannelMetadata::ChannelMetadata(const QString &key, const QDir &parent)
    : channelsRoot(parent) {
    const KeyType data = fromKeyString(key);
    id = data.first;
    vendor = data.second;
}

bool ChannelMetadata::hasThumbnail() const {
    return !thumbnailData.isEmpty();
}

QString ChannelMetadata::key() const {
    return key(id, vendor);
}

QString ChannelMetadata::key(const QString &id, const Platform::Vendor vendor) {
    return Platform::toString(vendor) + "_" + id;
}

ChannelMetadata::KeyType ChannelMetadata::fromKeyString(const QString &key) {
    const QString platformString = videoVendor(key);
    const QString id = channelID(key);
    return KeyType(id, Platform::toVendor(platformString));
}

QString ChannelMetadata::filePath() const {
    return channelsRoot.absoluteFilePath(key() + "." + channelExtension);
}

void ChannelMetadata::setName(const QString &n) {
    if (name == n)
        return;
    dirty = true;
    name = n;
}

void ChannelMetadata::setThumbnail(const QByteArray &ba) {
    if (!ba.size())
        return;
    thumbnailData = ba;
    dirty = true;
}

void ChannelMetadata::saveFile() {
    if (!dirty)
        return;
    dirty = false;

    QVariantMap m;
    m["name"] = name;
    m["id"] = id;
    m["creationDate"] = creationDate;
    m["vendor"] = Platform::toString(vendor);
    if (thumbnailData.size())
        m["thumbnail"] = QString::fromLatin1(thumbnailData.toBase64());

    QJsonDocument d = QJsonDocument::fromVariant(m);
    QFile f(filePath());

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Failed opening "<<filePath() << " for writing.";
        return;
    }
    f.write(d.toJson());
    f.close();
}

void ChannelMetadata::loadFile() {
    QFile f(filePath());
    if (!f.exists()) {
        qWarning() << f.fileName() << "does not exist.";
        return;
    }
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed opening file " << f.fileName()<< " for reading.";
        return;
    }
    QString data = f.readAll();
    f.close();

    QJsonDocument d = QJsonDocument::fromJson(data.toUtf8());
    QVariant v = d.toVariant();
    QVariantMap m = v.toMap();

    name = m.value("name").toString();
    id = m.value("id").toString();
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
