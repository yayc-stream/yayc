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

#ifndef KEYINTERCEPTOR_H
#define KEYINTERCEPTOR_H

#include <QObject>
#include <QEvent>
#include <QKeyEvent>
#include <QGuiApplication>

class KeyInterceptor : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playerActive READ playerActive WRITE setPlayerActive NOTIFY playerActiveChanged)

public:
    explicit KeyInterceptor(QObject *parent = nullptr) : QObject(parent) {}

    bool playerActive() const { return m_playerActive; }
    void setPlayerActive(bool active) {
        if (m_playerActive != active) {
            m_playerActive = active;
            emit playerActiveChanged();
        }
    }

    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() != QEvent::KeyPress || !m_playerActive)
            return false;

        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() != Qt::Key_Left && keyEvent->key() != Qt::Key_Right)
            return false;

        // Don't intercept if a text input has focus
        auto *focused = QGuiApplication::focusObject();
        if (focused) {
            QString className = QString::fromLatin1(focused->metaObject()->className());
            if (className.contains(QLatin1String("TextInput"))
                || className.contains(QLatin1String("TextArea"))
                || className.contains(QLatin1String("TextEdit")))
                return false;
        }

        int delta = (keyEvent->key() == Qt::Key_Right) ? 5 : -5;
        emit seekRequested(delta);
        return true;
    }

signals:
    void playerActiveChanged();
    void seekRequested(int deltaSec);

private:
    bool m_playerActive = false;
};

#endif // KEYINTERCEPTOR_H
