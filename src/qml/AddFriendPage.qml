// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page

    // What the Tox ID field starts with.
    property string startId

    header: ToolBar {
        RowLayout {
            anchors.fill: parent

            BackButton {
                onClicked: page.StackView.view.pop()
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Add friend")
                font.bold: true
            }
        }
    }

    ColumnLayout {
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: 16
        }
        spacing: 8

        TextField {
            id: toxId

            Layout.fillWidth: true
            placeholderText: qsTr("Tox ID")
            text: page.startId
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
        }

        TextArea {
            id: message

            Layout.fillWidth: true
            placeholderText: qsTr("Message (optional)")
            wrapMode: TextEdit.Wrap
        }

        Label {
            id: problem

            Layout.fillWidth: true
            visible: text.length > 0
            wrapMode: Text.Wrap
            color: "red"
        }

        Button {
            Layout.fillWidth: true
            text: qsTr("Send friend request")
            highlighted: true
            enabled: toxId.text.trim().length > 0
            onClicked: {
                problem.text = QTox.addFriend(toxId.text, message.text);
                if (problem.text.length === 0)
                    page.StackView.view.pop();
            }
        }
    }
}
