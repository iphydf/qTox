// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Where to send what another app shared.
Page {
    id: page

    // A file to send, or text for the message box.
    property url file
    property string text

    readonly property bool isFile: file.toString().length > 0

    function choose(kind, chatId) {
        if (page.isFile) {
            QTox.chat(kind, chatId).sendFile(page.file);
        }
        page.StackView.view.replace(page, chatPage, {
            kind: kind,
            chatId: chatId,
            draft: page.isFile ? "" : page.text
        });
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent

            BackButton {
                onClicked: page.StackView.view.pop()
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Send to")
                font.bold: true
            }
        }
    }

    ListView {
        anchors.fill: parent
        model: QTox.chats

        delegate: ListRow {
            id: chat

            required property int kind
            required property string chatId
            required property string name
            required property string avatarHash

            // Only friends take files.
            readonly property bool takes: !page.isFile || kind === ChatKind.Friend

            width: ListView.view.width
            height: takes ? implicitHeight : 0
            visible: takes
            Accessible.name: name
            onClicked: page.choose(kind, chatId)

            contentItem: RowLayout {
                spacing: 12

                Avatar {
                    source: QTox.avatarSource(chat.kind, chat.chatId, chat.avatarHash)
                    name: chat.kind === ChatKind.Friend ? chat.name : "#"
                }

                Label {
                    Layout.fillWidth: true
                    text: chat.name
                    elide: Text.ElideRight
                    Accessible.ignored: true
                }
            }
        }
    }

    Component {
        id: chatPage

        ChatPage {}
    }
}
