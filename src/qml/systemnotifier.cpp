/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "systemnotifier.h"

#include <QImage>
#include <QString>

#ifdef Q_OS_ANDROID
#include <QBuffer>
#include <QJniEnvironment>
#include <QJniObject>

#include <QtCore/qcoreapplication_platform.h>
#else
#include <tuple>
#endif

void SystemNotifier::show(const QString& tag, const QString& title, const QString& text,
                          const QImage& icon, Actions actions)
{
#ifdef Q_OS_ANDROID
    QByteArray png;
    if (!icon.isNull()) {
        QBuffer buffer{&png};
        buffer.open(QIODevice::WriteOnly);
        icon.save(&buffer, "PNG");
    }

    const QJniEnvironment env;
    jbyteArray bytes = env->NewByteArray(static_cast<jsize>(png.size()));
    env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(png.size()),
                            reinterpret_cast<const jbyte*>(png.constData()));
    QJniObject::callStaticMethod<void>(
        "io/github/qtox/qtox/QtoxService", "showNotification",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[BI)V",
        QNativeInterface::QAndroidApplication::context().object(),
        QJniObject::fromString(tag).object<jstring>(), QJniObject::fromString(title).object<jstring>(),
        QJniObject::fromString(text).object<jstring>(), bytes, static_cast<jint>(actions));
    env->DeleteLocalRef(bytes);
#else
    std::ignore = tag;
    std::ignore = title;
    std::ignore = text;
    std::ignore = actions;
    std::ignore = icon;
#endif
}

void SystemNotifier::cancel(const QString& tag)
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("io/github/qtox/qtox/QtoxService", "cancelNotification",
                                       "(Landroid/content/Context;Ljava/lang/String;)V",
                                       QNativeInterface::QAndroidApplication::context().object(),
                                       QJniObject::fromString(tag).object<jstring>());
#else
    std::ignore = tag;
#endif
}

void SystemNotifier::setRecording(bool microphone, bool camera)
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("io/github/qtox/qtox/QtoxService", "setRecording", "(ZZ)V",
                                       jboolean{microphone}, jboolean{camera});
#else
    std::ignore = microphone;
    std::ignore = camera;
#endif
}

void SystemNotifier::showStatus(const QString& status)
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("io/github/qtox/qtox/QtoxService", "showStatus",
                                       "(Landroid/content/Context;Ljava/lang/String;)V",
                                       QNativeInterface::QAndroidApplication::context().object(),
                                       QJniObject::fromString(status).object<jstring>());
#else
    std::ignore = status;
#endif
}
