/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#include "text.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QFontMetrics>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QTextBlock>
#include <QTextFragment>

#include "../documentcache.h"
#include <utility>

Text::Text(DocumentCache& documentCache_, Settings& settings_, Style& style_, const QColor& custom,
           const QString& txt, const QFont& font, bool enableElide, QString rawText_,
           const TextType& type)
    : rawText(std::move(rawText_))
    , elide(enableElide)
    , defFont(font)
    , textType(type)
    , customColor(custom)
    , documentCache(documentCache_)
    , settings{settings_}
    , defStyleSheet(style_.getStylesheet(QStringLiteral("chatArea/innerStyle.qss"), settings_, font))
    , style{style_}
{
    color = textColor();
    setText(txt);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
}

Text::Text(DocumentCache& documentCache_, Settings& settings_, Style& style_, const QString& txt,
           const QFont& font, bool enableElide, const QString& rawText_, const TextType& type)
    : Text(documentCache_, settings_, style_, style_.getColor(Style::ColorPalette::MainText), txt,
           font, enableElide, rawText_, type)
{
}

Text::~Text()
{
    if (doc != nullptr)
        documentCache.push(doc);
}

void Text::setText(const QString& txt)
{
    text = txt;
    dirty = true;
}

void Text::selectText(const QString& txt, const std::pair<int, int>& point)
{
    regenerate();

    if (doc == nullptr) {
        return;
    }

    auto cursor = doc->find(txt, point.first);

    selectText(cursor, point);
}

void Text::selectText(const QRegularExpression& exp, const std::pair<int, int>& point)
{
    regenerate();

    if (doc == nullptr) {
        return;
    }

    auto cursor = doc->find(exp, point.first);

    selectText(cursor, point);
}

void Text::deselectText()
{
    dirty = true;
    regenerate();
    update();
}

void Text::setWidth(float w)
{
    width = static_cast<qreal>(w);
    dirty = true;

    regenerate();
}

void Text::selectionMouseMove(QPointF scenePos)
{
    if (doc == nullptr)
        return;

    const int cur = cursorFromPos(scenePos);
    if (cur >= 0) {
        selectionEnd = cur;
        selectedText = extractSanitizedText(getSelectionStart(), getSelectionEnd());
    }

    update();
}

void Text::selectionStarted(QPointF scenePos)
{
    const int cur = cursorFromPos(scenePos);
    if (cur >= 0) {
        selectionEnd = cur;
        selectionAnchor = cur;
    }
}

void Text::selectionCleared()
{
    selectedText.clear();
    selectedText.squeeze();

    // Do not reset selectionAnchor!
    selectionEnd = -1;

    update();
}

void Text::selectionDoubleClick(QPointF scenePos)
{
    if (doc == nullptr)
        return;

    const int cur = cursorFromPos(scenePos);

    if (cur >= 0) {
        QTextCursor cursor(doc);
        cursor.setPosition(cur);
        cursor.select(QTextCursor::WordUnderCursor);

        selectionAnchor = cursor.selectionStart();
        selectionEnd = cursor.selectionEnd();

        selectedText = extractSanitizedText(getSelectionStart(), getSelectionEnd());
    }

    update();
}

void Text::selectionTripleClick(QPointF scenePos)
{
    if (doc == nullptr)
        return;

    const int cur = cursorFromPos(scenePos);

    if (cur >= 0) {
        QTextCursor cursor(doc);
        cursor.setPosition(cur);
        cursor.select(QTextCursor::BlockUnderCursor);

        selectionAnchor = cursor.selectionStart();
        selectionEnd = cursor.selectionEnd();

        if (cursor.block().isValid() && cursor.block().blockNumber() != 0)
            selectionAnchor++;

        selectedText = extractSanitizedText(getSelectionStart(), getSelectionEnd());
    }

    update();
}

void Text::selectionFocusChanged(bool focusIn)
{
    selectionHasFocus = focusIn;
    update();
}

bool Text::isOverSelection(QPointF scenePos) const
{
    const int cur = cursorFromPos(scenePos);
    return getSelectionStart() < cur && getSelectionEnd() >= cur;
}

QString Text::getSelectedText() const
{
    return selectedText;
}

void Text::fontChanged(const QFont& font)
{
    defFont = font;
}

QRectF Text::boundingRect() const
{
    return {{0, 0}, size};
}

void Text::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    std::ignore = option;
    std::ignore = widget;

    if (doc == nullptr)
        return;

    painter->setClipRect(boundingRect());

    // draw selection
    QAbstractTextDocumentLayout::PaintContext ctx;
    QAbstractTextDocumentLayout::Selection sel;

    if (hasSelection()) {
        sel.cursor = QTextCursor(doc);
        sel.cursor.setPosition(getSelectionStart());
        sel.cursor.setPosition(getSelectionEnd(), QTextCursor::KeepAnchor);
    }

    const QColor selectionColor = style.getColor(Style::ColorPalette::SelectText);
    sel.format.setBackground(selectionColor.lighter(selectionHasFocus ? 100 : 160));
    sel.format.setForeground(selectionHasFocus ? Qt::white : Qt::black);

    ctx.selections.append(sel);
    ctx.palette.setColor(QPalette::Text, color);

    // draw text
    doc->documentLayout()->draw(painter, ctx);
}

void Text::visibilityChanged(bool visible)
{
    keepInMemory = visible;

    regenerate();
    update();
}

void Text::reloadTheme()
{
    defStyleSheet = style.getStylesheet(QStringLiteral("chatArea/innerStyle.qss"), settings, defFont);
    color = textColor();
    dirty = true;
    regenerate();
    update();
}

qreal Text::getAscent() const
{
    return ascent;
}

void Text::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        event->accept(); // grabber
}

void Text::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (doc == nullptr)
        return;

    const QString anchor = doc->documentLayout()->anchorAt(event->pos());

    // open anchor in browser
    if (!anchor.isEmpty())
        QDesktopServices::openUrl(QUrl(anchor));
}

void Text::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    if (doc == nullptr)
        return;

    const QString anchor = doc->documentLayout()->anchorAt(event->pos());

    if (anchor.isEmpty())
        setCursor(Qt::IBeamCursor);
    else
        setCursor(Qt::PointingHandCursor);

    // tooltip
    setToolTip(extractImgTooltip(cursorFromPos(event->scenePos(), false)));
}

QString Text::getText() const
{
    return rawText;
}

/**
 * @brief Extracts the target of a link from the text at a given coordinate
 * @param scenePos Position in scene coordinates
 * @return The link target URL, or an empty string if there is no link there
 */
QString Text::getLinkAt(QPointF scenePos) const
{
    QTextCursor cursor(doc);
    cursor.setPosition(cursorFromPos(scenePos));
    return cursor.charFormat().anchorHref();
}

void Text::regenerate()
{
    if (doc == nullptr) {
        doc = documentCache.pop();
        dirty = true;
    }

    if (dirty) {
        doc->setDefaultFont(defFont);

        if (elide) {
            const QFontMetrics metrics = QFontMetrics(defFont);
            const QString elidedText = metrics.elidedText(text, Qt::ElideRight, qRound(width));

            doc->setPlainText(elidedText);
        } else {
            doc->setDefaultStyleSheet(defStyleSheet);
            doc->setHtml(text);
        }

        // wrap mode
        QTextOption opt;
        opt.setWrapMode(elide ? QTextOption::NoWrap : QTextOption::WrapAtWordBoundaryOrAnywhere);
        doc->setDefaultTextOption(opt);

        // width
        doc->setTextWidth(width);
        doc->documentLayout()->update();

        // update ascent
        if (doc->firstBlock().layout()->lineCount() > 0)
            ascent = doc->firstBlock().layout()->lineAt(0).ascent();

        // let the scene know about our change in size
        if (size != idealSize())
            prepareGeometryChange();

        // get the new width and height
        size = idealSize();

        dirty = false;
    }

    // if we are not visible -> free mem
    if (!keepInMemory)
        freeResources();
}

void Text::freeResources()
{
    documentCache.push(doc);
    doc = nullptr;
}

QSizeF Text::idealSize()
{
    if (doc != nullptr)
        return doc->size();

    return size;
}

int Text::cursorFromPos(QPointF scenePos, bool fuzzy) const
{
    if (doc != nullptr)
        return doc->documentLayout()->hitTest(mapFromScene(scenePos),
                                              fuzzy ? Qt::FuzzyHit : Qt::ExactHit);

    return -1;
}

int Text::getSelectionEnd() const
{
    return qMax(selectionAnchor, selectionEnd);
}

int Text::getSelectionStart() const
{
    return qMin(selectionAnchor, selectionEnd);
}

bool Text::hasSelection() const
{
    return selectionEnd >= 0;
}

QString Text::extractSanitizedText(int from, int to) const
{
    if (doc == nullptr)
        return "";

    QString txt;

    const QTextBlock begin = doc->findBlock(from);
    const QTextBlock end = doc->findBlock(to);
    for (QTextBlock block = begin; block != end.next() && block.isValid(); block = block.next()) {
        for (QTextBlock::Iterator itr = block.begin(); itr != block.end(); ++itr) {
            int pos = itr.fragment().position(); // fragment position -> position of the first
                                                 // character in the fragment

            if (itr.fragment().charFormat().isImageFormat()) {
                const QTextImageFormat imgFmt = itr.fragment().charFormat().toImageFormat();
                const QString key = imgFmt.name(); // img key (eg. key::D for :D)
                const QString rune = key.mid(4);

                if (pos >= from && pos < to) {
                    txt += rune;
                    ++pos;
                }
            } else {
                for (const QChar c : itr.fragment().text()) {
                    if (pos >= from && pos < to)
                        txt += c;

                    ++pos;
                }
            }
        }

        txt += '\n';
    }

    txt.chop(1);

    return txt;
}

QString Text::extractImgTooltip(int pos) const
{
    for (QTextBlock::Iterator itr = doc->firstBlock().begin(); itr != doc->firstBlock().end(); ++itr) {
        if (itr.fragment().contains(pos) && itr.fragment().charFormat().isImageFormat()) {
            const QTextImageFormat imgFmt = itr.fragment().charFormat().toImageFormat();
            return imgFmt.toolTip();
        }
    }

    return {};
}

void Text::selectText(QTextCursor& cursor, const std::pair<int, int>& point)
{
    if (!cursor.isNull()) {
        cursor.beginEditBlock();
        cursor.setPosition(point.first);
        cursor.setPosition(point.first + point.second, QTextCursor::KeepAnchor);
        cursor.endEditBlock();

        QTextCharFormat format;
        format.setBackground(QBrush(style.getColor(Style::ColorPalette::SearchHighlighted)));
        cursor.mergeCharFormat(format);

        regenerate();
        update();
    }
}

QColor Text::textColor() const
{
    QColor c = style.getColor(Style::ColorPalette::MainText);
    if (textType == ACTION) {
        c = style.getColor(Style::ColorPalette::Action);
    } else if (textType == CUSTOM) {
        c = customColor;
    }

    return c;
}
