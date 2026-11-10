// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page

    header: ToolBar {
        RowLayout {
            anchors.fill: parent

            BackButton {
                onClicked: page.StackView.view.pop()
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Requests")
                font.bold: true
            }
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: column.implicitHeight

        ColumnLayout {
            id: column

            width: parent.width
            spacing: 0

            Label {
                Layout.margins: 16
                Layout.bottomMargin: 4
                visible: requests.count > 0
                text: qsTr("Friend requests")
                font.bold: true
                opacity: 0.7
            }

            Repeater {
                id: requests

                model: QTox.friendRequests

                delegate: Pane {
                    id: request

                    required property string publicKey
                    required property string message

                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent

                        Label {
                            Layout.fillWidth: true
                            text: request.message
                            // Never HTML, as anyone with the user's Tox ID can write it.
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                        }

                        Label {
                            Layout.fillWidth: true
                            text: request.publicKey
                            textFormat: Text.PlainText
                            font.pixelSize: 11
                            opacity: 0.7
                            elide: Text.ElideMiddle
                        }

                        RowLayout {
                            Button {
                                text: qsTr("Accept")
                                highlighted: true
                                onClicked: QTox.acceptFriendRequest(request.publicKey)
                            }

                            Button {
                                text: qsTr("Reject")
                                flat: true
                                onClicked: QTox.rejectFriendRequest(request.publicKey)
                            }
                        }
                    }
                }
            }

            Label {
                Layout.margins: 16
                Layout.bottomMargin: 4
                visible: invites.count > 0
                text: qsTr("Invites")
                font.bold: true
                opacity: 0.7
            }

            Repeater {
                id: invites

                model: QTox.invites

                delegate: Pane {
                    id: invite

                    required property string inviteId
                    required property int kind
                    required property string friendName
                    required property string name

                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent

                        Label {
                            Layout.fillWidth: true
                            text: invite.kind === ChatKind.Group
                                  // In one go: the names may contain "%2".
                                  ? qsTr("%1 invites you to the group %2.").replace(/%[12]/g, marker => marker === "%1" ? invite.friendName
                                                                                                                         : invite.name)
                                  : qsTr("%1 invites you to a conference.").arg(invite.friendName)
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                        }

                        RowLayout {
                            Button {
                                text: qsTr("Join")
                                highlighted: true
                                onClicked: {
                                    if (invite.kind === ChatKind.Group)
                                        joinDialog.ask(invite.inviteId, invite.name);
                                    else
                                        QTox.acceptInvite(invite.inviteId);
                                }
                            }

                            Button {
                                text: qsTr("Decline")
                                flat: true
                                onClicked: QTox.rejectInvite(invite.inviteId)
                            }
                        }
                    }
                }
            }
        }
    }

    // Asks for a group's password, if it has one.
    Dialog {
        id: joinDialog

        property string inviteId

        property string name

        function ask(id, newName) {
            inviteId = id;
            name = newName;
            password.clear();
            open();
        }

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        // No name in the title, as dialogs show titles as HTML.
        title: qsTr("Join the group")
        onOpened: password.forceActiveFocus()
        onAccepted: QTox.acceptInvite(inviteId, password.text)

        ColumnLayout {
            width: parent.width

            Label {
                Layout.fillWidth: true
                text: joinDialog.name
                textFormat: Text.PlainText
                font.bold: true
                wrapMode: Text.Wrap
            }

            TextField {
                id: password

                Layout.fillWidth: true
                placeholderText: qsTr("Password, if it has one")
                echoMode: TextInput.Password
                onAccepted: joinDialog.accept()
            }
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Cancel")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("Join")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }
}
