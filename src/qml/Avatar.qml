// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A round picture, or the name's first letter if it has none.
Item {
    id: avatar

    // An image://avatar/ source.
    property string source
    property string name

    implicitWidth: 40
    implicitHeight: 40

    Rectangle {
        anchors.fill: parent
        visible: picture.status !== Image.Ready
        radius: width / 2
        color: Material.color(Material.BlueGrey, Material.Shade300)

        Label {
            anchors.centerIn: parent
            text: avatar.name.length > 0 ? avatar.name[0].toUpperCase() : ""
            color: "white"
            font.pixelSize: avatar.height * 0.45
            font.bold: true
            // The name is always next to it.
            Accessible.ignored: true
        }
    }

    Image {
        id: picture

        anchors.fill: parent
        source: avatar.source
        sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
    }
}
