// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The call, over every page: with whom, how it's going, and what the user can do.
Pane {
    id: bar

    readonly property var chat: QTox.callChat ? QTox.chat(QTox.callKind, QTox.callChat) : null
    readonly property bool active: QTox.callState === CallState.Active
    // Since the call became active.
    property int seconds: 0
    // Whether the user put the friend's video away.
    property bool videoHidden: false

    // A duration as the Android dialler shows it: 1:05, or 1:02:05.
    function describeDuration(total) {
        const hours = Math.floor(total / 3600);
        const minutes = Math.floor(total / 60) % 60;
        const secondsText = String(total % 60).padStart(2, "0");
        return hours > 0 ? "%1:%2:%3".arg(hours).arg(String(minutes).padStart(2, "0")).arg(secondsText)
                         : "%1:%2".arg(minutes).arg(secondsText);
    }

    visible: QTox.callState !== CallState.None
    Material.background: Material.color(Material.Green, Material.Shade800)
    Material.foreground: "white"

    Timer {
        interval: 1000
        repeat: true
        triggeredOnStart: true
        running: bar.active
        onTriggered: bar.seconds = Math.max(0, (Date.now() - QTox.callStarted.getTime()) / 1000)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Avatar {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                source: bar.chat ? bar.chat.avatar : ""
                name: !bar.chat ? "" : QTox.callKind === ChatKind.Conference ? "#" : bar.chat.name
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    text: bar.chat ? bar.chat.name : ""
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: {
                        switch (QTox.callState) {
                        case CallState.Incoming:
                            return QTox.callOffersVideo ? qsTr("Incoming video call") : qsTr("Incoming call");
                        case CallState.Outgoing:
                            return qsTr("Calling…");
                        default:
                            return bar.describeDuration(bar.seconds);
                        }
                    }
                    opacity: 0.8
                    elide: Text.ElideRight
                }
            }

            Button {
                visible: QTox.callState === CallState.Incoming
                text: qsTr("Answer")
                highlighted: true
                Material.accent: Material.color(Material.Green)
                // As the friend calls.
                onClicked: QTox.answerCall(QTox.callOffersVideo)
            }

            Button {
                text: QTox.callState === CallState.Incoming ? qsTr("Decline") : qsTr("End")
                highlighted: true
                Material.accent: Material.color(Material.Red)
                onClicked: QTox.endCall()
            }
        }

        // What the user can change in the call, on as many rows as it takes.
        Flow {
            Layout.fillWidth: true
            visible: QTox.callState === CallState.Outgoing || bar.active
                     || (QTox.callState === CallState.Incoming && QTox.callOffersVideo)

            Button {
                visible: QTox.callState === CallState.Incoming
                text: qsTr("Answer without video")
                flat: true
                onClicked: QTox.answerCall(false)
            }

            Button {
                visible: QTox.hasLoudspeakerChoice
                text: QTox.callLoudspeaker ? qsTr("Earpiece") : qsTr("Speaker")
                flat: true
                onClicked: QTox.toggleCallLoudspeaker()
            }

            Button {
                visible: bar.active
                text: QTox.callMicMuted ? qsTr("Unmute") : qsTr("Mute")
                flat: true
                onClicked: QTox.toggleCallMic()
            }

            Button {
                visible: bar.active
                text: QTox.callSpeakerMuted ? qsTr("Sound on") : qsTr("Sound off")
                flat: true
                onClicked: QTox.toggleCallSpeaker()
            }

            Button {
                visible: QTox.callSendsVideo && QTox.canSwitchCamera
                text: qsTr("Switch camera")
                flat: true
                onClicked: QTox.switchCamera()
            }

            Button {
                visible: QTox.callVideo.length > 0 || QTox.selfVideo.length > 0
                text: bar.videoHidden ? qsTr("Video") : qsTr("Hide video")
                flat: true
                onClicked: bar.videoHidden = !bar.videoHidden
            }
        }
    }
}
