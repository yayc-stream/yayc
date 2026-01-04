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

#include "VideoMetadata.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QBuffer>

VideoMetadata::~VideoMetadata() {
    if (!erased)
        saveFile();
}

VideoMetadata::VideoMetadata() {}

VideoMetadata::VideoMetadata(const QString &k, const QDir &p)
    : key(k), parent(p) {
    vendor = Platform::toVendor(videoVendor(key));
    creationDate = QDateTime::currentDateTimeUtc();
}

void VideoMetadata::setDuration(qreal d) {
    if (d == duration)
        return;
    duration = d;
    dirty = true;
}

void VideoMetadata::setPosition(qreal p) {
    if (p == position)
        return;
    auto oldPosition = position;

    // Don't rewind shorts so they look completed on the bookmark view
    if (isShorts(key) && viewed && p < oldPosition)
        return;

    position = p;
    dirty = true;
    const auto threshold = duration * 0.9;

    if (duration > 3. && position > threshold && oldPosition <= threshold) {
        // set only when trespassing threshold
        viewed = true;
    }
}

void VideoMetadata::setViewed(bool v) {
    if (viewed == v)
        return;
    viewed = v;
    dirty = true;
}

void VideoMetadata::setStarred(bool s) {
    if (starred == s)
        return;
    starred = s;
    dirty = true;
}

bool VideoMetadata::setTitle(const QString &t) {
    if (title == t)
        return false;
    title = t;
    dirty = true;
    return true;
}

bool VideoMetadata::setChannelID(const QString &cid) {
    if (channelID == cid)
        return false;
    channelID = cid;
    dirty = true;
    return true;
}

bool VideoMetadata::update(const QString &t, qreal p, qreal d) {
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

bool VideoMetadata::moveLocation(const QDir &d) {
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

bool VideoMetadata::eraseFile() {
    QFile f(filePath());
    erased = true;
    return f.remove();
}

void VideoMetadata::setThumbnail(const QByteArray &ba) {
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

const QByteArray &VideoMetadata::thumbnail() const {
    return thumbnailData;
}

void VideoMetadata::saveFile() {
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

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Failed opening file "<<f.fileName()<< " for writing.";
        return;
    }
    f.write(d.toJson());
    f.close();
}

void VideoMetadata::loadFile() {
    QFile f(filePath());
    if (!f.exists())
        return;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed opening file " << f.fileName()<< " for reading.";
        return;
    }
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

QString VideoMetadata::filePath() const {
    return parent.absoluteFilePath(key + "." + videoExtension);
}

QUrl VideoMetadata::url(bool startingTime) const {
    return Platform::toUrl(key, (position > 0. && startingTime) ? position : 0);
}
