// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls

// A row to tap, which screen readers can tap too: Qt only offers them a press for buttons.
ItemDelegate {
    id: row

    Accessible.onPressAction: {
        if (enabled)
            clicked();
    }

    // Never HTML, as rows show names: the style's shows what looks like HTML as HTML.
    contentItem: Label {
        text: row.text
        textFormat: Text.PlainText
        font: row.font
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
}
