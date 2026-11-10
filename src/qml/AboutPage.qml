// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// As the Widgets UI's About: what qTox this is, and where to report what's wrong with it.
Page {
    id: page

    readonly property var versions: QTox.versions

    header: ToolBar {
        RowLayout {
            anchors.fill: parent

            BackButton {
                onClicked: page.StackView.view.pop()
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("About")
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
                text: qsTr("You are using qTox version %1.").arg(page.versions.version)
                wrapMode: Text.Wrap
            }

            Label {
                Layout.fillWidth: true
                textFormat: Text.StyledText
                text: qsTr("Commit hash: %1").arg("<a href=\"https://github.com/TokTok/qTox/commit/%1\">%1</a>".arg(page.versions.commit))
                wrapMode: Text.WrapAnywhere
                onLinkActivated: link => Qt.openUrlExternally(link)
            }

            Label {
                text: qsTr("toxcore version: %1").arg(page.versions.toxcore)
            }

            Label {
                text: qsTr("Qt version: %1").arg(page.versions.qt)
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("This version of qTox is being maintained by the TokTok team following the archiving of the original qTox project.")
                wrapMode: Text.Wrap
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Device details")
                font.bold: true
            }

            // What a report about how qTox looks needs: not translated, so that it reads the same
            // to whoever gets it.
            Label {
                id: details

                readonly property var appWindow: page.ApplicationWindow.window

                Layout.fillWidth: true
                wrapMode: Text.WrapAnywhere
                // Nothing once the page has left its window, as it does on its way out.
                text: !appWindow ? "" : [
                    "qTox " + page.versions.version + ", Qt " + page.versions.qtRuntime
                        + " (built with " + page.versions.qt + ")",
                    page.versions.system + ", " + page.versions.device,
                    "screen " + Screen.width + "x" + Screen.height + " @" + Screen.devicePixelRatio,
                    "window " + appWindow.width + "x" + appWindow.height
                        + " flags 0x" + appWindow.flags.toString(16),
                    "safe area padding " + appWindow.leftPadding + " " + appWindow.topPadding
                        + " " + appWindow.rightPadding + " " + appWindow.bottomPadding,
                    "keyboard insets " + SystemInsets.left + " " + SystemInsets.top + " "
                        + SystemInsets.right + " " + SystemInsets.bottom,
                    "page " + page.width + "x" + page.height,
                ].join("\n")
            }

            Button {
                text: qsTr("Copy details")
                onClicked: QTox.copyToClipboard(details.text)
            }

            Button {
                text: qsTr("Known issues")
                onClicked: Qt.openUrlExternally("https://github.com/TokTok/qTox/issues")
            }

            Button {
                text: qsTr("Report a bug")
                onClicked: Qt.openUrlExternally("https://github.com/TokTok/qTox/issues/new?labels=bug&template=bug.yml"
                    + "&qtox_version=" + encodeURIComponent(page.versions.version)
                    + "&commit_hash=" + encodeURIComponent(page.versions.commit)
                    + "&toxcore_version=" + encodeURIComponent(page.versions.toxcore)
                    + "&qt_version=" + encodeURIComponent(page.versions.qt))
            }
        }
    }
}
