/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QTimer>

class QWidget;

/**
 * @brief Touch input for widgets made for mice, like on phones.
 *
 * A finger drags scroll areas and a long press opens the context menu. Mice still select text,
 * drag items and open menus with the right button. Android's Back closes menus and dialogs.
 */
class TouchInput : public QObject
{
    Q_OBJECT
public:
    explicit TouchInput(QObject* parent);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void longPress();

    QTimer pressTimer;
    QPointer<QWidget> pressed;
    QPoint pressPos;
    bool longPressed = false;
};
