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

#ifndef THUMBNAILIMAGEPROVIDER_H
#define THUMBNAILIMAGEPROVIDER_H

#include "Platform.h"

#include <QQuickImageProvider>
#include <QHash>
#include <QImage>
#include <QMutex>

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
        auto img = QImage::fromData(thumb);
        m_images[key] = std::move(img);
    }

    QImage requestImage(const QString &id,
                        QSize */*size*/,
                        const QSize &/*requestedSize*/) override
    {
        QMutexLocker locker(&m_mutex);

        if (m_images.contains(id))
            return m_images.value(id);

        return emptyImage;
    }
};

#endif // THUMBNAILIMAGEPROVIDER_H
