// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls

// A text field with a name as its placeholder: the style's shows what looks like HTML as HTML.
TextField {
    property string placeholder

    // Some text from the start: the style only makes room over the field's text for a placeholder
    // that has some when it's made.
    placeholderText: " "

    Component.onCompleted: {
        // The placeholder's Text is the style's, which gives it no name.
        for (const child of children) {
            if (child instanceof Text)
                child.textFormat = Text.PlainText;
        }
        // Only now, so that the name is never laid out as HTML: the lint can't know.
        placeholderText = Qt.binding(() => placeholder); // qmllint disable Plugin.qTox.untrusted-text
    }
}
