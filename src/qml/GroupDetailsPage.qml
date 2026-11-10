// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: page

    // The group's ID.
    required property string chatId
    readonly property var chat: QTox.chat(ChatKind.Group, chatId)
    readonly property bool isFounder: chat.selfRole === PeerRole.Founder

    // A peer's name, with what they may do in the group if more or less than others.
    function describePeer(peer) {
        const name = peer.self ? qsTr("%1 (you)").arg(peer.name) : peer.name || peer.publicKey;
        switch (peer.role) {
        case PeerRole.Founder:
            return qsTr("%1, founder").arg(name);
        case PeerRole.Moderator:
            return qsTr("%1, moderator").arg(name);
        case PeerRole.Observer:
            return qsTr("%1, observer").arg(name);
        default:
            return name;
        }
    }

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
                enabled: page.chat.canSetTopic
                placeholderText: qsTr("Topic")
                text: page.chat.topic
                onEditingFinished: {
                    if (text !== page.chat.topic)
                        page.chat.setTopic(text);
                }
            }

            Label {
                Layout.fillWidth: true
                text: page.chat.canSetTopic ? qsTr("Everyone in the group sees the topic.")
                    : page.chat.topicLocked ? qsTr("Only moderators can change the topic.")
                    : qsTr("Observers can't change the topic.")
                wrapMode: Text.Wrap
                font.pixelSize: 12
                opacity: 0.7
            }

            TextField {
                Layout.fillWidth: true
                placeholderText: QTox.selfName
                text: page.chat.nickname
                onEditingFinished: {
                    if (text !== page.chat.nickname)
                        page.chat.setNickname(text);
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Your name in this group. Without one, it's your profile's.")
                wrapMode: Text.Wrap
                font.pixelSize: 12
                opacity: 0.7
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Group ID")
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: page.chatId
                font.family: "monospace"
                wrapMode: Text.WrapAnywhere
            }

            // The founder's settings say it.
            Label {
                Layout.fillWidth: true
                visible: !page.isFounder
                text: page.chat.isPrivate ? qsTr("This group is private: only the friends its members invite can join.")
                                          : qsTr("This group is public: anyone with its ID can join.")
                wrapMode: Text.Wrap
                font.pixelSize: 12
                opacity: 0.7
            }

            RowLayout {
                visible: !page.chat.isPrivate

                Button {
                    text: qsTr("Copy")
                    onClicked: {
                        QTox.copyToClipboard(page.chatId);
                        QTox.notice(qsTr("Copied"));
                    }
                }

                Button {
                    visible: QTox.canShare()
                    text: qsTr("Share")
                    onClicked: QTox.share(page.chatId)
                }
            }

            // What only the founder decides.
            ColumnLayout {
                Layout.fillWidth: true
                visible: page.isFounder
                spacing: 8

                MenuSeparator {
                    Layout.fillWidth: true
                }

                Label {
                    text: qsTr("Settings")
                    font.bold: true
                }

                WrappingSwitch {
                    Layout.fillWidth: true
                    text: qsTr("Private: only the friends its members invite can join")
                    checked: page.chat.isPrivate
                    onToggled: page.chat.setPrivate(checked)
                }

                WrappingSwitch {
                    Layout.fillWidth: true
                    text: qsTr("Only moderators can change the topic")
                    checked: page.chat.topicLocked
                    onToggled: page.chat.setTopicLocked(checked)
                }

                Label {
                    text: qsTr("Who can write")
                }

                ComboBox {
                    readonly property var choices: [
                        {
                            value: VoiceState.All,
                            text: qsTr("Everyone but observers")
                        },
                        {
                            value: VoiceState.Moderators,
                            text: qsTr("Moderators")
                        },
                        {
                            value: VoiceState.Founder,
                            text: qsTr("Only you")
                        }
                    ]

                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: choices
                    // Not indexOfValue(), which runs before the model is there.
                    currentIndex: choices.findIndex(choice => choice.value === page.chat.voiceState)
                    onActivated: page.chat.setVoiceState(currentValue)
                }

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Most people at once")
                        wrapMode: Text.Wrap
                    }

                    SpinBox {
                        from: 1
                        to: 65535
                        editable: true
                        value: page.chat.peerLimit
                        onValueModified: page.chat.setPeerLimit(value)
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: page.chat.passwordSet ? qsTr("Joining needs a password.")
                                                : qsTr("Joining needs no password.")
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    Layout.fillWidth: true

                    TextField {
                        id: password

                        Layout.fillWidth: true
                        placeholderText: page.chat.passwordSet ? qsTr("New password") : qsTr("Password")
                        echoMode: TextInput.Password
                    }

                    Button {
                        enabled: password.text.length > 0
                        text: qsTr("Set")
                        onClicked: {
                            page.chat.setPassword(password.text);
                            password.clear();
                        }
                    }
                }

                Button {
                    visible: page.chat.passwordSet
                    text: qsTr("Remove password")
                    onClicked: page.chat.setPassword("")
                }
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

                // The others can be written to privately, and moderated.
                delegate: ListRow {
                    required property var modelData

                    Layout.fillWidth: true
                    leftPadding: 0
                    enabled: !modelData.self
                    text: page.describePeer(modelData)
                    onClicked: peerMenu.show(modelData, this)
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
                text: qsTr("Leave group")
                onClicked: {
                    QTox.leaveGroup(page.chatId);
                    page.StackView.view.popToIndex(0);
                }
            }
        }
    }

    // What the user can do with another peer.
    Menu {
        id: peerMenu

        property var peer: ({})
        readonly property bool moderated: peer.role !== undefined && page.chat.canModerate(peer.role)

        function offers(role) {
            return moderated && page.chat.canGiveRole(peer.role, role);
        }

        function show(newPeer, item) {
            peer = newPeer;
            popup(item, 0, item.height);
        }

        MenuItem {
            text: qsTr("Write privately")
            onTriggered: {
                page.chat.privateTo = peerMenu.peer.publicKey;
                page.StackView.view.pop();
            }
        }

        MenuItem {
            visible: peerMenu.offers(PeerRole.Moderator)
            height: visible ? implicitHeight : 0
            text: qsTr("Make moderator")
            onTriggered: page.chat.setPeerRole(peerMenu.peer.publicKey, PeerRole.Moderator)
        }

        MenuItem {
            visible: peerMenu.offers(PeerRole.User)
            height: visible ? implicitHeight : 0
            text: peerMenu.peer.role === PeerRole.Observer ? qsTr("Let them write") : qsTr("Make regular user")
            onTriggered: page.chat.setPeerRole(peerMenu.peer.publicKey, PeerRole.User)
        }

        MenuItem {
            visible: peerMenu.offers(PeerRole.Observer)
            height: visible ? implicitHeight : 0
            text: qsTr("Make observer")
            onTriggered: page.chat.setPeerRole(peerMenu.peer.publicKey, PeerRole.Observer)
        }

        MenuItem {
            visible: peerMenu.moderated
            height: visible ? implicitHeight : 0
            text: qsTr("Remove from group")
            onTriggered: kickDialog.ask(peerMenu.peer)
        }
    }

    Dialog {
        id: kickDialog

        property var peer: ({})

        function ask(newPeer) {
            peer = newPeer;
            open();
        }

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: qsTr("Remove %1?").arg(peer.name)
        onAccepted: page.chat.kick(peer.publicKey)

        Label {
            width: parent.width
            text: page.chat.isPrivate ? qsTr("They can come back only if someone invites them.")
                                      : qsTr("They can come back with the group's ID.")
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
                Material.foreground: Material.color(Material.Red)
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    InviteDialog {
        id: inviteDialog

        chat: page.chat
    }
}
