/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "keepalive.h"

#include "src/platform/androidpackage.h"

#ifdef QTOX_ANDROID_PACKAGE
#include <QJniObject>

#include <QtCore/qcoreapplication_platform.h>
#endif

Platform::KeepAlive::KeepAlive()
{
#ifdef QTOX_ANDROID_PACKAGE
    QJniObject::callStaticMethod<void>("io/github/qtox/qtox/KeepAliveService", "start",
                                       "(Landroid/app/Activity;)V",
                                       QNativeInterface::QAndroidApplication::context().object());
#endif
}

Platform::KeepAlive::~KeepAlive()
{
#ifdef QTOX_ANDROID_PACKAGE
    QJniObject::callStaticMethod<void>("io/github/qtox/qtox/KeepAliveService", "stop",
                                       "(Landroid/content/Context;)V",
                                       QNativeInterface::QAndroidApplication::context().object());
#endif
}
