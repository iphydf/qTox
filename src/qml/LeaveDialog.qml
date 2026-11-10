// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Asks before leaving a conference or group, as the Widgets UI does: coming back takes its ID, or
// for a private group a new invite.
Dialog {
    id: dialog

    // A ChatKind, and the conference's or group's ID.
    required property int kind
    required property string chatId

    // Once the user left.
    signal left

    anchors.centerIn: parent
    width: Math.min(parent.width - 32, 400)
    modal: true
    title: kind === ChatKind.Group ? qsTr("Leave group") : qsTr("Leave conference")
    onAccepted: {
        if (kind === ChatKind.Group)
            QTox.leaveGroup(chatId);
        else
            QTox.leaveConference(chatId);
        left();
    }

    // The page it's on shows the name.
    Label {
        width: parent.width
        text: dialog.kind === ChatKind.Group ? qsTr("Are you sure you want to leave this group?")
                                             : qsTr("Are you sure you want to leave this conference?")
        wrapMode: Text.Wrap
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Cancel")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        Button {
            text: qsTr("Leave")
            flat: true
            Material.foreground: Material.color(Material.Red)
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }
}
