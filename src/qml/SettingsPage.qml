// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
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
                text: qsTr("Settings")
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
                text: qsTr("Profile")
                font.bold: true
            }

            RowLayout {
                spacing: 12

                Avatar {
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 64
                    source: QTox.selfAvatar
                    name: QTox.selfName
                }

                ColumnLayout {
                    Button {
                        text: qsTr("Change picture")
                        onClicked: picturePicker.open()
                    }

                    Button {
                        visible: QTox.hasOwnAvatar
                        text: qsTr("Remove picture")
                        flat: true
                        onClicked: QTox.removeAvatar()
                    }
                }
            }

            TextField {
                Layout.fillWidth: true
                placeholderText: qsTr("Name")
                text: QTox.selfName
                onEditingFinished: {
                    if (text !== QTox.selfName)
                        QTox.selfName = text;
                }
            }

            TextField {
                Layout.fillWidth: true
                placeholderText: qsTr("Status message")
                text: QTox.selfStatusMessage
                onEditingFinished: {
                    if (text !== QTox.selfStatusMessage)
                        QTox.selfStatusMessage = text;
                }
            }

            Button {
                text: qsTr("Your Tox ID")
                onClicked: page.StackView.view.push(toxIdPage)
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Profile file")
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: QTox.hasPassword ? qsTr("%1, protected by a password").arg(QTox.profileName)
                                       : qsTr("%1, without a password").arg(QTox.profileName)
                wrapMode: Text.Wrap
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: QTox.hasPassword ? qsTr("Change password") : qsTr("Set password")
                    onClicked: passwordDialog.open()
                }

                Button {
                    visible: QTox.hasPassword
                    text: qsTr("Remove password")
                    onClicked: removePasswordDialog.open()
                }

                Button {
                    text: qsTr("Rename")
                    onClicked: renameDialog.open()
                }

                Button {
                    text: qsTr("Export")
                    onClicked: exportPicker.open()
                }

                Button {
                    text: qsTr("Delete")
                    Material.foreground: Material.color(Material.Red)
                    onClicked: deleteDialog.open()
                }
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Privacy")
                font.bold: true
            }

            WrappingSwitch {
                Layout.fillWidth: true
                text: qsTr("Let friends see when you're typing")
                checked: QTox.typingNotification
                onToggled: QTox.typingNotification = checked
            }

            WrappingSwitch {
                Layout.fillWidth: true
                text: qsTr("Say in chats when friends' statuses change")
                checked: QTox.statusChangeMessages
                onToggled: QTox.statusChangeMessages = checked
            }

            WrappingSwitch {
                Layout.fillWidth: true
                text: qsTr("Keep chat history")
                checked: QTox.keepHistory
                onToggled: {
                    QTox.keepHistory = checked;
                    // As the Widgets UI asks.
                    if (!checked)
                        eraseHistoryDialog.open();
                }
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Notifications")
                font.bold: true
            }

            WrappingSwitch {
                Layout.fillWidth: true
                text: qsTr("Show notifications")
                checked: QTox.notify
                onToggled: QTox.notify = checked
            }

            WrappingSwitch {
                Layout.fillWidth: true
                text: qsTr("Ring for calls")
                checked: QTox.notifySound
                onToggled: QTox.notifySound = checked
            }

            WrappingSwitch {
                Layout.fillWidth: true
                enabled: QTox.notifySound
                text: qsTr("Also while you're Busy")
                checked: QTox.busySound
                onToggled: QTox.busySound = checked
            }

            WrappingSwitch {
                Layout.fillWidth: true
                enabled: QTox.notify
                text: qsTr("For every conference and group message, not only those that mention you")
                checked: QTox.conferenceAlwaysNotify
                onToggled: QTox.conferenceAlwaysNotify = checked
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Files")
                font.bold: true
            }

            WrappingSwitch {
                Layout.fillWidth: true
                text: qsTr("Save files from every friend without asking")
                checked: QTox.autoSaveFiles
                onToggled: QTox.autoSaveFiles = checked
            }

            RowLayout {
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Up to this size, in MiB (0 for any)")
                    wrapMode: Text.Wrap
                }

                TextField {
                    Layout.preferredWidth: 100
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator {
                        bottom: 0
                    }
                    text: Math.round(QTox.maxAutoAcceptSize / (1024 * 1024))
                    onEditingFinished: QTox.maxAutoAcceptSize = Number(text) * 1024 * 1024
                }
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Language")
                font.bold: true
            }

            ComboBox {
                Layout.fillWidth: true
                model: QTox.languages
                textRole: "name"
                valueRole: "code"
                // Again once the model is there.
                currentIndex: count > 0 ? indexOfValue(QTox.language) : -1
                onActivated: QTox.language = currentValue
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Network")
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Changes apply the next time you log in.")
                wrapMode: Text.Wrap
                opacity: 0.7
            }

            WrappingSwitch {
                Layout.fillWidth: true
                text: qsTr("Use UDP")
                // Tox uses only TCP through a proxy.
                enabled: QTox.proxyType === 0
                checked: QTox.udpEnabled
                onToggled: QTox.udpEnabled = checked
            }

            WrappingSwitch {
                Layout.fillWidth: true
                text: qsTr("Use IPv6")
                checked: QTox.ipv6Enabled
                onToggled: QTox.ipv6Enabled = checked
            }

            WrappingSwitch {
                Layout.fillWidth: true
                text: qsTr("Find friends on the local network")
                // It needs UDP.
                enabled: QTox.udpEnabled && QTox.proxyType === 0
                checked: QTox.lanDiscoveryEnabled
                onToggled: QTox.lanDiscoveryEnabled = checked
            }

            Label {
                text: qsTr("Proxy")
            }

            ComboBox {
                Layout.fillWidth: true
                // In ICoreSettings::ProxyType's order.
                model: [qsTr("None"), "SOCKS5", "HTTP"]
                currentIndex: QTox.proxyType
                onActivated: QTox.proxyType = currentIndex
            }

            RowLayout {
                Layout.fillWidth: true
                visible: QTox.proxyType !== 0

                TextField {
                    Layout.fillWidth: true
                    placeholderText: qsTr("Address")
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhUrlCharactersOnly
                    text: QTox.proxyAddress
                    onEditingFinished: QTox.proxyAddress = text.trim()
                }

                TextField {
                    Layout.preferredWidth: 100
                    placeholderText: qsTr("Port")
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator {
                        bottom: 0
                        top: 65535
                    }
                    text: QTox.proxyPort > 0 ? QTox.proxyPort : ""
                    onEditingFinished: QTox.proxyPort = Number(text)
                }
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("About")
                flat: true
                onClicked: page.StackView.view.push(aboutPage)
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("Log out")
                onClicked: QTox.logout()
            }
        }
    }

    Component {
        id: aboutPage

        AboutPage {}
    }

    Component {
        id: toxIdPage

        ToxIdPage {}
    }

    FileDialog {
        id: exportPicker

        title: qsTr("Export profile")
        fileMode: FileDialog.SaveFile
        selectedFile: QTox.profileName + ".tox"
        onAccepted: QTox.exportProfile(selectedFile)
    }

    Dialog {
        id: passwordDialog

        // As the Widgets UI asks for it.
        readonly property string problem: newPassword.text.length < 6 ? qsTr("The password is too short.")
            : newPassword.text !== repeatedPassword.text ? qsTr("The password doesn't match.") : ""

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: QTox.hasPassword ? qsTr("Change password") : qsTr("Set password")
        onOpened: {
            newPassword.clear();
            repeatedPassword.clear();
            newPassword.forceActiveFocus();
        }
        onAccepted: QTox.setPassword(newPassword.text)

        ColumnLayout {
            width: parent.width

            Label {
                Layout.fillWidth: true
                text: qsTr("You need it to log in. Nobody can recover it if you forget it.")
                wrapMode: Text.Wrap
            }

            TextField {
                id: newPassword

                Layout.fillWidth: true
                placeholderText: qsTr("New password")
                echoMode: TextInput.Password
            }

            TextField {
                id: repeatedPassword

                Layout.fillWidth: true
                placeholderText: qsTr("Repeat password")
                echoMode: TextInput.Password
            }

            Label {
                Layout.fillWidth: true
                visible: newPassword.text.length > 0
                text: passwordDialog.problem
                wrapMode: Text.Wrap
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
                text: qsTr("OK")
                flat: true
                enabled: passwordDialog.problem === ""
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    Dialog {
        id: removePasswordDialog

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: qsTr("Remove password")
        onAccepted: QTox.setPassword("")

        Label {
            width: parent.width
            text: qsTr("Are you sure you want to remove your password?")
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
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    Dialog {
        id: renameDialog

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: qsTr("Rename \"%1\"").arg(QTox.profileName)
        onOpened: {
            newName.text = QTox.profileName;
            newName.selectAll();
            newName.forceActiveFocus();
        }
        onAccepted: QTox.renameProfile(newName.text)

        TextField {
            id: newName

            width: parent.width
            placeholderText: qsTr("Profile name")
            onAccepted: renameDialog.accept()
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Cancel")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("Rename")
                flat: true
                enabled: newName.text.trim().length > 0
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    Dialog {
        id: eraseHistoryDialog

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: qsTr("Confirmation")
        onAccepted: QTox.eraseHistory()

        Label {
            width: parent.width
            text: qsTr("Do you want to permanently delete all chat history?")
            wrapMode: Text.Wrap
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Keep")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("Delete")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    Dialog {
        id: deleteDialog

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: qsTr("Delete profile")
        onAccepted: QTox.deleteProfile()

        Label {
            width: parent.width
            text: qsTr("This deletes your Tox ID, your friends and your chat history from this device. Export the profile first to keep them.")
            wrapMode: Text.Wrap
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Cancel")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("Delete")
                flat: true
                Material.foreground: Material.color(Material.Red)
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

    FileDialog {
        id: picturePicker

        title: qsTr("Choose a picture")
        nameFilters: [qsTr("Pictures (*.png *.jpg *.jpeg *.gif *.bmp *.webp)")]
        onAccepted: QTox.setAvatar(selectedFile)
    }
}
