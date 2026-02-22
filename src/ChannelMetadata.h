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

#ifndef CHANNELMETADATA_H
#define CHANNELMETADATA_H

#include "Platform.h"

#include <QString>
#include <QPair>
#include <QDir>
#include <QByteArray>
#include <QDateTime>

struct ChannelMetadata
{
    using KeyType = QPair<QString, Platform::Vendor>; // Id, vendor

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
                                  const QDir &parent);

    ChannelMetadata();
    ChannelMetadata(const QString &key, const QDir &parent);

    bool hasThumbnail() const;
    QString key() const;
    static QString key(const QString &id, const Platform::Vendor vendor);
    static KeyType fromKeyString(const QString &key);
    QString filePath() const;
    void setName(const QString &n);
    void setThumbnail(const QByteArray &ba);
    void saveFile();
    void loadFile();
};

#endif // CHANNELMETADATA_H
