// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page

    // The conference's ID.
    required property string chatId
    readonly property var chat: QTox.chat(ChatKind.Conference, chatId)

    header: ToolBar {
        RowLayout {
            anchors.fill: parent

            BackButton {
                onClicked: page.StackView.view.pop()
            }

            Label {
                Layout.fillWidth: true
                text: page.chat.name
                font.bold: true
                elide: Text.ElideRight
            }
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: column.implicitHeight + 32

        ColumnLayout {
            id: column

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: 16
            }
            spacing: 8

            TextField {
                Layout.fillWidth: true
                placeholderText: qsTr("Title")
                text: page.chat.name
                onEditingFinished: {
                    if (text.trim() !== "" && text !== page.chat.name)
                        page.chat.setTitle(text);
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Everyone in the conference sees the title and can change it.")
                wrapMode: Text.Wrap
                font.pixelSize: 12
                opacity: 0.7
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("%n user(s) in chat", "Number of users in chat", page.chat.peers.length)
                font.bold: true
            }

            Repeater {
                model: page.chat.peers

                delegate: Label {
                    required property var modelData

                    Layout.fillWidth: true
                    text: modelData.self ? qsTr("%1 (you)").arg(modelData.name)
                                                                : modelData.name || modelData.publicKey
                    elide: Text.ElideRight
                }
            }

            Button {
                text: qsTr("Invite friends")
                onClicked: inviteDialog.open()
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("Leave conference")
                onClicked: {
                    QTox.leaveConference(page.chatId);
                    page.StackView.view.popToIndex(0);
                }
            }
        }
    }

    InviteDialog {
        id: inviteDialog

        chat: page.chat
    }
}
