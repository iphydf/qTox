/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "touchinput.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QScroller>
#include <QScrollerProperties>
#include <QStyleHints>
#include <QWidget>

namespace {
void grabTouchScrolling(QAbstractScrollArea& area)
{
    QWidget* viewport = area.viewport();
    QScroller::grabGesture(viewport, QScroller::TouchGesture);

    // Stop at the ends like the rest of the widgets do, without bouncing.
    QScroller* scroller = QScroller::scroller(viewport);
    QScrollerProperties properties = scroller->scrollerProperties();
    const auto off = QVariant::fromValue(QScrollerProperties::OvershootAlwaysOff);
    properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, off);
    properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, off);
    scroller->setScrollerProperties(properties);
}

bool isTouch(const QMouseEvent& event)
{
    return event.pointingDevice() != nullptr
           && event.pointingDevice()->type() == QInputDevice::DeviceType::TouchScreen;
}
} // namespace

TouchInput::TouchInput(QObject* parent)
    : QObject(parent)
{
    pressTimer.setSingleShot(true);
    connect(&pressTimer, &QTimer::timeout, this, &TouchInput::longPress);
}

bool TouchInput::eventFilter(QObject* obj, QEvent* event)
{
    switch (event->type()) {
    case QEvent::Polish:
        if (auto* area = qobject_cast<QAbstractScrollArea*>(obj)) {
            grabTouchScrolling(*area);
        }
        break;
    case QEvent::MouseButtonPress: {
        const auto& mouse = *static_cast<QMouseEvent*>(event);
        // The event goes to the parents too if the widget ignores it: only the first one counts.
        if (isTouch(mouse) && mouse.button() == Qt::LeftButton && !pressTimer.isActive()) {
            pressPos = mouse.globalPosition().toPoint();
            pressed = QApplication::widgetAt(pressPos);
            longPressed = false;
            pressTimer.start(QGuiApplication::styleHints()->mousePressAndHoldInterval());
        }
        break;
    }
    case QEvent::MouseMove: {
        const auto& mouse = *static_cast<QMouseEvent*>(event);
        if (pressTimer.isActive()
            && (mouse.globalPosition().toPoint() - pressPos).manhattanLength()
                   > QApplication::startDragDistance()) {
            pressTimer.stop();
        }
        break;
    }
    case QEvent::KeyPress:
        // Android's Back closes the open menu or dialog first.
        if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Back) {
            QWidget* top = QApplication::activePopupWidget();
            if (top == nullptr) {
                top = QApplication::activeModalWidget();
            }
            if (top != nullptr) {
                top->close();
                event->accept();
                return true;
            }
        }
        break;
    case QEvent::MouseButtonRelease:
        pressTimer.stop();
        // The finger lifting off the menu a long press opened doesn't pick an item.
        if (longPressed) {
            longPressed = false;
            return QApplication::activePopupWidget() != nullptr;
        }
        break;
    default:
        break;
    }
    return false;
}

void TouchInput::longPress()
{
    longPressed = true;
    if (pressed.isNull()) {
        return;
    }
    QContextMenuEvent menuEvent(QContextMenuEvent::Mouse, pressed->mapFromGlobal(pressPos), pressPos);
    QCoreApplication::sendEvent(pressed, &menuEvent);
}
