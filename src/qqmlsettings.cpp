#include "qqmlsettings.h"

#include <QtQml/qjsvalue.h>
#include <QtQml/qqmlfile.h>
#include <QtQml/qqmlinfo.h>

#include <QtCore/qbasictimer.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qdebug.h>
#include <QtCore/qhash.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/qpointer.h>
#include <QtCore/qsettings.h>

using namespace std::chrono_literals;

using namespace Qt::StringLiterals;

Q_STATIC_LOGGING_CATEGORY(lcQmlSettings, "qt.core.settings")

static constexpr auto settingsWriteDelay = 500ms;

class QQmlSettingsPrivate
{
    Q_DISABLE_COPY_MOVE(QQmlSettingsPrivate)
    Q_DECLARE_PUBLIC(QQmlSettings)

public:
    QQmlSettingsPrivate() = default;
    ~QQmlSettingsPrivate() = default;

    QSettings *instance() const;

    void init();
    void reset();

    void load();
    void store();

    void _q_propertyChanged();
    QVariant readProperty(const QMetaProperty &property) const;

    QQmlSettings *q_ptr = nullptr;
    QBasicTimer timer;
    bool initialized = false;
    QString category = {};
    QUrl location = {};
    mutable QPointer<QSettings> settings = nullptr;
    QHash<const char *, QVariant> changedProperties = {};
};

QSettings *QQmlSettingsPrivate::instance() const
{
    if (settings)
        return settings;

    QQmlSettings *q = const_cast<QQmlSettings *>(q_func());
    settings = QQmlFile::isLocalFile(location)
#ifdef Q_OS_LINUX
            ? new QSettings(QQmlFile::urlToLocalFileOrQrc(location), QSettings::NativeFormat, q)
#else
            ? new QSettings(QQmlFile::urlToLocalFileOrQrc(location), QSettings::IniFormat, q)
#endif
            : new QSettings(q);

    if (settings->status() != QSettings::NoError) {
        // TODO: can't print out the enum due to the following error:
        // error: C2666: 'QQmlInfo::operator <<': 15 overloads have similar conversions
        qmlWarning(q) << "Failed to initialize QSettings instance. Status code is: " << int(settings->status());

        if (settings->status() == QSettings::AccessError) {
            QStringList missingIdentifiers = {};
            if (QCoreApplication::organizationName().isEmpty())
                missingIdentifiers.append(u"organizationName"_s);
            if (QCoreApplication::organizationDomain().isEmpty())
                missingIdentifiers.append(u"organizationDomain"_s);
            if (QCoreApplication::applicationName().isEmpty())
                missingIdentifiers.append(u"applicationName"_s);

            if (!missingIdentifiers.isEmpty())
                qmlWarning(q) << "The following application identifiers have not been set: " << missingIdentifiers;
        }

        return settings;
    }

    if (!category.isEmpty())
        settings->beginGroup(category);

    if (initialized)
        q->load();

    return settings;
}

void QQmlSettingsPrivate::init()
{
    if (initialized)
        return;
    QQmlSettings *q = const_cast<QQmlSettings *>(q_func());
    q->load();
    initialized = true;
    qCDebug(lcQmlSettings) << "QQmlSettings: stored at" << instance()->fileName();
}

void QQmlSettingsPrivate::reset()
{
    if (initialized && settings && !changedProperties.isEmpty())
        store();
    delete settings;
}

void QQmlSettingsPrivate::load()
{
    Q_Q(QQmlSettings);
    const QMetaObject *mo = q->metaObject();
    const int offset = QQmlSettings::staticMetaObject.propertyCount();
    const int count = mo->propertyCount();

    for (int i = offset; i < count; ++i) {
        QMetaProperty property = mo->property(i);
        const QString propertyName = QString::fromUtf8(property.name());

        const QVariant previousValue = readProperty(property);
        const QVariant currentValue = instance()->value(propertyName,
                                                        previousValue);

        if (!currentValue.isNull() && (!previousValue.isValid()
                || (currentValue.canConvert(previousValue.metaType())
                    && previousValue != currentValue))) {
            property.write(q, currentValue);
            qCDebug(lcQmlSettings) << "QQmlSettings: load" << property.name() << "setting:" << currentValue << "default:" << previousValue;
        }

        // ensure that a non-existent setting gets written
        // even if the property wouldn't change later
        if (!instance()->contains(propertyName))
            _q_propertyChanged();

        // setup change notifications on first load
        if (!initialized && property.hasNotifySignal()) {
            static const int propertyChangedIndex = mo->indexOfSlot("_q_propertyChanged()");
            QMetaObject::connect(q, property.notifySignalIndex(), q, propertyChangedIndex);
        }
    }
}

void QQmlSettingsPrivate::store()
{
    QHash<const char *, QVariant>::const_iterator it = changedProperties.constBegin();
    while (it != changedProperties.constEnd()) {
        instance()->setValue(QString::fromUtf8(it.key()), it.value());
        qCDebug(lcQmlSettings) << "QQmlSettings: store" << it.key() << ":" << it.value();
        ++it;
    }
    changedProperties.clear();
}

void QQmlSettingsPrivate::_q_propertyChanged()
{
    Q_Q(QQmlSettings);
    const QMetaObject *mo = q->metaObject();
    const int offset = QQmlSettings::staticMetaObject.propertyCount() ;
    const int count = mo->propertyCount();
    for (int i = offset; i < count; ++i) {
        const QMetaProperty &property = mo->property(i);
        const QVariant value = readProperty(property);
        changedProperties.insert(property.name(), value);
        qCDebug(lcQmlSettings) << "QQmlSettings: cache" << property.name() << ":" << value;
    }
    timer.start(settingsWriteDelay, q);
}

QVariant QQmlSettingsPrivate::readProperty(const QMetaProperty &property) const
{
    Q_Q(const QQmlSettings);
    QVariant var = property.read(q);
    if (var.metaType() == QMetaType::fromType<QJSValue>())
        var = var.value<QJSValue>().toVariant();
    return var;
}

QQmlSettings::QQmlSettings(QObject *parent)
    : QObject(parent), d_ptr(new QQmlSettingsPrivate)
{
    Q_D(QQmlSettings);
    d->q_ptr = this;
}

QQmlSettings::~QQmlSettings()
{
    Q_D(QQmlSettings);
    d->reset(); // flush pending changes
}

QString QQmlSettings::category() const
{
    Q_D(const QQmlSettings);
    return d->category;
}

void QQmlSettings::setCategory(const QString &category)
{
    Q_D(QQmlSettings);
    if (d->category == category)
        return;
    d->reset();
    d->category = category;
    if (d->initialized)
        load();
    Q_EMIT categoryChanged(category);
}

QUrl QQmlSettings::location() const
{
    Q_D(const QQmlSettings);
    return d->location;
}

void QQmlSettings::setLocation(const QUrl &location)
{
    Q_D(QQmlSettings);
    if (d->location == location)
        return;
    d->reset();
    d->location = location;
    if (d->initialized)
        d->load();
    Q_EMIT locationChanged(location);
}

bool QQmlSettings::loaded() const
{
    Q_D(const QQmlSettings);
    return d->initialized && d->instance();
}

QVariant QQmlSettings::value(const QString &key, const QVariant &defaultValue) const
{
    Q_D(const QQmlSettings);
    return d->instance()->value(key, defaultValue);
}

void QQmlSettings::setValue(const QString &key, const QVariant &value)
{
    Q_D(const QQmlSettings);
    d->instance()->setValue(key, value);
    qCDebug(lcQmlSettings) << "QQmlSettings: setValue" << key << ":" << value;
}

void QQmlSettings::sync()
{
    Q_D(QQmlSettings);
    d->instance()->sync();
}

void QQmlSettings::classBegin()
{
}

void QQmlSettings::componentComplete()
{
    Q_D(QQmlSettings);
    d->init();
}

void QQmlSettings::load()
{
    Q_D(QQmlSettings);
    d->load();
    QMetaObject::invokeMethod(this, &QQmlSettings::loadedChanged, Qt::QueuedConnection);
}

void QQmlSettings::timerEvent(QTimerEvent *event)
{
    Q_D(QQmlSettings);
    QObject::timerEvent(event);
    if (!event->matches(d->timer))
        return;
    d->timer.stop();
    d->store();
}

#include "moc_qqmlsettings.cpp"
