/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "androidintents.h"

#include "src/platform/androidpackage.h"

#include <QUrl>

#ifdef QTOX_ANDROID_PACKAGE
#include <QJniObject>

#include <QtCore/private/qjnihelpers_p.h>
#include <QtCore/qcoreapplication_platform.h>
#include <tuple>

struct AndroidIntents::Listener : QtAndroidPrivate::NewIntentListener
{
    explicit Listener(AndroidIntents& intents_)
        : intents{intents_}
    {
        // A tap or a share that started the user interface.
        open(QJniObject{QNativeInterface::QAndroidApplication::context()}
                 .callObjectMethod("getIntent", "()Landroid/content/Intent;"));
        QtAndroidPrivate::registerNewIntentListener(this);
    }

    ~Listener() override
    {
        QtAndroidPrivate::unregisterNewIntentListener(this);
    }

    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener(Listener&&) = delete;
    Listener& operator=(Listener&&) = delete;

    // On Android's main thread.
    bool handleNewIntent(JNIEnv* env, jobject intent) override
    {
        std::ignore = env;
        return open(QJniObject{intent});
    }

    /** Emits on Qt's thread what @p intent asks for, if anything. */
    bool open(const QJniObject& intent)
    {
        const QString destination = ask("destination", intent);
        const QString file = ask("sharedFile", intent);
        const QString text = ask("sharedText", intent);
        AndroidIntents* receiver = &intents;
        if (!destination.isEmpty()) {
            QMetaObject::invokeMethod(
                receiver, [receiver, destination] { emit receiver->openRequested(destination); },
                Qt::QueuedConnection);
            return true;
        }
        if (!file.isEmpty() || !text.isEmpty()) {
            QMetaObject::invokeMethod(
                receiver, [receiver, file, text] { emit receiver->shareReceived(QUrl{file}, text); },
                Qt::QueuedConnection);
            return true;
        }
        return false;
    }

    static QString ask(const char* method, const QJniObject& intent)
    {
        return QJniObject::callStaticMethod<jstring>("io/github/qtox/qtox/QtoxActivity", method,
                                                     "(Landroid/content/Intent;)Ljava/lang/String;",
                                                     intent.object())
            .toString();
    }

    AndroidIntents& intents;
};
#else
struct AndroidIntents::Listener
{};
#endif

AndroidIntents::AndroidIntents(QObject* parent)
    : QObject(parent)
#ifdef QTOX_ANDROID_PACKAGE
    , listener{std::make_unique<Listener>(*this)}
#endif
{
}

AndroidIntents::~AndroidIntents() = default;
