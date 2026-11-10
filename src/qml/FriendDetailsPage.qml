// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: page

    // The friend's public key.
    required property string chatId
    readonly property var chat: QTox.chat(ChatKind.Friend, chatId)

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

            RowLayout {
                spacing: 12

                Avatar {
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 64
                    source: page.chat.avatar
                    name: page.chat.name
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: page.chat.userName || page.chat.name
                        font.bold: true
                        font.pixelSize: 18
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        StatusDot {
                            id: statusDot

                            status: page.chat.status
                        }

                        Label {
                            text: statusDot.description
                            opacity: 0.7
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: page.chat.statusMessage
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                opacity: 0.7
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Alias")
                font.bold: true
            }

            TextField {
                Layout.fillWidth: true
                placeholderText: page.chat.userName || qsTr("Alias")
                text: page.chat.alias
                onEditingFinished: {
                    if (text !== page.chat.alias)
                        page.chat.setAlias(text);
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Shown instead of their name. Only you see it.")
                wrapMode: Text.Wrap
                font.pixelSize: 12
                opacity: 0.7
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Public key")
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: page.chatId
                wrapMode: Text.WrapAnywhere
                font.family: "monospace"
            }

            Button {
                text: qsTr("Copy")
                onClicked: QTox.copyToClipboard(page.chatId)
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            TextArea {
                Layout.fillWidth: true
                placeholderText: qsTr("Note")
                text: page.chat.note
                wrapMode: TextEdit.Wrap
                onEditingFinished: {
                    if (text !== page.chat.note)
                        page.chat.setNote(text);
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Only you see it.")
                wrapMode: Text.Wrap
                font.pixelSize: 12
                opacity: 0.7
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            WrappingSwitch {
                Layout.fillWidth: true
                text: qsTr("Save the files they send without asking")
                checked: page.chat.autoAcceptFiles
                onToggled: page.chat.setAutoAcceptFiles(checked)
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Conferences and groups")
                font.bold: true
            }

            Button {
                enabled: page.chat.online
                text: qsTr("Invite to a conference or group")
                onClicked: inviteDialog.open()
            }

            WrappingSwitch {
                Layout.fillWidth: true
                text: qsTr("Join the conferences and groups they invite you to without asking")
                checked: page.chat.autoAcceptInvites
                onToggled: page.chat.setAutoAcceptInvites(checked)
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("Remove chat history")
                onClicked: removeHistoryDialog.open()
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("Remove friend")
                Material.foreground: Material.color(Material.Red)
                onClicked: removeDialog.open()
            }
        }
    }

    Dialog {
        id: inviteDialog

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: qsTr("Invite %1 to").arg(page.chat.name)

        ColumnLayout {
            width: parent.width

            Repeater {
                model: QTox.chats

                delegate: ListRow {
                    required property int kind
                    required property string chatId
                    required property string name

                    visible: kind !== ChatKind.Friend
                    Layout.fillWidth: true
                    text: name
                    onClicked: {
                        QTox.chat(kind, chatId).invite(page.chatId);
                        QTox.notice(qsTr("Invited %1").arg(page.chat.name));
                        inviteDialog.close();
                    }
                }
            }

            ListRow {
                Layout.fillWidth: true
                text: qsTr("New conference")
                onClicked: {
                    QTox.createConference("", page.chatId);
                    QTox.notice(qsTr("Invited %1").arg(page.chat.name));
                    inviteDialog.close();
                }
            }
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Cancel")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }
    }

    Dialog {
        id: removeHistoryDialog

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: qsTr("Confirmation")
        onAccepted: page.chat.removeHistory()

        Label {
            width: parent.width
            // As the Widgets UI asks.
            text: qsTr("Are you sure to remove %1 chat history?").arg(page.chat.name)
            wrapMode: Text.Wrap
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Cancel")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("Remove")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    Dialog {
        id: removeDialog

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: qsTr("Remove %1?").arg(page.chat.name)
        onAccepted: {
            QTox.removeFriend(page.chatId, removeHistory.checked);
            page.StackView.view.popToIndex(0);
        }

        ColumnLayout {
            width: parent.width

            Label {
                Layout.fillWidth: true
                text: qsTr("You can't message each other until one of you adds the other again.")
                wrapMode: Text.Wrap
            }

            CheckBox {
                id: removeHistory

                text: qsTr("Also remove the chat history")
            }
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Cancel")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("Remove")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }
}
