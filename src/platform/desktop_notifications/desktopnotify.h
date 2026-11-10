/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include "src/model/inotifier.h"

#include <QObject>

#include <memory>

class QSystemTrayIcon;
class INotificationSettings;

/**
 * @brief The desktop's notifications: the system's through D-Bus, or the tray icon's.
 *
 * The desktop shows one at a time, each replacing the last, so tags only tell them apart.
 */
class DesktopNotify : public QObject, public INotifier
{
    Q_OBJECT

public:
    DesktopNotify(const INotificationSettings& settings, QObject* parent);
    ~DesktopNotify() override;
    DesktopNotify(const DesktopNotify&) = delete;
    DesktopNotify& operator=(const DesktopNotify&) = delete;
    DesktopNotify(DesktopNotify&&) = delete;
    DesktopNotify& operator=(DesktopNotify&&) = delete;

    void setIcon(QSystemTrayIcon* icon);

    void setListener(IListener* listener_) override;
    void show(const QString& tag, const NotificationData& data, Actions actions) override;
    void cancel(const QString& tag) override;
    void showStatus(const QString& status) override;
    void setRecording(bool microphone, bool camera) override;

private:
    void onClicked();

    struct Private;
    const std::unique_ptr<Private> d;
};
