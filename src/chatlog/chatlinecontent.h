/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QGraphicsItem>

class ChatLine;

class ChatLineContent : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    enum GraphicsItemType
    {
        ChatLineContentType = QGraphicsItem::UserType + 1,
    };

    int getColumn() const;

    virtual void setWidth(float width) = 0;
    int type() const final;

    virtual void selectionMouseMove(QPointF scenePos);
    virtual void selectionStarted(QPointF scenePos);
    virtual void selectionCleared();
    virtual void selectionDoubleClick(QPointF scenePos);
    virtual void selectionTripleClick(QPointF scenePos);
    virtual void selectionFocusChanged(bool focusIn);
    virtual bool isOverSelection(QPointF scenePos) const;
    virtual QString getSelectedText() const;
    virtual void fontChanged(const QFont& font);

    virtual QString getText() const;

    virtual qreal getAscent() const;

    QRectF boundingRect() const override = 0;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override = 0;

    virtual void visibilityChanged(bool visible);
    virtual void reloadTheme();

private:
    friend class ChatLine;
    void setIndex(int row, int col);

private:
    int row = -1;
    int col = -1;
};
