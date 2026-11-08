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

#ifndef TESTCONTROL_H
#define TESTCONTROL_H

#include <QObject>
#include <QString>
#include <functional>

class QJsonObject;
class QLocalServer;
class QQmlApplicationEngine;
class QQuickItem;

// Test builds only (YAYC_TEST_BUILD): an external script drives the app through
// a local socket, for the update test bench. One command per line, one JSON
// reply per line: {"ok": bool, "value": …} or {"ok": false, "error": "…"}.
//
//   get <obj>.<prop> | get <contextProperty>   enum properties give the key name
//   set <obj>.<prop> <json>
//   call <obj>.<method> [<json> …]             Q_INVOKABLE or QML function
//   wait <obj>.<prop> <value> <ms>             replies when get gives <value>
//   click <objectName>                         press + release at the item centre
//   exists | visible | enabled <objectName>
//
// <obj>: a QObject context property (updateManager, utilities) or a QML object
// with that objectName (the visible one first).
class TestControl : public QObject
{
    Q_OBJECT
public:
    explicit TestControl(QQmlApplicationEngine *engine, QObject *parent = nullptr);

    // A stale socket of a crashed run is replaced. False: not listening
    bool listen(const QString &name);
    // One command line; \a done gets the reply (later for wait)
    void execute(const QString &line, std::function<void(const QJsonObject &)> done);

private:
    QObject *findObject(const QString &name) const;
    QQuickItem *findItem(const QString &objectName) const;
    bool read(const QString &path, QVariant *value, QString *error) const;
    void waitFor(const QString &path, const QString &expected, int remainingMs,
                 std::function<void(const QJsonObject &)> done);

    QQmlApplicationEngine *m_engine;
    QLocalServer *m_server{nullptr};
};

#endif // TESTCONTROL_H
