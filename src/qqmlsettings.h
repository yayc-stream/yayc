// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default
// Copyright (C) 2025 YAYC team <yaycteam@gmail.com>

#ifndef QQMLSETTINGS_P_H
#define QQMLSETTINGS_P_H


#include <QtCore/qobject.h>
#include <QtCore/qvariant.h>
#include <QtCore/qurl.h>
#include <QtQml/qqml.h>
#include <QtQml/qqmlparserstatus.h>

class QQmlSettingsPrivate;

class QQmlSettings : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_DECLARE_PRIVATE(QQmlSettings)

    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY categoryChanged FINAL)
    Q_PROPERTY(QUrl location READ location WRITE setLocation NOTIFY locationChanged FINAL)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged FINAL)

public:
    explicit QQmlSettings(QObject *parent = nullptr);
    ~QQmlSettings() override;

    QString category() const;
    void setCategory(const QString &category);

    QUrl location() const;
    void setLocation(const QUrl &location);

    bool loaded() const;

    Q_INVOKABLE QVariant value(const QString &key, const QVariant &defaultValue = {}) const;
    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);
    Q_INVOKABLE void sync();

Q_SIGNALS:
    void categoryChanged(const QString &arg);
    void locationChanged(const QUrl &arg);
    void loadedChanged();

protected:
    void timerEvent(QTimerEvent *event) override;

    void classBegin() override;
    void componentComplete() override;
    void load();

private:
    QScopedPointer<QQmlSettingsPrivate> d_ptr;

    Q_PRIVATE_SLOT(d_func(), void _q_propertyChanged())
};

#endif // QQMLSETTINGS_P_H
