// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls

// A header's way back: an arrow on screen, "Back" to screen readers.
ToolButton {
    text: "‹"
    font.pixelSize: 24
    Accessible.name: qsTr("Back")
}
