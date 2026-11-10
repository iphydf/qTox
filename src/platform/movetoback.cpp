/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "movetoback.h"

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

void Platform::moveToBack()
{
#ifdef Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject{QNativeInterface::QAndroidApplication::context()}
            .callMethod<jboolean>("moveTaskToBack", jboolean{true});
    });
#endif
}
