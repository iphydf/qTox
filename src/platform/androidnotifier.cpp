/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "androidnotifier.h"

#include "src/model/notificationdata.h"
#include "src/platform/androidpackage.h"

#include <QString>

#ifdef QTOX_ANDROID_PACKAGE
#include <QBuffer>
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>

#include <QtCore/qcoreapplication_platform.h>
#include <atomic>
#endif

#include <tuple>

namespace {
#ifdef QTOX_ANDROID_PACKAGE
// The newest notifier, which gets the user's answers.
std::atomic<AndroidNotifier*> instance{nullptr};

/** Hands @p answer the listener on Qt's thread, if there still is one then. */
template <typename Answer>
void answer(Answer answer)
{
    QMetaObject::invokeMethod(
        qApp,
        [answer] {
            if (INotifier::IListener* listener = AndroidNotifier::currentListener()) {
                answer(*listener);
            }
        },
        Qt::QueuedConnection);
}

// On Android's main thread: what the user wrote in a message's notification.
void nativeReply(JNIEnv* env, jclass cls, jstring tag, jstring text)
{
    std::ignore = env;
    std::ignore = cls;
    answer([tag = QJniObject{tag}.toString(), text = QJniObject{text}.toString()](
               INotifier::IListener& listener) { listener.replied(tag, text); });
}

// On Android's main thread: the user declined a call from its notification.
void nativeDecline(JNIEnv* env, jclass cls, jstring tag)
{
    std::ignore = env;
    std::ignore = cls;
    answer([tag = QJniObject{tag}.toString()](INotifier::IListener& listener) {
        listener.declined(tag);
    });
}
#endif
} // namespace

AndroidNotifier::AndroidNotifier()
{
#ifdef QTOX_ANDROID_PACKAGE
    instance = this;
    const JNINativeMethod methods[] = {
        {"nativeReply", "(Ljava/lang/String;Ljava/lang/String;)V", reinterpret_cast<void*>(nativeReply)},
        {"nativeDecline", "(Ljava/lang/String;)V", reinterpret_cast<void*>(nativeDecline)},
    };
    QJniEnvironment{}.registerNativeMethods("io/github/qtox/qtox/NotificationReceiver", methods,
                                            std::size(methods));
#endif
}

AndroidNotifier::~AndroidNotifier()
{
#ifdef QTOX_ANDROID_PACKAGE
    AndroidNotifier* self = this;
    instance.compare_exchange_strong(self, nullptr);
#endif
}

INotifier::IListener* AndroidNotifier::currentListener()
{
#ifdef QTOX_ANDROID_PACKAGE
    AndroidNotifier* notifier = instance;
    return notifier != nullptr ? notifier->listener : nullptr;
#else
    return nullptr;
#endif
}

void AndroidNotifier::setListener(IListener* listener_)
{
    listener = listener_;
}

void AndroidNotifier::show(const QString& tag, const NotificationData& data, Actions actions)
{
#ifdef QTOX_ANDROID_PACKAGE
    QByteArray png;
    if (!data.image.isNull()) {
        QBuffer buffer{&png};
        buffer.open(QIODevice::WriteOnly);
        data.image.save(&buffer, "PNG");
    }

    const QJniEnvironment env;
    jbyteArray bytes = env->NewByteArray(static_cast<jsize>(png.size()));
    env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(png.size()),
                            reinterpret_cast<const jbyte*>(png.constData()));
    QJniObject::callStaticMethod<void>(
        "io/github/qtox/qtox/QtoxService", "showNotification",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[BI)V",
        QNativeInterface::QAndroidApplication::context().object(),
        QJniObject::fromString(tag).object<jstring>(),
        QJniObject::fromString(data.title).object<jstring>(),
        QJniObject::fromString(data.message).object<jstring>(), bytes, static_cast<jint>(actions));
    env->DeleteLocalRef(bytes);
#else
    std::ignore = tag;
    std::ignore = data;
    std::ignore = actions;
#endif
}

void AndroidNotifier::cancel(const QString& tag)
{
#ifdef QTOX_ANDROID_PACKAGE
    QJniObject::callStaticMethod<void>("io/github/qtox/qtox/QtoxService", "cancelNotification",
                                       "(Landroid/content/Context;Ljava/lang/String;)V",
                                       QNativeInterface::QAndroidApplication::context().object(),
                                       QJniObject::fromString(tag).object<jstring>());
#else
    std::ignore = tag;
#endif
}

void AndroidNotifier::setRecording(bool microphone, bool camera)
{
#ifdef QTOX_ANDROID_PACKAGE
    QJniObject::callStaticMethod<void>("io/github/qtox/qtox/QtoxService", "setRecording", "(ZZ)V",
                                       jboolean{microphone}, jboolean{camera});
#else
    std::ignore = microphone;
    std::ignore = camera;
#endif
}

void AndroidNotifier::showStatus(const QString& status)
{
#ifdef QTOX_ANDROID_PACKAGE
    QJniObject::callStaticMethod<void>("io/github/qtox/qtox/QtoxService", "showStatus",
                                       "(Landroid/content/Context;Ljava/lang/String;)V",
                                       QNativeInterface::QAndroidApplication::context().object(),
                                       QJniObject::fromString(status).object<jstring>());
#else
    std::ignore = status;
#endif
}
