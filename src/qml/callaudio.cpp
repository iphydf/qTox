/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "callaudio.h"

#include <QtGlobal>

#ifdef Q_OS_ANDROID
#include <QJniObject>

#include <QtCore/qcoreapplication_platform.h>
#else
#include <tuple>
#endif

void CallAudio::setInCall(bool inCall)
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("io/github/qtox/qtox/CallAudio", "setInCall",
                                       "(Landroid/content/Context;Z)V",
                                       QNativeInterface::QAndroidApplication::context().object(),
                                       jboolean{inCall});
#else
    std::ignore = inCall;
#endif
}

void CallAudio::setLoudspeaker(bool on)
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("io/github/qtox/qtox/CallAudio", "setLoudspeaker",
                                       "(Landroid/content/Context;Z)V",
                                       QNativeInterface::QAndroidApplication::context().object(),
                                       jboolean{on});
#else
    std::ignore = on;
#endif
}
