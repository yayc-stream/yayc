#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

class QQmlEngine;
class QJSEngine;

class Localization : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

public:
    static Localization* instance();
    static Localization* create(QQmlEngine *engine, QJSEngine *scriptEngine);

    Q_INVOKABLE QString tr(const QString &key) const;
    Q_INVOKABLE QStringList availableLanguages() const;
    Q_INVOKABLE QString displayName(const QString &langCode) const;

    QString language() const;
    void setLanguage(const QString &lang);

signals:
    void languageChanged();

private:
    explicit Localization(QObject *parent = nullptr);
    void load(const QString &lang);

    static Localization *m_instance;
    QMap<QString, QString> m_strings;
    QString m_language;
};

#endif // LOCALIZATION_H
