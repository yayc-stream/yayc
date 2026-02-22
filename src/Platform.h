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

#ifndef PLATFORM_H
#define PLATFORM_H

#include <QObject>
#include <QUrl>
#include <QString>
#include <QMap>
#include <QMetaEnum>
#include <QFileInfo>
#include <QLatin1String>
#include <QImage>
#include <QRegularExpression>

// Constants
extern const QString videoExtension;
extern const QString channelExtension;
extern const QString shortsVideoPattern;
extern const QString standardVideoPattern;
extern const QString youtubeHomePattern;
extern const QString youtubeChannelPattern;
extern const QString repositoryURL;
extern const QString latestReleaseVersionURL;
extern const QString donateURL;
extern const QImage emptyImage;
extern const QRegularExpression allowedDirsPattern;

// Helper functions
QString videoType(const QString &key);
bool isShorts(const QString &key);
QString videoVendor(const QString &key);
QString videoID(const QString &key);
QString channelID(const QString &key);
QUrl removeWww(QUrl url);
bool isExec(const QString &fileName);
QString avatarUrl(QString originalAvatarUrl);
QByteArray appVersion();

class Platform : public QObject {
    Q_OBJECT
public:
    enum Vendor
    {
        UNK = 0x0,
        YTB         // YouTube
    };
    Q_ENUM(Vendor)

    static QString toString(const Vendor &v);
    static Vendor toVendor(const QString &name);
    static Vendor urlToVendor(const QUrl &url);
    static QString toUrl(const QString &key, qreal position = 0.);
};

#endif // PLATFORM_H
