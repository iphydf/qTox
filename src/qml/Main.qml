// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

ApplicationWindow {
    id: window

    width: 420
    height: 760
    visible: true
    // Under the system's bars: Qt keeps the content clear of them.
    flags: Qt.Window | Qt.ExpandedClientAreaHint
    title: "qTox"
    Material.theme: Material.System
    Material.accent: Material.Blue

    // Closes a search, goes back a page, or from the first page goes to the background on Android:
    // false if there's nowhere to go.
    function goBack() {
        // Only the friends and chat pages have these: pages_test's searchChats checks them.
        if (stack.currentItem.searching) { // qmllint disable missing-property
            stack.currentItem.stopSearch(); // qmllint disable missing-property
        } else if (stack.depth > 1) {
            stack.pop();
        } else if (Qt.platform.os === "android") {
            QTox.moveToBack();
        } else {
            return false;
        }
        return true;
    }

    // Android's back button: Qt moves qTox to the background when nothing takes it.
    Shortcut {
        sequence: "Back"
        onActivated: window.goBack()
    }

    onClosing: close => close.accepted = !window.goBack()

    // What the service is ready for: starting, logging in, or chatting.
    readonly property Component page: !QTox.connected ? startingPage
        : QTox.loggedIn ? friendsPage : loginPage

    onPageChanged: stack.replace(null, page)

    StackView {
        id: stack

        // The window stays as big as the screen, also under the keyboard.
        anchors.fill: parent
        anchors.leftMargin: SystemInsets.left
        anchors.topMargin: SystemInsets.top + (callBar.visible ? callBar.height : 0)
        anchors.rightMargin: SystemInsets.right
        anchors.bottomMargin: SystemInsets.bottom
        initialItem: window.page
    }

    // The friend's video in the call over the pages, and in a corner what the user's camera sends.
    Rectangle {
        anchors.fill: stack
        z: 1
        visible: (QTox.callVideo.length > 0 || QTox.selfVideo.length > 0) && !callBar.videoHidden
        color: "black"

        Image {
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            cache: false
            source: QTox.callVideo
        }

        Image {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 16
            width: parent.width / 4
            height: parent.height / 4
            fillMode: Image.PreserveAspectFit
            horizontalAlignment: Image.AlignRight
            verticalAlignment: Image.AlignBottom
            cache: false
            source: QTox.selfVideo
        }
    }

    CallBar {
        id: callBar

        y: SystemInsets.top
        x: SystemInsets.left
        width: parent.width - SystemInsets.left - SystemInsets.right
    }

    Component {
        id: startingPage

        Page {
            BusyIndicator {
                anchors.centerIn: parent
            }
        }
    }

    Component {
        id: loginPage

        LoginPage {}
    }

    Component {
        id: friendsPage

        FriendsPage {}
    }

    // A toast, as Android shows them.
    ToolTip {
        id: notice

        x: (window.width - width) / 2
        y: window.height - window.bottomPadding - SystemInsets.bottom - height - 48

        // Never HTML, as notices can contain names.
        contentItem: Label {
            text: notice.text
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            font: notice.font
            color: notice.Material.toolTextColor
        }
    }

    Dialog {
        id: errorDialog

        property alias text: errorText.text

        anchors.centerIn: parent
        width: Math.min(window.width - 32, 400)
        modal: true
        standardButtons: Dialog.Ok
        // The style's, for the title: never HTML either.
        Component.onCompleted: (header as Label).textFormat = Text.PlainText

        Label {
            id: errorText

            width: parent.width
            // Never HTML, as errors can name files and profiles.
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }
    }

    Connections {
        target: QTox

        function onError(title, text) {
            // The service's own words, which the lint can't know.
            errorDialog.title = title; // qmllint disable Plugin.qTox.untrusted-text
            errorDialog.text = text;
            errorDialog.open();
        }

        function onNotice(text) {
            notice.show(text, 2000);
        }
    }
}
