/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include "roworcolumn.h"

#include <QBoxLayout>
#include <QEvent>
#include <QWidget>

#include <algorithm>
#include <tuple>

RowOrColumn::RowOrColumn(QBoxLayout& layout_, QWidget& container)
    : QObject(&container)
    , layout{layout_}
{
    container.installEventFilter(this);
}

bool RowOrColumn::eventFilter(QObject* obj, QEvent* event)
{
    std::ignore = obj;
    if (event->type() == QEvent::Resize) {
        update();
    }
    return false;
}

/**
 * @brief The width the items want side by side.
 */
int RowOrColumn::rowWidth() const
{
    const QMargins margins = layout.contentsMargins();
    int width = margins.left() + margins.right();
    int shown = 0;
    for (int i = 0; i < layout.count(); ++i) {
        const QLayoutItem* item = layout.itemAt(i);
        if (item->isEmpty()) {
            continue;
        }
        width += item->sizeHint().width();
        ++shown;
    }
    return width + std::max(0, layout.spacing()) * std::max(0, shown - 1);
}

void RowOrColumn::update()
{
    // The layout's geometry is already set for the new size: layouts handle resizes before filters.
    const auto direction = layout.geometry().width() >= rowWidth() ? QBoxLayout::LeftToRight
                                                                     : QBoxLayout::TopToBottom;
    if (layout.direction() != direction) {
        layout.setDirection(direction);
    }
}
