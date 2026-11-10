// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: page

    // The kind of chat the user is making or joining, whose page opens once it's there, or -1.
    property int waitingFor: -1

    // Goes where a tapped notification asked to.
    function openRequested() {
        const request = QTox.takeOpenRequest();
        if (request.page === undefined) {
            return;
        }

        const view = page.StackView.view;
        view.pop(page, StackView.Immediate);
        if (request.page === "chat") {
            view.push(chatPage, {
                kind: request.kind,
                chatId: request.chatId
            });
        } else if (request.page === "share") {
            view.push(sharePage, {
                file: request.file,
                text: request.text
            });
        } else {
            view.push(requestsPage);
        }
    }

    StackView.onActivated: openRequested()

    Connections {
        target: QTox

        function onOpenRequested() {
            // Otherwise it waits for the page to be activated.
            if (page.StackView.view) {
                page.openRequested();
            }
        }
    }

    // Whether the header looks for chats by name.
    property bool searching: false

    Component.onDestruction: QTox.chatFilter = ""

    function stopSearch() {
        page.searching = false;
        searchField.clear();
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            visible: page.searching

            BackButton {
                onClicked: page.stopSearch()
            }

            TextField {
                id: searchField

                Layout.fillWidth: true
                placeholderText: qsTr("Search")
                onTextChanged: QTox.chatFilter = text
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            visible: !page.searching

            // The user's status, which they change here.
            ListRow {
                Layout.fillWidth: true
                padding: 0
                Accessible.name: [selfName.text, selfStatus.description].join(", ")
                Accessible.description: qsTr("Change your status")
                onClicked: statusMenu.open()

                contentItem: RowLayout {
                    spacing: 8

                    StatusDot {
                        id: selfStatus

                        status: QTox.online ? QTox.selfStatus : UserStatus.Offline
                    }

                    Label {
                        id: selfName

                        Layout.fillWidth: true
                        text: QTox.selfName || "qTox"
                        font.bold: true
                        elide: Text.ElideRight
                        Accessible.ignored: true
                    }
                }

                Menu {
                    id: statusMenu

                    y: parent.height

                    Repeater {
                        model: [
                            {
                                status: UserStatus.Online,
                                text: qsTr("Online")
                            },
                            {
                                status: UserStatus.Away,
                                text: qsTr("Away")
                            },
                            {
                                status: UserStatus.Busy,
                                text: qsTr("Busy")
                            }
                        ]

                        delegate: MenuItem {
                            required property var modelData

                            text: modelData.text
                            checkable: true
                            checked: QTox.selfStatus === modelData.status
                            onTriggered: QTox.selfStatus = modelData.status
                        }
                    }
                }
            }

            ToolButton {
                readonly property int count: QTox.friendRequestCount + QTox.inviteCount

                visible: count > 0
                text: qsTr("Requests (%1)").arg(count)
                onClicked: page.StackView.view.push(requestsPage)
            }

            ToolButton {
                text: "⋮"
                Accessible.name: qsTr("More options")
                font.pixelSize: 20
                onClicked: menu.open()

                Menu {
                    id: menu

                    // Under the button, inside the window.
                    x: parent.width - width
                    y: parent.height

                    MenuItem {
                        text: qsTr("Search")
                        onTriggered: {
                            page.searching = true;
                            searchField.forceActiveFocus();
                        }
                    }

                    MenuItem {
                        text: qsTr("Add friend")
                        onTriggered: page.StackView.view.push(addFriendPage)
                    }

                    MenuItem {
                        text: qsTr("New conference")
                        onTriggered: newConferenceDialog.open()
                    }

                    MenuItem {
                        text: qsTr("New group")
                        onTriggered: newGroupDialog.open()
                    }

                    MenuItem {
                        text: qsTr("Join a group")
                        onTriggered: joinGroupDialog.open()
                    }

                    MenuItem {
                        text: qsTr("Your Tox ID")
                        onTriggered: page.StackView.view.push(toxIdPage)
                    }

                    MenuItem {
                        text: qsTr("Settings")
                        onTriggered: page.StackView.view.push(settingsPage)
                    }
                }
            }
        }
    }

    // When a chat's last message came: the time today, else the day.
    function describeTime(time) {
        const today = new Date();
        today.setHours(0, 0, 0, 0);
        return time >= today ? time.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
                             : time.toLocaleDateString(Qt.locale(), Locale.ShortFormat);
    }

    ListView {
        id: list

        anchors.fill: parent
        model: QTox.foundChats

        delegate: ListRow {
            id: chat

            required property int kind
            required property string chatId
            required property string name
            required property string statusMessage
            required property string topic
            required property bool online
            required property int status
            required property string lastMessage
            required property string lastSender
            required property bool lastIsSelf
            required property bool typing
            required property date lastTime
            required property int unread
            required property string avatarHash

            // Conferences and groups, which have many people in them.
            readonly property bool isMultiUser: kind !== ChatKind.Friend
            // Under the name: what's going on in the chat.
            readonly property string preview: {
                if (typing) {
                    return qsTr("typing…");
                }
                if (lastMessage.length === 0) {
                    return kind === ChatKind.Group ? topic : statusMessage;
                }
                // Conferences and groups say who wrote it.
                if (isMultiUser && lastIsSelf) {
                    return qsTr("You: %1").arg(lastMessage);
                }
                if (isMultiUser && lastSender.length > 0) {
                    // In one go: the name and message may contain "%2".
                    return qsTr("%1: %2").replace(/%[12]/g, marker => marker === "%1" ? lastSender : lastMessage);
                }
                return lastMessage;
            }

            width: ListView.view.width
            // The row in one go, as it reads on screen.
            Accessible.name: [name, isMultiUser ? "" : dot.description, unread > 0 ? qsTr("%n unread", "", unread) : "",
                              preview, time.text].filter(part => part.length > 0).join(", ")
            onClicked: page.StackView.view.push(chatPage, {
                kind: chat.kind,
                chatId: chat.chatId
            })

            contentItem: RowLayout {
                spacing: 12

                Avatar {
                    source: QTox.avatarSource(chat.kind, chat.chatId, chat.avatarHash)
                    name: chat.isMultiUser ? "#" : chat.name

                    StatusDot {
                        id: dot

                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        width: 12
                        height: 12
                        visible: !chat.isMultiUser
                        border.width: 2
                        border.color: Material.background
                        status: chat.status
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    RowLayout {
                        Label {
                            Layout.fillWidth: true
                            text: chat.name
                            font.bold: true
                            elide: Text.ElideRight
                            Accessible.ignored: true
                        }

                        Label {
                            id: time

                            visible: !isNaN(chat.lastTime)
                            text: visible ? page.describeTime(chat.lastTime) : ""
                            font.pixelSize: 12
                            color: chat.unread > 0 ? Material.accent : Material.foreground
                            opacity: chat.unread > 0 ? 1 : 0.7
                            Accessible.ignored: true
                        }
                    }

                    RowLayout {
                        Label {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: chat.preview
                            textFormat: Text.PlainText
                            maximumLineCount: 1
                            opacity: 0.7
                            elide: Text.ElideRight
                            Accessible.ignored: true
                        }

                        Label {
                            // A circle, longer for longer numbers.
                            Layout.preferredWidth: Math.max(implicitWidth, implicitHeight)
                            visible: chat.unread > 0
                            topPadding: 2
                            bottomPadding: 2
                            leftPadding: 6
                            rightPadding: 6
                            horizontalAlignment: Text.AlignHCenter
                            text: chat.unread
                            font.pixelSize: 12
                            font.bold: true
                            color: "white"
                            Accessible.ignored: true

                            background: Rectangle {
                                radius: height / 2
                                color: Material.accent
                            }
                        }
                    }
                }
            }
        }
    }

    Label {
        anchors.centerIn: parent
        width: parent.width - 64
        visible: list.count === 0
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        opacity: 0.7
        text: QTox.chatFilter ? qsTr("No chats with that name.")
                              : qsTr("No friends yet. Add one from the ⋮ menu, or give them your Tox ID.")
    }

    Component {
        id: chatPage

        ChatPage {}
    }

    Component {
        id: addFriendPage

        AddFriendPage {}
    }

    Component {
        id: requestsPage

        RequestsPage {}
    }

    Dialog {
        id: newConferenceDialog

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: qsTr("New conference")
        onOpened: {
            conferenceTitle.clear();
            conferenceTitle.forceActiveFocus();
        }
        onAccepted: {
            page.waitingFor = ChatKind.Conference;
            QTox.createConference(conferenceTitle.text, "");
        }

        TextField {
            id: conferenceTitle

            width: parent.width
            placeholderText: qsTr("Title")
            onAccepted: newConferenceDialog.accept()
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Cancel")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("Create")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    Dialog {
        id: newGroupDialog

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: qsTr("New group")
        onOpened: {
            groupName.clear();
            groupName.forceActiveFocus();
        }
        onAccepted: {
            page.waitingFor = ChatKind.Group;
            QTox.createGroup(groupName.text, "");
        }

        ColumnLayout {
            width: parent.width

            TextField {
                id: groupName

                Layout.fillWidth: true
                placeholderText: qsTr("Name")
                onAccepted: {
                    if (text.trim().length > 0)
                        newGroupDialog.accept();
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Anyone with its ID can join a public group.")
                wrapMode: Text.Wrap
                font.pixelSize: 12
                opacity: 0.7
            }
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Cancel")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                enabled: groupName.text.trim().length > 0
                text: qsTr("Create")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    Dialog {
        id: joinGroupDialog

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: qsTr("Join a group")
        onOpened: {
            groupId.clear();
            groupPassword.clear();
            joinError.text = "";
            groupId.forceActiveFocus();
        }

        ColumnLayout {
            width: parent.width

            TextField {
                id: groupId

                Layout.fillWidth: true
                placeholderText: qsTr("Group ID")
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            }

            TextField {
                id: groupPassword

                Layout.fillWidth: true
                placeholderText: qsTr("Password, if it has one")
                echoMode: TextInput.Password
            }

            Label {
                id: joinError

                Layout.fillWidth: true
                visible: text.length > 0
                wrapMode: Text.Wrap
                color: Material.color(Material.Red)
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
                onClicked: {
                    page.waitingFor = ChatKind.Group;
                    // Stays open to say what's wrong.
                    joinError.text = QTox.joinGroup(groupId.text, groupPassword.text);
                    if (joinError.text.length === 0)
                        joinGroupDialog.close();
                }
            }
        }
    }

    Connections {
        target: QTox

        function onChatAdded(kind, id) {
            if (kind !== page.waitingFor)
                return;
            page.waitingFor = -1;
            page.StackView.view.push(chatPage, {
                kind: kind,
                chatId: id
            });
        }
    }

    Component {
        id: sharePage

        SharePage {}
    }

    Component {
        id: toxIdPage

        ToxIdPage {}
    }

    Component {
        id: settingsPage

        SettingsPage {}
    }
}
