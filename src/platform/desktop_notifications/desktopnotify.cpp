/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2024-2026 The TokTok team.
 */

#include "desktopnotify.h"

#include "desktopnotifybackend.h"

#include "src/model/notificationdata.h"
#include "src/persistence/inotificationsettings.h"

#include <QDebug>
#include <QPixmap>
#include <QSystemTrayIcon>

#include <tuple>

struct DesktopNotify::Private
{
    const INotificationSettings& settings;
    QSystemTrayIcon* icon;
    DesktopNotifyBackend* dbus;
    IListener* listener;
};

DesktopNotify::DesktopNotify(const INotificationSettings& settings, QObject* parent)
    : QObject(parent)
    , d(std::make_unique<Private>(Private{
          settings,
          nullptr,
          new DesktopNotifyBackend(this),
          nullptr,
      }))
{
    connect(d->dbus, &DesktopNotifyBackend::messageClicked, this, &DesktopNotify::onClicked);
}

DesktopNotify::~DesktopNotify() = default;

void DesktopNotify::setIcon(QSystemTrayIcon* icon)
{
    if (d->icon != nullptr) {
        disconnect(d->icon, &QSystemTrayIcon::messageClicked, this, &DesktopNotify::onClicked);
    }
    d->icon = icon;
    if (d->icon != nullptr) {
        connect(d->icon, &QSystemTrayIcon::messageClicked, this, &DesktopNotify::onClicked);
    }
}

void DesktopNotify::setListener(IListener* listener_)
{
    d->listener = listener_;
}

void DesktopNotify::onClicked()
{
    if (d->listener != nullptr) {
        d->listener->opened();
    }
}

void DesktopNotify::show(const QString& tag, const NotificationData& data, Actions actions)
{
    // Nothing tells them apart, and the desktop has no buttons for them.
    std::ignore = tag;
    std::ignore = actions;

    // Try system-backends first.
    if (d->settings.getNotifySystemBackend()) {
        if (d->dbus->showMessage(data.title, data.message, data.category,
                                 QPixmap::fromImage(data.image))) {
            return;
        }
    }

    if (!QSystemTrayIcon::supportsMessages()) {
        qWarning() << "System does not support notifications";
        return;
    }

    if (d->icon == nullptr) {
        qWarning() << "System tray not yet initialised";
        return;
    }

    // Fallback to QSystemTrayIcon.
    d->icon->showMessage(data.title,
                         d->dbus->supportsBodyMarkup() ? data.message.toHtmlEscaped() : data.message,
                         QPixmap::fromImage(data.image));
}

void DesktopNotify::cancel(const QString& tag)
{
    // The next one replaces it.
    std::ignore = tag;
}

void DesktopNotify::showStatus(const QString& status)
{
    // Desktops keep running without one.
    std::ignore = status;
}

void DesktopNotify::setRecording(bool microphone, bool camera)
{
    // Desktops let apps record in the background.
    std::ignore = microphone;
    std::ignore = camera;
}
