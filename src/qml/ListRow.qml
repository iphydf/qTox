// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls

// A row to tap, which screen readers can tap too: Qt only offers them a press for buttons.
ItemDelegate {
    Accessible.onPressAction: {
        if (enabled)
            clicked();
    }
}
