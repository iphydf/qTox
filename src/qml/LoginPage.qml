// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Page {
    id: page

    title: "qTox"

    header: ToolBar {
        Label {
            anchors.centerIn: parent
            text: page.title
            font.bold: true
        }
    }

    FileDialog {
        id: importPicker

        title: qsTr("Import profile")
        onAccepted: {
            const name = QTox.importedProfileName(selectedFile);
            if (name === "")
                return;
            if (QTox.profiles.includes(name)) {
                replaceDialog.file = selectedFile;
                replaceDialog.name = name;
                replaceDialog.open();
            } else {
                QTox.importProfile(selectedFile);
            }
        }
    }

    Dialog {
        id: replaceDialog

        property url file
        property string name

        anchors.centerIn: parent
        width: Math.min(page.width - 32, 400)
        modal: true
        title: qsTr("Profile already exists")
        onAccepted: QTox.importProfile(file)

        Label {
            width: parent.width
            text: qsTr("A profile named \"%1\" already exists. Do you want to erase it?").arg(replaceDialog.name)
            wrapMode: Text.Wrap
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Cancel")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("Erase")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
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
                visible: profiles.count > 0
                text: qsTr("Log in")
                font.bold: true
            }

            ComboBox {
                id: profiles

                visible: count > 0
                Layout.fillWidth: true
                model: QTox.profiles
            }

            TextField {
                id: password

                visible: profiles.count > 0 && QTox.isEncrypted(profiles.currentText)
                Layout.fillWidth: true
                placeholderText: qsTr("Password")
                echoMode: TextInput.Password
                onAccepted: login.clicked()
            }

            // Password protected profiles can't log in by themselves.
            CheckBox {
                id: autoLogin

                visible: profiles.count > 0 && !password.visible
                text: qsTr("Log in automatically")
                checked: true
            }

            Button {
                id: login

                visible: profiles.count > 0
                Layout.fillWidth: true
                text: qsTr("Log in")
                highlighted: true
                onClicked: QTox.loadProfile(profiles.currentText, password.text, autoLogin.checked)
            }

            MenuSeparator {
                visible: profiles.count > 0
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("New profile")
                font.bold: true
            }

            TextField {
                id: newName

                Layout.fillWidth: true
                placeholderText: qsTr("Profile name")
            }

            TextField {
                id: newPassword

                Layout.fillWidth: true
                placeholderText: qsTr("Password (optional)")
                echoMode: TextInput.Password
            }

            TextField {
                id: repeatedPassword

                visible: newPassword.text.length > 0
                Layout.fillWidth: true
                placeholderText: qsTr("Repeat password")
                echoMode: TextInput.Password
            }

            Label {
                id: newProfileProblem

                // As the Widgets UI checks them.
                readonly property string problem: QTox.profiles.includes(newName.text.trim())
                    ? qsTr("A profile with this name already exists.")
                    : newPassword.text.length > 0 && newPassword.text.length < 6
                    ? qsTr("The password must be at least 6 characters long.")
                    : newPassword.text !== repeatedPassword.text
                    ? qsTr("The passwords you've entered are different.") : ""

                visible: text.length > 0 && (newName.text.length > 0 || newPassword.text.length > 0)
                Layout.fillWidth: true
                text: problem
                wrapMode: Text.Wrap
                opacity: 0.7
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("Create profile")
                enabled: newName.text.trim().length > 0 && newProfileProblem.problem === ""
                // As "Log in automatically", for the profiles that can.
                onClicked: QTox.createProfile(newName.text.trim(), newPassword.text, true)
            }

            MenuSeparator {
                Layout.fillWidth: true
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("Import profile")
                flat: true
                onClicked: importPicker.open()
            }
        }
    }
}
