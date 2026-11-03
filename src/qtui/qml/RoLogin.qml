import QtQuick 2.15

RoFrame {
    id: panel
    objectName: "loginPanel"
    property var info: ({})
    property var buttons: []
    property string accountId: ""
    property string passwordMask: ""
    property bool passwordFocused: false
    property bool rememberAccount: false
    property bool caretVisible: true
    readonly property var account: info.account || ({})
    readonly property var password: info.password || ({})
    readonly property var remember: info.remember || ({})
    RoTheme { id: theme }

    Text {
        x: 12; y: 2
        text: panel.info.title || "OpenMidgard"
        color: theme.titleText
        font.family: theme.fontFamily; font.pixelSize: theme.titleSize
        font.weight: Font.DemiBold
    }
    Text {
        x: 20; y: 35; width: parent.width - 40
        visible: !panel.info.compact
        text: "Welcome back"
        color: theme.ink
        font.family: theme.fontFamily; font.pixelSize: 23; font.weight: Font.DemiBold
    }
    Text {
        x: 20; y: 67; width: parent.width - 40
        visible: !panel.info.compact
        text: "Sign in to continue your journey."
        color: theme.muted
        font.family: theme.fontFamily; font.pixelSize: 12
        elide: Text.ElideRight
    }

    Repeater {
        model: [
            { name: "loginAccount", rect: panel.account, label: "Account ID", value: panel.accountId, focused: !panel.passwordFocused },
            { name: "loginPassword", rect: panel.password, label: "Password", value: panel.passwordMask.replace(/\*/g, "\u2022"), focused: panel.passwordFocused }
        ]
        delegate: Item {
            required property var modelData
            x: modelData.rect.x || 0; y: modelData.rect.y || 0
            width: modelData.rect.width || 0; height: modelData.rect.height || 0
            objectName: modelData.name
            Text {
                y: -18; width: parent.width
                text: modelData.label
                color: theme.muted
                font.family: theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold
            }
            RoField { anchors.fill: parent; editing: modelData.focused; emphasize: true }
            Text {
                anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                text: modelData.value + (modelData.focused && panel.caretVisible ? "|" : "")
                textFormat: Text.PlainText
                color: theme.ink
                font.family: theme.fontFamily; font.pixelSize: 13
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideLeft
            }
        }
    }

    Item {
        objectName: "loginRemember"
        x: panel.remember.x || 0; y: panel.remember.y || 0
        width: panel.remember.width || 0; height: panel.remember.height || 0
        RoField {
            width: 16; height: 16; anchors.verticalCenter: parent.verticalCenter
            editing: panel.rememberAccount
            color: panel.rememberAccount ? theme.focus : theme.fieldFill
            Text {
                anchors.centerIn: parent
                text: panel.rememberAccount ? "\u2713" : ""
                color: theme.titleText
                font.family: theme.fontFamily; font.pixelSize: 12; font.bold: true
            }
        }
        Text {
            x: 24; width: parent.width - 24; height: parent.height
            text: panel.info.saveLabel || "Remember account ID"
            color: theme.muted
            font.family: theme.fontFamily; font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
    Rectangle {
        x: panel.account.x || 0; y: panel.info.compact ? 194 : 296
        width: panel.account.width || 0; height: 1
        color: theme.divider
    }
    Repeater {
        model: panel.buttons
        delegate: RoTextButton {
            id: action
            required property var modelData
            objectName: "loginButton" + modelData.id
            x: modelData.x - panel.x; y: modelData.y - panel.y
            width: modelData.width; height: modelData.height
            label: modelData.label || ""
            labelSize: modelData.id === 120 ? 13 : 11
            labelBold: modelData.id === 120
            primary: modelData.id === 120
            hovered: hoverArea.containsMouse
            MouseArea { id: hoverArea; anchors.fill: parent; acceptedButtons: Qt.NoButton; hoverEnabled: true }
        }
    }
}
