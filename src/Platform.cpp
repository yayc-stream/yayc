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

#include "Platform.h"

// Constants
const QString videoExtension{"yayc"};
const QString channelExtension{"yaycc"};
const QString shortsVideoPattern{"https://youtube.com/shorts/"};
const QString standardVideoPattern{"https://youtube.com/watch?v="};
const QString youtubeHomePattern{"https://youtube.com"};
const QString youtubeChannelPattern{"https://youtube.com/@"};
const QString repositoryURL{"https://github.com/yayc-stream/yayc"};
const QString latestReleaseVersionURL{"https://raw.githubusercontent.com/yayc-stream/yayc/master/APPVERSION"};
const QString donateURL{"https://raw.githubusercontent.com/yayc-stream/yayc/master/DONATE"};
const QImage emptyImage(1, 1, QImage::Format_RGB32);
const QRegularExpression allowedDirsPattern("^[^\\.].*");

// Helper functions
QByteArray appVersion() {
    QByteArray sversion(QT_STRINGIFY(APPVERSION));
    return sversion;
}

QString videoType(const QString &key) {
    return key.mid(3, 2);
}

bool isShorts(const QString &key) {
    return videoType(key).startsWith('s');
}

QString videoVendor(const QString &key) {
    return key.mid(0, 3);
}

QString videoID(const QString &key) {
    return key.mid(5);
}

QString channelID(const QString &key) {
    return key.mid(4);
}

QUrl removeWww(QUrl url) {
    QString host = url.host();
    if (host.startsWith(QLatin1String("www."))) {
        host = host.mid(4);
        url.setHost(host);
    }
    return url;
}

bool isExec(const QString &fileName) {
    QFileInfo check_file(fileName);
    if (check_file.exists() && check_file.isFile() && check_file.isExecutable())
        return true;
    return false;
}

QString avatarUrl(QString originalAvatarUrl) {
    return originalAvatarUrl.replace(QLatin1String("=s48-"), QLatin1String("=s128-"));
}

// Platform implementation
QString Platform::toString(const Vendor &v) {
    QMetaEnum metaEnum = QMetaEnum::fromType<Platform::Vendor>();
    return metaEnum.valueToKey(v);
}

Platform::Vendor Platform::toVendor(const QString &name) {
    static QMap<QString, Vendor> reverseLUT;
    if (reverseLUT.empty()) {
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

Platform::Vendor Platform::urlToVendor(const QUrl &url) {
    static QMap<QString, Vendor> LUT;
    if (LUT.isEmpty()) {
        LUT["youtube.com"] = Platform::YTB;
    }
    QString host = url.host();
    if (host.startsWith("www."))
        host = host.mid(4);
    if (!LUT.contains(host))
        return Platform::UNK;
    return LUT.value(host);
}

QString Platform::toUrl(const QString &key, qreal position) {
    auto vendor = Platform::toVendor(videoVendor(key));
    if (vendor == Platform::UNK) {
        return {};
    }
    if (vendor == Platform::YTB) {
        const QString &type = videoType(key);
        const QString &id = videoID(key);
        if (type.startsWith('s')) {
            return shortsVideoPattern + id;
        } else if (type.startsWith('v')) {
            return standardVideoPattern + id + ((position > 0.)
                                                ? "&t=" + QString::number(int(position)) + "s"
                                                : "");
        } else return {};
    }
    return {};
}
