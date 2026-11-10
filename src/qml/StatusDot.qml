// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls.Material

// How someone is: in the colours of the Widgets UI's status dots, and grey when offline.
Rectangle {
    // A UserStatus.Status.
    property int status: UserStatus.Offline
    // The status in words, for those who can't see the colour.
    readonly property string description: {
        switch (status) {
        case UserStatus.Online:
            return qsTr("Online");
        case UserStatus.Away:
            return qsTr("Away");
        case UserStatus.Busy:
            return qsTr("Busy");
        default:
            return qsTr("Offline");
        }
    }

    width: 10
    height: 10
    radius: width / 2
    color: {
        switch (status) {
        case UserStatus.Online:
            return "#6fc062";
        case UserStatus.Away:
            return "#cdbe41";
        case UserStatus.Busy:
            return "#c94f50";
        default:
            return Material.color(Material.Grey);
        }
    }
}
