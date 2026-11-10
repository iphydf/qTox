/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#pragma once

#include <QObject>

class QBoxLayout;
class QWidget;

/**
 * @brief Lays a box layout out as a row when its items fit side by side, and as a column when not.
 *
 * Watches the widget the layout sits in, so narrow windows stack what wide ones put in a row.
 */
class RowOrColumn : public QObject
{
    Q_OBJECT
public:
    RowOrColumn(QBoxLayout& layout, QWidget& container);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    int rowWidth() const;
    void update();

    QBoxLayout& layout;
};
