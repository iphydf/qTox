/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#pragma once

#include "src/widget/style.h"

#include <QFont>

#include "../chatlinecontent.h"

class QTextDocument;
class DocumentCache;
class Settings;

class Text : public ChatLineContent
{
    Q_OBJECT

public:
    enum TextType
    {
        NORMAL,
        ACTION,
        CUSTOM
    };

    Text(DocumentCache& documentCache, Settings& settings, Style& style, const QColor& custom,
         const QString& txt = "", const QFont& font = QFont(), bool enableElide = false,
         QString rawText = QString(), const TextType& type = NORMAL);
    Text(DocumentCache& documentCache, Settings& settings, Style& style, const QString& txt = "",
         const QFont& font = QFont(), bool enableElide = false, const QString& rawText = QString(),
         const TextType& type = NORMAL);
    ~Text() override;

    void setText(const QString& txt);
    void selectText(const QString& txt, const std::pair<int, int>& point);
    void selectText(const QRegularExpression& exp, const std::pair<int, int>& point);
    void deselectText();

    void setWidth(float width) final;

    void selectionMouseMove(QPointF scenePos) final;
    void selectionStarted(QPointF scenePos) final;
    void selectionCleared() final;
    void selectionDoubleClick(QPointF scenePos) final;
    void selectionTripleClick(QPointF scenePos) final;
    void selectionFocusChanged(bool focusIn) final;
    bool isOverSelection(QPointF scenePos) const final;
    QString getSelectedText() const final;
    void fontChanged(const QFont& font) final;

    QRectF boundingRect() const final;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) final;

    void visibilityChanged(bool visible) final;
    void reloadTheme() final;

    qreal getAscent() const final;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) final;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) final;

    QString getText() const final;
    QString getLinkAt(QPointF scenePos) const;

protected:
    // dynamic resource management
    void regenerate();
    void freeResources();

    virtual QSizeF idealSize();
    int cursorFromPos(QPointF scenePos, bool fuzzy = true) const;
    int getSelectionEnd() const;
    int getSelectionStart() const;
    bool hasSelection() const;
    QString extractSanitizedText(int from, int to) const;
    QString extractImgTooltip(int pos) const;

    QTextDocument* doc = nullptr;
    QSizeF size;
    qreal width = 0.0;

private:
    void selectText(QTextCursor& cursor, const std::pair<int, int>& point);
    QColor textColor() const;

    QString text;
    QString rawText;
    QString selectedText;
    bool keepInMemory = false;
    bool elide = false;
    bool dirty = false;
    bool selectionHasFocus = true;
    int selectionEnd = -1;
    int selectionAnchor = -1;
    qreal ascent = 0.0;
    QFont defFont;
    TextType textType;
    QColor color;
    QColor customColor;
    DocumentCache& documentCache;
    Settings& settings;
    QString defStyleSheet;
    Style& style;
};
