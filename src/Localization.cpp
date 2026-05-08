#include "Localization.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>

Localization *Localization::m_instance = nullptr;

Localization::Localization(QObject *parent)
    : QObject(parent), m_language("en") {
}

Localization *Localization::instance() {
    if (!m_instance) {
        m_instance = new Localization();
    }
    return m_instance;
}

Localization *Localization::create(QQmlEngine *engine, QJSEngine *scriptEngine) {
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    return instance();
}

QString Localization::tr(const QString &key) const {
    if (m_language == "en") {
        return key;
    }
    return m_strings.value(key, key);
}

QStringList Localization::availableLanguages() const {
    QDir dir(":/assets/i18n/");
    QStringList langs;
    for (const QString &file : dir.entryList({"*.json"}, QDir::Files)) {
        langs << file.left(file.length() - 5); // strip ".json"
    }
    langs.sort();
    return langs;
}

QString Localization::displayName(const QString &langCode) const {
    QFile file(":/assets/i18n/" + langCode + ".json");
    if (!file.open(QIODevice::ReadOnly)) {
        return langCode;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isObject()) {
        return doc.object().value("lang.display_name").toString(langCode);
    }
    return langCode;
}

QString Localization::language() const {
    return m_language;
}

void Localization::setLanguage(const QString &lang) {
    if (m_language == lang) return;
    m_language = lang;
    load(lang);
    emit languageChanged();
}

void Localization::load(const QString &lang) {
    m_strings.clear();
    if (lang == "en") {
        return; // English is the fallback, no JSON needed
    }
    QFile file(":/assets/i18n/" + lang + ".json");
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            m_strings[it.key()] = it.value().toString();
        }
    }
}
