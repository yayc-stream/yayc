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

#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
}

SingleInstance::~SingleInstance()
{
    // Before the lock: a closing Unix server removes its socket file, which
    // a new owner may have made already
    delete m_server;
}

std::unique_ptr<QLockFile> SingleInstance::release()
{
    delete m_server;
    m_server = nullptr;
    return std::move(m_lock);
}

QString SingleInstance::serverName(const QString &configFile)
{
    QString id = QDir::cleanPath(QFileInfo(configFile).absoluteFilePath());
#ifdef Q_OS_WIN
    id = id.toLower(); // a shortcut may spell the path in another case
#endif
    // Unix: a socket in /tmp, Windows: a pipe; both shared by all users
    const QString user = qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME"));
    return QLatin1String("yayc-")
            + QString::fromLatin1(QCryptographicHash::hash((user + QLatin1Char('\n') + id).toUtf8(),
                                                           QCryptographicHash::Sha1)
                                  .toHex().left(16));
}

bool SingleInstance::claim(const QString &configFile, int waitMs)
{
    const QString name = serverName(configFile);
    QDir().mkpath(QFileInfo(configFile).absolutePath());
    m_lock = std::make_unique<QLockFile>(configFile + QLatin1String(".instance.lock"));
    m_lock->setStaleLockTime(0); // only a dead owner makes it stale
    if (!m_lock->tryLock(0)) {
        // Unwritable dir (or else): run unguarded rather than not at all
        if (m_lock->error() != QLockFile::LockFailedError) {
            m_lock.reset();
            return true;
        }
        QLocalSocket owner;
        owner.connectToServer(name);
        if (owner.waitForConnected(2000)) {
            owner.disconnectFromServer(); // the connection is the request
            m_lock.reset();
            return false;
        }
        if (!m_lock->tryLock(waitMs)) {
            m_lock.reset();
            return false;
        }
    }
    return listen(name);
}

bool SingleInstance::listen(const QString &name)
{
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(name); // Unix: the socket file of a crashed owner
    connect(m_server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket *s = m_server->nextPendingConnection()) {
            s->disconnectFromServer();
            s->deleteLater();
            emit activationRequested();
        }
    });
    m_server->listen(name); // without it: no activation, the guard still holds
    return true;
}
