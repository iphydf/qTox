/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "systeminsetsforeign.h"

#include <QJSEngine>
#include <QQmlEngine>

#include <tuple>

SystemInsets* SystemInsetsForeign::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    std::ignore = qmlEngine;
    std::ignore = jsEngine;
    // The app owns it, not QML.
    QJSEngine::setObjectOwnership(SystemInsets::current(), QJSEngine::CppOwnership);
    return SystemInsets::current();
}
