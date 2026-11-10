// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Invites online friends into a conference or group.
Dialog {
    id: dialog

    // A QmlChat.
    required property var chat
    readonly property var present: chat.peers.map(peer => peer.publicKey)
    readonly property bool anyone: {
        for (let row = 0; row < invitable.count; ++row) {
            if (invitable.itemAt(row)?.visible)
                return true;
        }
        return false;
    }

    anchors.centerIn: parent
    width: Math.min(parent.width - 32, 400)
    modal: true
    title: qsTr("Invite friends")

    ColumnLayout {
        width: parent.width

        Label {
            Layout.fillWidth: true
            visible: !dialog.anyone
            text: qsTr("Friends can be invited when they're online.")
            wrapMode: Text.Wrap
        }

        Repeater {
            id: invitable

            model: QTox.chats

            // The online friends, without those in a conference: a group gives its peers other
            // keys.
            delegate: ListRow {
                required property int kind
                required property string chatId
                required property string name
                required property bool online

                visible: kind === ChatKind.Friend && online && !dialog.present.includes(chatId)
                Layout.fillWidth: true
                text: name
                onClicked: {
                    dialog.chat.invite(chatId);
                    QTox.notice(qsTr("Invited %1").arg(name));
                    dialog.close();
                }
            }
        }
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Close")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
}
