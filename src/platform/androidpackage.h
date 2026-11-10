/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QtGlobal>

// Our Java classes, such as QtoxActivity and QtoxService, are in the Qt 6.12 Android package.
#if defined(Q_OS_ANDROID) && QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
#define QTOX_ANDROID_PACKAGE
#endif
