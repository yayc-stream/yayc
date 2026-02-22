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

#include "RequestInterceptor.h"

#include <QFile>
#include <QThreadPool>
#include <QDebug>

EasylistLoader::EasylistLoader(const QString &path, RequestInterceptor *interceptor)
    : m_path(path), m_interceptor(interceptor)
{
}

void EasylistLoader::run()
{
    QFile file(m_path);
    QString easyListTxt;

    if (!file.exists()) {
        qWarning() << "No easylist.txt file found at " << m_path;
    } else {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            easyListTxt = file.readAll();
            file.close();
        } else {
            qWarning() << "Failed opening file "<<file.fileName()<< " for reading.";
        }
    }
    if (m_interceptor)
        m_interceptor->m_loading.storeRelease(0);
}

RequestInterceptor::RequestInterceptor(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
}

RequestInterceptor::~RequestInterceptor()
{
    // Wait for any running EasylistLoader to finish
    while (m_loading.loadAcquire() == 1) {
        QThreadPool::globalInstance()->waitForDone();
    }
}

void RequestInterceptor::setEasyListPath(QString newPath)
{
    if (!m_easyListPath.isEmpty() || m_loading.loadAcquire() == 1)
        return;

    if (newPath.startsWith("file://")) {
        newPath = newPath.mid(7);
#if defined(Q_OS_WINDOWS)
        if (newPath[0] == '/')
            newPath = newPath.mid(1);
#endif
    }
    m_easyListPath = newPath;

    EasylistLoader *loader = new EasylistLoader(newPath, this);
    loader->setAutoDelete(true);
    m_loading.storeRelease(1);
    QThreadPool::globalInstance()->start(loader);
}

void RequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    Q_UNUSED(info)
    if (m_loading.loadAcquire() == 1)
        return;
}
