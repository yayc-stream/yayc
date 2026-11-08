/*
Copyright (C) 2023- YAYC team <info@yayc.stream>

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

#include "TestControl.h"

#include <QCoreApplication>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMetaProperty>
#include <QMouseEvent>
#include <QPointer>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
#include <QTimer>

namespace {

QJsonObject ok(const QJsonValue &value = QJsonValue())
{
    return {{QStringLiteral("ok"), true}, {QStringLiteral("value"), value}};
}

QJsonObject fail(const QString &error)
{
    return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
}

QJsonValue toJson(QVariant v)
{
    if (v.metaType() == QMetaType::fromType<QJSValue>())
        v = v.value<QJSValue>().toVariant();
    if (QObject *o = v.value<QObject *>())
        return o->objectName();
    return QJsonValue::fromVariant(v);
}

// The form wait compares: "true", "3", "ExitPending", "9.9.1"
QString toText(const QJsonValue &v)
{
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return v.toVariant().toString();
}

// A JSON literal (true, 5, "a b") or else the word itself as a string
QVariant parseArg(const QString &token)
{
    const QJsonDocument doc = QJsonDocument::fromJson(("[" + token + "]").toUtf8());
    if (doc.isArray() && doc.array().size() == 1)
        return doc.array().first().toVariant();
    return token;
}

bool isVisible(QObject *o)
{
    return o->property("visible").toBool();
}

} // namespace

TestControl::TestControl(QQmlApplicationEngine *engine, QObject *parent)
    : QObject(parent),
      m_engine(engine)
{
}

bool TestControl::listen(const QString &name)
{
    if (name.isEmpty())
        return false;
    QLocalServer::removeServer(name);
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(name))
        return false;
    connect(m_server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
                while (socket->canReadLine()) {
                    const QString line = QString::fromUtf8(socket->readLine()).trimmed();
                    if (line.isEmpty())
                        continue;
                    QPointer<QLocalSocket> guard(socket);
                    execute(line, [guard](const QJsonObject &reply) {
                        if (!guard)
                            return;
                        guard->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
                        guard->flush();
                    });
                }
            });
        }
    });
    return true;
}

QObject *TestControl::findObject(const QString &name) const
{
    const QVariant property = m_engine->rootContext()->contextProperty(name);
    if (QObject *o = property.value<QObject *>())
        return o;

    // QObject children and visual children: popups and their content live in
    // the window overlay, not under the item that declares them
    QObject *found = nullptr;
    QSet<QObject *> seen;
    std::function<void(QObject *)> visit = [&](QObject *o) {
        if (!o || seen.contains(o))
            return;
        seen.insert(o);
        if (o->objectName() == name && (!found || (!isVisible(found) && isVisible(o))))
            found = o;
        for (QObject *child : o->children())
            visit(child);
        if (auto *item = qobject_cast<QQuickItem *>(o)) {
            for (QQuickItem *child : item->childItems())
                visit(child);
        }
        if (auto *window = qobject_cast<QQuickWindow *>(o))
            visit(window->contentItem()->parentItem());
    };
    for (QObject *root : m_engine->rootObjects())
        visit(root);
    return found;
}

QQuickItem *TestControl::findItem(const QString &objectName) const
{
    return qobject_cast<QQuickItem *>(findObject(objectName));
}

bool TestControl::read(const QString &path, QVariant *value, QString *error) const
{
    const int dot = path.indexOf(QLatin1Char('.'));
    if (dot < 0) {
        *value = m_engine->rootContext()->contextProperty(path);
        if (!value->isValid())
            *error = QStringLiteral("no context property ") + path;
        return value->isValid();
    }
    const QString name = path.left(dot);
    QObject *o = findObject(name);
    if (!o) {
        *error = QStringLiteral("no object ") + name;
        return false;
    }
    const QByteArray prop = path.mid(dot + 1).toUtf8();
    const int index = o->metaObject()->indexOfProperty(prop.constData());
    if (index < 0) {
        *error = QStringLiteral("no property ") + path;
        return false;
    }
    const QMetaProperty mp = o->metaObject()->property(index);
    *value = mp.read(o);
    if (mp.isEnumType())
        *value = QString::fromLatin1(mp.enumerator().valueToKey(value->toInt()));
    return true;
}

void TestControl::waitFor(const QString &path, const QString &expected, int remainingMs,
                          std::function<void(const QJsonObject &)> done)
{
    QVariant value;
    QString error;
    const bool readable = read(path, &value, &error);
    const QString text = readable ? toText(toJson(value)) : QString();
    if (readable && text == expected) {
        done(ok(toJson(value)));
        return;
    }
    if (remainingMs <= 0) {
        done(fail(QStringLiteral("timeout: ") + path + QStringLiteral(" is ")
                  + (readable ? text : error)));
        return;
    }
    QTimer::singleShot(100, this, [this, path, expected, remainingMs, done]() {
        waitFor(path, expected, remainingMs - 100, done);
    });
}

void TestControl::execute(const QString &line, std::function<void(const QJsonObject &)> done)
{
    const QStringList t = QProcess::splitCommand(line);
    const QString cmd = t.value(0);

    if (cmd == QLatin1String("get") && t.size() == 2) {
        QVariant value;
        QString error;
        done(read(t.at(1), &value, &error) ? ok(toJson(value)) : fail(error));
        return;
    }
    if (cmd == QLatin1String("wait") && t.size() == 4) {
        waitFor(t.at(1), t.at(2), t.at(3).toInt(), done);
        return;
    }

    const QString target = t.value(1);
    const int dot = target.indexOf(QLatin1Char('.'));
    if ((cmd == QLatin1String("set") && t.size() == 3)
            || (cmd == QLatin1String("call") && t.size() >= 2)) {
        QObject *o = dot > 0 ? findObject(target.left(dot)) : nullptr;
        if (!o) {
            done(fail(QStringLiteral("no object for ") + target));
            return;
        }
        const QString member = target.mid(dot + 1);
        if (cmd == QLatin1String("set")) {
            const bool written = o->setProperty(member.toUtf8().constData(), parseArg(t.at(2)));
            done(written ? ok() : fail(QStringLiteral("cannot set ") + target));
            return;
        }
        // Through the JS engine: it converts the arguments for C++ and QML alike.
        // Explicit: newQObject() hands a parentless C++ object (a stack one in
        // the tests) to the garbage collector, which would delete it
        if (QJSEngine::objectOwnership(o) == QJSEngine::CppOwnership)
            QJSEngine::setObjectOwnership(o, QJSEngine::CppOwnership);
        QJSValue self = m_engine->newQObject(o);
        const QJSValue fn = self.property(member);
        if (!fn.isCallable()) {
            done(fail(QStringLiteral("no method ") + target));
            return;
        }
        QJSValueList args;
        for (int i = 2; i < t.size(); ++i)
            args << m_engine->toScriptValue(parseArg(t.at(i)));
        const QJSValue result = fn.callWithInstance(self, args);
        done(result.isError() ? fail(result.toString()) : ok(toJson(result.toVariant())));
        return;
    }

    if (t.size() != 2) {
        done(fail(QStringLiteral("bad command: ") + line));
        return;
    }
    QObject *o = findObject(target);
    if (cmd == QLatin1String("exists")) {
        done(ok(o != nullptr));
        return;
    }
    if (!o) {
        done(fail(QStringLiteral("no object ") + target));
        return;
    }
    if (cmd == QLatin1String("visible") || cmd == QLatin1String("enabled")) {
        done(ok(o->property(cmd.toLatin1().constData()).toBool()));
        return;
    }
    if (cmd == QLatin1String("click")) {
        auto *item = qobject_cast<QQuickItem *>(o);
        if (!item || !item->window()) {
            done(fail(target + QStringLiteral(" is not a visual item")));
            return;
        }
        if (!item->isVisible() || !item->isEnabled()) {
            done(fail(target + QStringLiteral(" is not visible and enabled")));
            return;
        }
        // Real events into the window: whatever is on top at that point gets them
        QQuickWindow *window = item->window();
        const QPointF scene = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
        const QPointF global = window->mapToGlobal(scene);
        QMouseEvent press(QEvent::MouseButtonPress, scene, scene, global,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, scene, scene, global,
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &release);
        done(ok());
        return;
    }
    done(fail(QStringLiteral("bad command: ") + line));
}
