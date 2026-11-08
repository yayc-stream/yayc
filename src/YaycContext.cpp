#include "YaycContext.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlContext>
#include <QDebug>

YaycContext::YaycContext(QQmlEngine &engine, QObject *parent)
    : QObject(parent)
{
    engine.rootContext()->setContextObject(this);
    QJSValue self = engine.newQObject(this);
    QJSValue factory = engine.evaluate(
        "(function(ctx) { return function(key) { return ctx.tr(key); }; })");
    m_trFunc = factory.call({self});
    Q_ASSERT(m_trFunc.isCallable());
}

const QJSValue &YaycContext::getUiTr() const
{
    return m_trFunc;
}

QString YaycContext::tr(const QString &key) const
{
    if (m_language == "en")
        return key;
    return m_strings.value(key, key);
}

void YaycContext::retranslate(const QString &lang)
{
    qDebug() << "[i18n] retranslate:" << lang;
    m_language = lang;
    m_strings.clear();
    if (lang != "en") {
        QFile file(":/assets/i18n/" + lang + ".json");
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "[i18n] failed to open" << file.fileName();
        } else {
            const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
            // Untranslated keys are added with "": tr() then falls back to the key
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                const QString value = it.value().toString();
                if (!value.isEmpty())
                    m_strings[it.key()] = value;
            }
            qDebug() << "[i18n] loaded" << m_strings.size() << "strings";
        }
    }
    emit uiTrChanged();
}
