// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

Page {
    id: page

    // A ChatKind, and a friend's public key or a conference's or group's ID.
    required property int kind
    required property string chatId
    readonly property var chat: QTox.chat(kind, chatId)
    // What the message box starts with.
    property string draft
    // Whether the header looks for messages, and the row of the one it found, or -1.
    property bool searching: false
    property int foundRow: -1

    StackView.onActivated: QTox.showingChat(kind, chatId)
    StackView.onDeactivated: QTox.hidingChat(kind, chatId)
    Component.onDestruction: QTox.hidingChat(kind, chatId)

    // A message's day, as the model's yyyy-MM-dd says it.
    function describeDay(day) {
        const date = Date.fromLocaleDateString(Qt.locale("C"), day, "yyyy-MM-dd");
        const today = new Date();
        today.setHours(0, 0, 0, 0);
        // Rounded, as a day can be an hour longer or shorter.
        switch (Math.round((today - date) / (24 * 60 * 60 * 1000))) {
        case 0:
            return qsTr("Today");
        case 1:
            return qsTr("Yesterday");
        default:
            return date.toLocaleDateString(Qt.locale(), Locale.LongFormat);
        }
    }

    function showDetails() {
        const details = {
            [ChatKind.Friend]: friendDetailsPage,
            [ChatKind.Conference]: conferenceDetailsPage,
            [ChatKind.Group]: groupDetailsPage
        };
        page.StackView.view.push(details[page.kind], {
            chatId: page.chatId
        });
    }

    // A file's size, as the Widgets UI says it.
    function describeSize(bytes) {
        const units = ["B", "KiB", "MiB", "GiB", "TiB"];
        let unit = 0;
        while (bytes >= 1024 && unit < units.length - 1) {
            bytes /= 1024;
            ++unit;
        }
        return "%1 %2".arg(Number(bytes).toLocaleString(Qt.locale(), "f", unit === 0 ? 0 : 1)).arg(units[unit]);
    }

    // How a file transfer is going.
    function describeFile(file) {
        switch (file.fileStatus) {
        case "waiting":
            return file.isSelf ? qsTr("Waiting for them to accept") : describeSize(file.fileSize);
        case "transmitting":
            return qsTr("%1 of %2").arg(describeSize(file.fileTransferred)).arg(describeSize(file.fileSize));
        case "paused":
            return qsTr("Paused");
        case "broken":
            return qsTr("Failed");
        case "canceled":
            return qsTr("Cancelled");
        default:
            return describeSize(file.fileSize);
        }
    }

    // When a message was sent and whether the friend got it: with a tick on screen, in words when spoken.
    function describeStamp(message, spoken) {
        const time = message.time.toLocaleTimeString(Qt.locale(), Locale.ShortFormat);
        if (message.broken) {
            return spoken ? qsTr("Not sent, %1").arg(time) : qsTr("Not sent · %1").arg(time);
        }
        if (!message.showDelivery) {
            return time;
        }
        if (message.pending) {
            return spoken ? qsTr("Sending, %1").arg(time) : qsTr("Sending · %1").arg(time);
        }
        return spoken ? qsTr("%1, delivered").arg(time) : time + " ✓";
    }

    // A message as a screen reader says it: who, what, and when.
    function describeMessage(message, sender) {
        const who = message.kind === "system" ? "" : message.showSender ? sender : message.isSelf ? qsTr("You") : message.sender;
        const what = message.kind === "file" ? [message.text, describeFile(message)] : [message.text];
        return [who, ...what, describeStamp(message, true)].filter(part => part.length > 0).join(", ");
    }

    function send() {
        messages.following = true;
        page.chat.sendMessage(input.text);
        input.clear();
    }

    // From the newest, else past the one found.
    function findNext(backward) {
        const from = page.foundRow < 0 ? messages.count - 1 : page.foundRow + (backward ? -1 : 1);
        if (from < 0 || from >= messages.count) {
            QTox.notice(qsTr("No more messages with it"));
            return;
        }
        page.chat.find(searchField.text, from, backward);
    }

    // A tox: link adds a friend; the others open in their apps.
    function openLink(link) {
        if (link.startsWith("tox:")) {
            page.StackView.view.push(addFriendPage, {
                startId: link.substring(4)
            });
        } else {
            Qt.openUrlExternally(link);
        }
    }

    function stopSearch() {
        page.searching = false;
        page.foundRow = -1;
        searchField.clear();
    }

    Connections {
        target: page.chat

        function onFound(row) {
            if (row < 0) {
                QTox.notice(page.foundRow < 0 ? qsTr("No messages with it") : qsTr("No more messages with it"));
                return;
            }
            page.foundRow = row;
            messages.following = false;
            messages.positionViewAtIndex(row, ListView.Center);
        }
    }

    header: ToolBar {
        // Looking for messages.
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
                onAccepted: page.findNext(true)
                onTextChanged: page.foundRow = -1
            }

            ToolButton {
                text: "▲"
                Accessible.name: qsTr("Older match")
                enabled: searchField.text.length > 0
                onClicked: page.findNext(true)
            }

            ToolButton {
                text: "▼"
                Accessible.name: qsTr("Newer match")
                enabled: page.foundRow >= 0
                onClicked: page.findNext(false)
            }
        }

        RowLayout {
            anchors.fill: parent
            visible: !page.searching

            BackButton {
                onClicked: page.StackView.view.pop()
            }

            Avatar {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                source: page.chat.avatar
                name: page.kind === ChatKind.Friend ? page.chat.name : "#"
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                TapHandler {
                    onTapped: page.showDetails()
                }

                Label {
                    Layout.fillWidth: true
                    text: page.chat.name
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    visible: page.chat.typing
                    text: qsTr("typing…")
                    font.pixelSize: 12
                    font.italic: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: page.chat.topic
                    textFormat: Text.PlainText
                    font.pixelSize: 12
                    opacity: 0.7
                    elide: Text.ElideRight
                }
            }

            ToolButton {
                visible: page.kind === ChatKind.Friend
                enabled: page.chat.online && QTox.callState === CallState.None
                text: "🎥"
                Accessible.name: qsTr("Video call")
                font.pixelSize: 20
                onClicked: QTox.startCall(page.chatId, true)
            }

            ToolButton {
                readonly property bool isFriend: page.kind === ChatKind.Friend

                visible: isFriend || page.chat.audio
                enabled: (!isFriend || page.chat.online) && QTox.callState === CallState.None
                text: "☎"
                Accessible.name: isFriend ? qsTr("Call") : qsTr("Join the call")
                font.pixelSize: 20
                onClicked: isFriend ? QTox.startCall(page.chatId) : QTox.joinConferenceCall(page.chatId)
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
                        text: qsTr("Details")
                        onTriggered: page.showDetails()
                    }

                    MenuItem {
                        text: qsTr("Search")
                        onTriggered: {
                            page.searching = true;
                            searchField.forceActiveFocus();
                        }
                    }

                    MenuItem {
                        visible: page.kind !== ChatKind.Friend
                        height: visible ? implicitHeight : 0
                        text: qsTr("Leave")
                        onTriggered: {
                            if (page.kind === ChatKind.Group)
                                QTox.leaveGroup(page.chatId);
                            else
                                QTox.leaveConference(page.chatId);
                            page.StackView.view.pop();
                        }
                    }
                }
            }
        }
    }

    FollowingListView {
        id: messages

        anchors.fill: parent
        anchors.margins: 8
        spacing: 6
        clip: true
        model: page.chat.messages

        section.property: "day"
        section.delegate: Label {
            required property string section

            width: ListView.view.width
            // Screen readers go from day to day.
            Accessible.role: Accessible.Heading
            // A message's day comes with its text.
            height: section ? implicitHeight : 0
            visible: section
            topPadding: 8
            bottomPadding: 2
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 12
            opacity: 0.7
            text: section ? page.describeDay(section) : ""
        }

        delegate: Item {
            id: message

            required property int index
            required property string kind
            required property string text
            required property string sender
            required property bool isSelf
            required property date time
            required property bool pending
            required property bool broken
            required property bool isAction
            required property string fileStatus
            required property real fileSize
            required property real fileTransferred
            required property int fileNum
            required property string filePath
            required property bool filePausedByUser
            required property string html
            required property bool isPrivate
            required property string recipient

            // Conference and group messages say who sent them, and private ones to whom.
            readonly property bool showSender: page.kind !== ChatKind.Friend && kind === "message" && (!isSelf || isPrivate)
            // Only friends say they got a message.
            readonly property bool showDelivery: page.kind === ChatKind.Friend && isSelf && kind === "message"

            function showMenu(position) {
                messageMenu.text = text;
                messageMenu.filePath = fileStatus === "finished" ? filePath : "";
                messageMenu.popup(bubble, position);
            }

            width: ListView.view.width
            height: bubble.height

            Rectangle {
                id: bubble

                anchors.right: message.isSelf ? parent.right : undefined
                anchors.horizontalCenter: message.kind === "system" ? parent.horizontalCenter : undefined
                width: Math.min(Math.max(body.implicitWidth, senderName.implicitWidth, stamp.implicitWidth, file.implicitWidth), message.width * 0.8) + 20
                height: file.y + file.height + stamp.implicitHeight + 8
                radius: 12
                // The message that the search found.
                border.width: message.index === page.foundRow ? 2 : 0
                border.color: Material.accent
                color: message.kind === "system" ? "transparent"
                    : message.isSelf ? Material.color(Material.Blue, Material.Shade700)
                    : Material.color(Material.Grey, Material.theme === Material.Dark ? Material.Shade800 : Material.Shade200)
                // The message in one go, its labels left out. Screen readers open its menu with a tap, as
                // they can't long-press.
                Accessible.role: Accessible.ListItem
                Accessible.name: page.describeMessage(message, senderName.text)
                Accessible.onPressAction: {
                    if (message.kind !== "system")
                        message.showMenu(Qt.point(width / 2, height / 2));
                }

                TapHandler {
                    enabled: message.kind !== "system"
                    onLongPressed: message.showMenu(point.position)
                }

                Label {
                    id: senderName

                    x: 10
                    y: 6
                    width: Math.min(implicitWidth, message.width * 0.8)
                    visible: message.showSender
                    height: visible ? implicitHeight : 0
                    elide: Text.ElideRight
                    font.bold: true
                    font.pixelSize: 12
                    color: message.isSelf ? "white" : Material.accent
                    Accessible.ignored: true
                    text: !message.isPrivate ? message.sender
                        : message.isSelf ? qsTr("Privately to %1").arg(message.recipient)
                        : qsTr("%1, privately").arg(message.sender)
                }

                Label {
                    id: body

                    x: 10
                    y: senderName.y + senderName.height + (senderName.visible ? 2 : 0)
                    width: Math.min(implicitWidth, message.width * 0.8)
                    wrapMode: Text.Wrap
                    // Messages are formatted, as the Widgets UI shows them.
                    textFormat: message.kind === "message" ? Text.StyledText : Text.PlainText
                    font.italic: message.isAction || message.kind === "system"
                    font.bold: message.kind === "file"
                    color: message.isSelf ? "white" : Material.foreground
                    linkColor: message.isSelf ? "white" : Material.accent
                    Accessible.ignored: true
                    text: message.kind === "message" ? message.html : message.text
                    onLinkActivated: link => page.openLink(link)
                }

                // File controls, for files only: making them for every message slowed opening a chat.
                Loader {
                    id: file

                    x: 10
                    y: body.y + body.implicitHeight + (active ? 4 : 0)
                    active: message.kind === "file"

                    sourceComponent: Component {
                        ColumnLayout {
                            id: fileControls

                            readonly property bool transferring: message.fileStatus === "transmitting" || message.fileStatus === "paused"

                            width: Math.min(implicitWidth, message.width * 0.8)
                            spacing: 2

                            // A picture, as the Widgets UI previews them.
                            Image {
                                readonly property int side: 240

                                Layout.preferredWidth: Math.min(side, implicitWidth / Screen.devicePixelRatio)
                                Layout.preferredHeight: implicitWidth > 0 ? Layout.preferredWidth * implicitHeight / implicitWidth : 0
                                visible: status === Image.Ready
                                asynchronous: true
                                fillMode: Image.PreserveAspectFit
                                sourceSize: Qt.size(side * Screen.devicePixelRatio, side * Screen.devicePixelRatio)
                                source: message.fileStatus === "finished" && message.filePath.length > 0
                                    ? "image://preview/" + encodeURIComponent(message.filePath) : ""

                                TapHandler {
                                    onTapped: QTox.openFile(message.filePath)
                                }
                            }

                            Label {
                                text: page.describeFile(message)
                                font.pixelSize: 12
                                color: body.color
                                opacity: 0.8
                                Accessible.ignored: true
                            }

                            ProgressBar {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 160
                                visible: fileControls.transferring
                                value: message.fileSize > 0 ? message.fileTransferred / message.fileSize : 0
                            }

                            RowLayout {
                                spacing: 4

                                Button {
                                    visible: message.fileStatus === "waiting" && !message.isSelf
                                    text: qsTr("Accept")
                                    highlighted: true
                                    onClicked: page.chat.acceptFile(message.fileNum)
                                }

                                // As the Widgets UI: Resume once the user paused it, else Pause.
                                Button {
                                    visible: fileControls.transferring
                                    text: message.fileStatus === "paused" && message.filePausedByUser ? qsTr("Resume") : qsTr("Pause")
                                    flat: true
                                    Material.foreground: body.color
                                    onClicked: page.chat.pauseFile(message.fileNum)
                                }

                                Button {
                                    visible: message.fileStatus === "waiting" || fileControls.transferring
                                    text: message.fileStatus === "waiting" && !message.isSelf ? qsTr("Decline") : qsTr("Cancel")
                                    flat: true
                                    Material.foreground: body.color
                                    onClicked: page.chat.cancelFile(message.fileNum, message.isSelf)
                                }

                                Button {
                                    visible: message.fileStatus === "finished" && message.filePath.length > 0
                                    text: qsTr("Open")
                                    flat: true
                                    Material.foreground: body.color
                                    onClicked: QTox.openFile(message.filePath)
                                }
                            }
                        }
                    }
                }

                Label {
                    id: stamp

                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.top: file.bottom
                    font.pixelSize: 11
                    opacity: 0.7
                    color: body.color
                    Accessible.ignored: true
                    text: page.describeStamp(message, false)
                }
            }
        }
    }

    footer: Pane {
        padding: 8

        ColumnLayout {
            anchors.fill: parent

            RowLayout {
                Layout.fillWidth: true
                visible: page.chat.privateTo.length > 0

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Privately to %1").arg(page.chat.privateToName)
                    font.bold: true
                    color: Material.accent
                    elide: Text.ElideRight
                }

                ToolButton {
                    text: "✕"
                    Accessible.name: qsTr("Write to everyone")
                    focusPolicy: Qt.NoFocus
                    onClicked: page.chat.privateTo = ""
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.margins: 8
                visible: !page.chat.canWrite
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                opacity: 0.7
                text: page.chat.selfRole === PeerRole.Observer ? qsTr("Observers can't write in this group.")
                    : page.chat.voiceState === VoiceState.Founder ? qsTr("Only the founder can write to everyone in this group. You can write to someone privately from the group's details.")
                    : qsTr("Only moderators can write to everyone in this group. You can write to someone privately from the group's details.")
            }

            RowLayout {
                Layout.fillWidth: true
                visible: page.chat.canWrite

                ToolButton {
                    visible: page.kind === ChatKind.Friend
                    text: "+"
                    Accessible.name: qsTr("Send a file")
                    font.pixelSize: 24
                    focusPolicy: Qt.NoFocus
                    onClicked: filePicker.open()
                }

                TextField {
                    id: input

                    Layout.fillWidth: true
                    placeholderText: qsTr("Message")
                    text: page.draft
                    onAccepted: page.send()
                    onTextChanged: page.chat.setDraft(text)
                }

                Button {
                    text: qsTr("Send")
                    highlighted: true
                    // The keyboard stays open for the next message.
                    focusPolicy: Qt.NoFocus
                    enabled: input.text.trim().length > 0
                    onClicked: page.send()
                }
            }
        }
    }

    FileDialog {
        id: filePicker

        title: qsTr("Send a file")
        onAccepted: page.chat.sendFile(selectedFile)
    }

    // Shared by the messages: a menu each slowed opening a chat.
    Menu {
        id: messageMenu

        // The message's, and its file, if it's a finished one.
        property string text
        property string filePath

        MenuItem {
            text: qsTr("Copy")
            onTriggered: QTox.copyToClipboard(messageMenu.text)
        }

        MenuItem {
            visible: messageMenu.filePath.length > 0
            height: visible ? implicitHeight : 0
            text: qsTr("Save as…")
            onTriggered: {
                fileSaver.source = messageMenu.filePath;
                fileSaver.selectedFile = messageMenu.text;
                fileSaver.open();
            }
        }

        MenuItem {
            visible: messageMenu.filePath.length > 0 && QTox.canShare()
            height: visible ? implicitHeight : 0
            text: qsTr("Share")
            onTriggered: QTox.shareFile(messageMenu.filePath)
        }
    }

    FileDialog {
        id: fileSaver

        // The file to copy.
        property string source

        title: qsTr("Save a file")
        fileMode: FileDialog.SaveFile
        onAccepted: QTox.saveFile(source, selectedFile)
    }

    Component {
        id: friendDetailsPage

        FriendDetailsPage {}
    }

    Component {
        id: addFriendPage

        AddFriendPage {}
    }

    Component {
        id: conferenceDetailsPage

        ConferenceDetailsPage {}
    }

    Component {
        id: groupDetailsPage

        GroupDetailsPage {}
    }
}
