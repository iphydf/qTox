// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick

// Keeps its last item in view, also when it gets smaller, as when the keyboard opens, unless the
// user scrolled away from it.
ListView {
    id: list

    property bool following: true

    onMovementEnded: following = atYEnd
    onCountChanged: if (following) positionViewAtEnd()
    onHeightChanged: if (following) positionViewAtEnd()
    // A new item's height comes after its count, when the service sends its data.
    onContentHeightChanged: if (following && !moving) positionViewAtEnd()
    Component.onCompleted: positionViewAtEnd()
}
