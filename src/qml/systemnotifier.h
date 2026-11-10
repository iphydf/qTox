/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include "src/qml/inotifier.h"

/**
 * @brief Android's notifications, and none on the desktop.
 */
class SystemNotifier : public INotifier
{
public:
    void show(const QString& tag, const QString& title, const QString& text, const QImage& icon,
              Actions actions) override;
    void cancel(const QString& tag) override;
    void showStatus(const QString& status) override;
    void setRecording(bool microphone, bool camera) override;
};
