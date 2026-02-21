/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2015-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#include "abstractscreenshotgrabber.h"

AbstractScreenshotGrabber::AbstractScreenshotGrabber(QObject* parent)
    : QObject(parent)
{
}

AbstractScreenshotGrabber::~AbstractScreenshotGrabber() = default;
