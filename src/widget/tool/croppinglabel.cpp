/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#include "croppinglabel.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QTextDocument>

CroppingLabel::CroppingLabel(QWidget* parent)
    : QLabel(parent)
    , blockPaintEvents(false)
    , editable(false)
    , elideMode(Qt::ElideRight)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    class LineEdit : public QLineEdit
    {
    public:
        explicit LineEdit(QWidget* parent = nullptr)
            : QLineEdit(parent)
        {
        }

    protected:
        void keyPressEvent(QKeyEvent* event) override
        {
            if (event->key() == Qt::Key_Escape) {
                undo();
                clearFocus();
            }

            QLineEdit::keyPressEvent(event);
        }
    };

    textEdit = new LineEdit(this);
    textEdit->hide();
    textEdit->setInputMethodHints(Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText | Qt::ImhPreferLatin);

    connect(textEdit, &QLineEdit::editingFinished, this, &CroppingLabel::editingFinished);
}

void CroppingLabel::editBegin()
{
    showTextEdit();
    textEdit->selectAll();
}

void CroppingLabel::setEditable(bool editable_)
{
    editable = editable_;

    if (editable)
        setCursor(Qt::PointingHandCursor);
    else
        unsetCursor();
}

void CroppingLabel::setElideMode(Qt::TextElideMode elide)
{
    elideMode = elide;
}

void CroppingLabel::setText(const QString& text)
{
    origText = text.trimmed();
    setElidedText();
}

void CroppingLabel::setPlaceholderText(const QString& text)
{
    textEdit->setPlaceholderText(text);
    setElidedText();
}

void CroppingLabel::resizeEvent(QResizeEvent* ev)
{
    setElidedText();
    textEdit->resize(ev->size());

    QLabel::resizeEvent(ev);
}

QSize CroppingLabel::sizeHint() const
{
    return {0, QLabel::sizeHint().height()};
}

QSize CroppingLabel::minimumSizeHint() const
{
    return {fontMetrics().horizontalAdvance("..."), QLabel::minimumSizeHint().height()};
}

void CroppingLabel::mouseReleaseEvent(QMouseEvent* e)
{
    if (editable)
        showTextEdit();

    emit clicked();

    QLabel::mouseReleaseEvent(e);
}

void CroppingLabel::paintEvent(QPaintEvent* paintEvent)
{
    if (blockPaintEvents) {
        paintEvent->ignore();
        return;
    }
    QLabel::paintEvent(paintEvent);
}

void CroppingLabel::setElidedText()
{
    const QString elidedText = fontMetrics().elidedText(origText, elideMode, width());
    if (elidedText != origText)
        setToolTip(Qt::convertFromPlainText(origText, Qt::WhiteSpaceNormal));
    else
        setToolTip(QString());
    if (!elidedText.isEmpty()) {
        QLabel::setText(elidedText);
    } else {
        // NOTE: it would be nice if the label had custom styling when it was default
        QLabel::setText(textEdit->placeholderText());
    }
}

void CroppingLabel::hideTextEdit()
{
    textEdit->hide();
    blockPaintEvents = false;
}

void CroppingLabel::showTextEdit()
{
    blockPaintEvents = true;
    textEdit->show();
    textEdit->setFocus();
    textEdit->setText(origText);
    textEdit->setFocusPolicy(Qt::ClickFocus);
}

/**
 * @brief Get original full text.
 * @return The un-cropped text.
 */
QString CroppingLabel::fullText()
{
    return origText;
}

void CroppingLabel::minimizeMaximumWidth()
{
    // This function chooses the smallest possible maximum width.
    // Text width + padding. Without padding, we'll have ellipses.
    setMaximumWidth(fontMetrics().horizontalAdvance(origText) + fontMetrics().horizontalAdvance("..."));
}

void CroppingLabel::editingFinished()
{
    hideTextEdit();
    const QString newText =
        textEdit->text().trimmed().remove(QRegularExpression(QStringLiteral(R"([\t\n\v\f\r\x0000])")));

    if (origText != newText)
        emit editFinished(textEdit->text());

    emit editRemoved();
}
