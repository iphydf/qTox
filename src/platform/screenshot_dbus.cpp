/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2025-2026 The TokTok team.
 */

#include "screenshot_dbus.h" // IWYU pragma: keep

#include <QtGlobal>

#if QT_CONFIG(dbus)
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDebug>
#include <QGuiApplication>
#include <QPixmap>
#include <QRect>
#include <QUrl>
#include <QWidget>

#include <memory>

namespace {
const QString PORTAL_DBUS_NAME = QStringLiteral("org.freedesktop.portal.Desktop");
const QString PORTAL_DBUS_CORE_OBJECT = QStringLiteral("/org/freedesktop/portal/desktop");
const QString PORTAL_DBUS_REQUEST_INTERFACE = QStringLiteral("org.freedesktop.portal.Request");
const QString PORTAL_DBUS_SCREENSHOT_INTERFACE =
    QStringLiteral("org.freedesktop.portal.Screenshot");

QString winIdString(WId winId)
{
    // If we're wayland, return wayland:$id.
    if (QGuiApplication::platformName() == QStringLiteral("wayland")) {
        return QStringLiteral("wayland:%1").arg(winId);
    }
    if (QGuiApplication::platformName() == QStringLiteral("xcb")) {
        return QStringLiteral("x11:0x%1").arg(winId, 0, 16);
    }
    qWarning() << "Unknown platform:" << QGuiApplication::platformName() << "cannot take screenshots";
    return {};
}
} // namespace

struct DBusScreenshotGrabber::Data
{
    QDBusInterface portal;
    WId winId;
    QString path;

    explicit Data(QWidget* parent)
        : portal(PORTAL_DBUS_NAME, PORTAL_DBUS_CORE_OBJECT, PORTAL_DBUS_SCREENSHOT_INTERFACE,
                 QDBusConnection::sessionBus(), parent)
        , winId{parent->topLevelWidget()->winId()}
    {
    }

    QString grabScreen()
    {
        if (!portal.connection().isConnected()) {
            qWarning() << "DBus connection failed";
            return {};
        }

        if (!portal.isValid()) {
            qWarning() << "Failed to connect to org.freedesktop.portal.Screenshot";
            return {};
        }

        const QString wid = winIdString(winId);
        if (wid.isEmpty()) {
            return {};
        }

        const QDBusMessage reply = portal.call(QStringLiteral("Screenshot"), wid,
                                               QVariantMap{
                                                   {"modal", true},
                                                   {"interactive", true},
                                                   {"handle_token", "1"},
                                               });

        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "Failed to take screenshot:" << reply.errorMessage();
            return {};
        }

        return reply.arguments().value(0).value<QDBusObjectPath>().path();
    }
};

DBusScreenshotGrabber::DBusScreenshotGrabber(QWidget* parent)
    : AbstractScreenshotGrabber(parent)
    , d(std::make_unique<Data>(parent))
{
}

DBusScreenshotGrabber::~DBusScreenshotGrabber() = default;

DBusScreenshotGrabber* DBusScreenshotGrabber::create(QWidget* parent)
{
    auto grabber = std::make_unique<DBusScreenshotGrabber>(parent);
    if (!grabber->isValid()) {
        return nullptr;
    }

    const QString path = grabber->d->grabScreen();
    if (path.isEmpty()) {
        // Some connection problem or unsupported platform. We're not trying this again.
        return nullptr;
    }

    qDebug() << "Opened screenshot dialog; waiting for response on" << path;
    QDBusConnection::sessionBus().connect(
        // org.freedesktop.portal.Request::Response
        PORTAL_DBUS_NAME, path, PORTAL_DBUS_REQUEST_INTERFACE, QStringLiteral("Response"),
        grabber.get(), SLOT(screenshotResponse(uint, QVariantMap)));

    return grabber.release();
}

bool DBusScreenshotGrabber::isValid() const
{
    return d->portal.isValid();
}

void DBusScreenshotGrabber::showGrabber()
{
    // Nothing to do here. We've done everything in create().
}

/**
 * https://docs.flatpak.org/en/latest/portal-api-reference.html#gdbus-signal-org-freedesktop-portal-Request.Response
 *
 * Emitted when the user interaction for a portal request is over.
 *
 * The response indicates how the user interaction ended:
 *
 * 0: Success, the request is carried out
 * 1: The user cancelled the interaction
 * 2: The user interaction was ended in some other way
 */
void DBusScreenshotGrabber::screenshotResponse(uint response, QVariantMap results)
{
    switch (response) {
    case 0: {
        const QUrl uri{results[QStringLiteral("uri")].toString()};
        qDebug() << "Screenshot taken:" << uri.toString();
        emit screenshotTaken(QPixmap(uri.toLocalFile()));
        break;
    }
    case 1:
        qDebug() << "User cancelled screenshot request";
        emit rejected();
        break;
    default:
        qWarning() << "Screenshot request failed:" << response;
        emit rejected();
        break;
    }

    // We're done, clean up.
    deleteLater();
}
#endif // QT_CONFIG(dbus)
