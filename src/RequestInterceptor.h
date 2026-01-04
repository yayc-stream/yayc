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

#ifndef REQUESTINTERCEPTOR_H
#define REQUESTINTERCEPTOR_H

#include <QtWebEngineCore/qwebengineurlrequestinterceptor.h>
#include <QAtomicInt>
#include <QRunnable>

class RequestInterceptor;

class EasylistLoader : public QRunnable
{
public:
    EasylistLoader(const QString &path, RequestInterceptor *interceptor);
    void run() override;

private:
    QString m_path;
    RequestInterceptor *m_interceptor;
};

class RequestInterceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    RequestInterceptor(QObject *parent = nullptr);

    Q_INVOKABLE void setEasyListPath(QString newPath);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

protected:
    QAtomicInt m_loading{0};
    QString m_easyListPath;

    friend class EasylistLoader;
};

#endif // REQUESTINTERCEPTOR_H
