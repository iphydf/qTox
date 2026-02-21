/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2025-2026 The TokTok team.
 */

#include "screenshot.h"

#include <QtGlobal>

#if QT_CONFIG(dbus)
#include "screenshot_dbus.h"
#endif

#include "src/widget/tool/abstractscreenshotgrabber.h"

AbstractScreenshotGrabber* Platform::createScreenshotGrabber(QWidget* parent)
{
#if QT_CONFIG(dbus)
    return DBusScreenshotGrabber::create(parent);
#else
    std::ignore = parent;
    return nullptr;
#endif
}
