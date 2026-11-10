// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2026 The TokTok team.

import QtQuick
import QtQuick.Controls

// For lint_test: the lint must warn of the lines that end in "// HTML", and of no others.
Item {
    id: root

    property string name
    property bool on
    property int count

    // Our own words.
    Label { text: "a" }
    Label { text: qsTr("a") }
    Label { text: root.on ? qsTr("a") : qsTr("b") }
    Label { text: root.on ? "a" : root.count > 1 ? qsTr("b ? c : d") : 'e' }
    Label { text: root.name === "x ? y" && root.on ? qsTr("a") : qsTr("b", "which") }
    Label { text: qsTr("%n thing(s)", "", root.count) }
    Label { text: root.on ? qsTr("%n thing(s)", "things", root.count + (root.on ? 1 : 0)) : "" }
    Button { text: [1, 2].includes(root.count) ? qsTr("a") : qsTr("b") }
    Dialog { title: qsTr("a") }
    TextField { placeholderText: qsTr("a") }
    Button { ToolTip.text: qsTr("a") }

    // Others' words in a Text that says how it shows them.
    Label { text: root.name; textFormat: Text.PlainText }
    Label { text: root.name; textFormat: Text.StyledText }
    Label { id: plain; textFormat: Text.PlainText }
    Switch {
        id: plainSwitch

        text: root.name
        contentItem: Label { text: plainSwitch.text; textFormat: Text.PlainText }
    }
    ToolTip {
        id: plainTip

        contentItem: Label { text: plainTip.text; textFormat: Text.PlainText }
    }

    // Others' words in one that doesn't.
    Text { text: root.name } // HTML
    Label { text: root.name } // HTML
    Label { text: root.on ? root.name : qsTr("b") } // HTML
    Label { text: root.on ? qsTr("a") : root.count > 1 ? root.name : "e" } // HTML
    Label { text: root.on ? "a" : root.name ?? "b" } // HTML
    Label { text: root.on ? `a${root.name}` : "b" } // HTML
    Label { text: (root.on ? "a" : "b") + root.name } // HTML
    Label { text: "a" + (root.on ? "a" : root.name) } // HTML
    Label { text: root.name || qsTr("a") } // HTML
    Label { text: qsTr("a") + root.name } // HTML
    Label { text: qsTr("a %1").arg(root.name) } // HTML
    Label { text: qsTr(root.name) } // HTML
    Label { text: qsTr("a", root.name) } // HTML
    Label { text: { return root.name; } } // HTML
    Label { id: label }

    // In a control's own Text.
    Button { text: root.name } // HTML
    CheckBox { text: root.name } // HTML
    ItemDelegate { text: root.name } // HTML
    Action { text: root.name } // HTML
    Menu { title: root.name } // HTML
    GroupBox { title: root.name } // HTML
    Dialog { id: dialog; title: root.name } // HTML
    TextField { placeholderText: root.name } // HTML
    TextArea { placeholderText: root.name } // HTML
    ToolTip { id: tip } // HTML
    Button { id: button; ToolTip.text: root.name } // HTML

    function write(text: string) {
        plain.text = text;
        plainTip.show(text);
        label.text = text; // HTML
        dialog.title = text; // HTML
        tip.show(text);
        button.ToolTip.text = text; // HTML
        button.ToolTip.show(text); // HTML
        ToolTip.toolTip.show(text); // HTML
    }
}
