// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
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
                text: qsTr("Your Tox ID")
                font.bold: true
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

            Label {
                Layout.fillWidth: true
                text: qsTr("Friends add you with this ID. They can scan the code or you can send them the text.")
                wrapMode: Text.Wrap
            }

            Image {
                id: qr

                readonly property int side: Math.min(column.width, 320)

                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: side
                Layout.preferredHeight: side
                // As the Widgets UI encodes it.
                source: QTox.selfId ? "image://qr/" + encodeURIComponent("tox:" + QTox.selfId) : ""
                sourceSize: Qt.size(side, side)
                fillMode: Image.PreserveAspectFit
                smooth: false
                Accessible.role: Accessible.Graphic
                Accessible.name: qsTr("QR code of your Tox ID")
            }

            // As the Widgets UI colours it: the public key, the NoSpam, the checksum.
            Label {
                Layout.fillWidth: true
                textFormat: Text.StyledText
                text: QTox.selfId.substring(0, 64) + "<font color=\"" + Material.color(Material.Blue)
                      + "\">" + QTox.selfId.substring(64, 72) + "</font><font color=\"grey\">"
                      + QTox.selfId.substring(72) + "</font>"
                wrapMode: Text.WrapAnywhere
                font.family: "monospace"
            }

            RowLayout {
                Button {
                    text: qsTr("Copy")
                    onClicked: {
                        QTox.copyToClipboard(QTox.selfId);
                        QTox.notice(qsTr("Copied"));
                    }
                }

                Button {
                    visible: QTox.canShare()
                    text: qsTr("Share")
                    onClicked: QTox.share(QTox.selfId)
                }
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("NoSpam")
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("If people you don't want to hear from have your ID, a new NoSpam stops their friend requests. Your friends stay.")
                wrapMode: Text.Wrap
            }

            Button {
                text: qsTr("New NoSpam")
                onClicked: nospamDialog.open()
            }
        }
    }

    Dialog {
        id: nospamDialog

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: qsTr("New NoSpam?")
        onAccepted: QTox.newNospam()

        Label {
            width: parent.width
            text: qsTr("Friend requests to your current ID won't arrive anymore. Give people your new ID instead.")
            wrapMode: Text.Wrap
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Cancel")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("Change")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }
}
