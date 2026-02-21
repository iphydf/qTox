/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2025-2026 The TokTok team.
 */

#pragma once

class AbstractScreenshotGrabber;
class QWidget;

namespace Platform {
/**
 * @brief Create a platform-dependent screenshot grabber.
 *
 * If no platform-specific screenshot grabber is available, this function returns nullptr.
 * The caller should then create a default screenshot grabber.
 *
 * @param parent The parent widget for the screenshot grabber.
 */
AbstractScreenshotGrabber* createScreenshotGrabber(QWidget* parent);
} // namespace Platform
