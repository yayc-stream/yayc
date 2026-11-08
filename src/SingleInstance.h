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

#ifndef SINGLEINSTANCE_H
#define SINGLEINSTANCE_H

#include <QObject>
#include <QString>

#include <memory>

class QLocalServer;
class QLockFile;

// One process per configuration file, as Firefox per profile. A second
// start with the same file asks the first to show its window, then quits
class SingleInstance : public QObject {
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent = nullptr);
    ~SingleInstance() override;

    // True: this process owns \a configFile (<configFile>.instance.lock;
    // QSettings itself locks <configFile>.lock while it writes) until it is
    // destroyed. False: another one does, and was asked to show its window.
    // A busy owner that does not answer is quitting: its lock is awaited up
    // to \a waitMs
    bool claim(const QString &configFile, int waitMs = 30000);
    // At the quit: no more activations (nobody would show a window), the
    // lock goes to the caller. Held through the exit plan, released before
    // a restart relaunches
    std::unique_ptr<QLockFile> release();
    // Per user and file
    static QString serverName(const QString &configFile);

signals:
    // A second start with the same configuration
    void activationRequested();

private:
    bool listen(const QString &name);

    std::unique_ptr<QLockFile> m_lock;
    QLocalServer *m_server{nullptr};
};

#endif // SINGLEINSTANCE_H
