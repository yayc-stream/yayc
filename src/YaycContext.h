#ifndef YAYCCONTEXT_H
#define YAYCCONTEXT_H

#include <QJSValue>
#include <QMap>
#include <QObject>
#include <QString>
#include <QQmlEngine>

class YaycContext : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJSValue uiTr READ getUiTr NOTIFY uiTrChanged)

public:
    explicit YaycContext(QQmlEngine &engine, QObject *parent = nullptr);

    Q_INVOKABLE QString tr(const QString &key) const;
    void retranslate(const QString &lang);

signals:
    void uiTrChanged();

private:
    const QJSValue &getUiTr() const;

    QJSValue m_trFunc;
    QMap<QString, QString> m_strings;
    QString m_language{"en"};
};

#endif // YAYCCONTEXT_H
