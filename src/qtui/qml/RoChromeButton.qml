import QtQuick 2.15

Rectangle {
    id: button

    RoTheme { id: theme }

    property bool pressed: false
    property bool hovered: false
    property string label: ""
    property string icon: ""
    property int labelSize: 8
    property bool labelBold: true
    property string labelFont: theme.fontFamily

    radius: Math.min(theme.buttonRadius, Math.max(1, Math.floor(height / 2)))
    clip: true
    color: !enabled ? theme.rowAlt : (pressed ? theme.buttonPressed : (hovered ? theme.buttonHover : theme.button))
    border.width: 1
    border.color: (button.hovered || button.pressed) ? theme.gold : theme.buttonBorder
    antialiasing: true

    RoBasicMenuIcon {
        visible: button.icon !== ""
        anchors.centerIn: parent
        anchors.verticalCenterOffset: button.pressed ? 1 : 0
        width: Math.max(12, parent.width - 6)
        height: Math.max(12, parent.height - 4)
        icon: button.icon
        ink: theme.ink
        paper: button.color
    }

    Text {
        anchors.fill: parent
        anchors.margins: 1
        visible: button.icon === ""
        text: button.label
        color: theme.ink
        font.family: button.labelFont
        font.pixelSize: Math.max(7, Math.min(button.labelSize, Math.max(7, button.height - 3)))
        font.bold: button.labelBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
