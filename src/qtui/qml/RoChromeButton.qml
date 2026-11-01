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
    readonly property bool isSystemControl: width <= 20 && height <= 20
        && (label === "X" || label === "x" || label === "_" || label === "B")
    readonly property string effectiveIcon: icon !== "" ? icon
        : (isSystemControl ? (label === "_" ? "minimize" : (label === "B" ? "expand" : "close")) : "")

    radius: Math.min(theme.buttonRadius, Math.max(1, Math.floor(height / 2)))
    clip: true
    color: isSystemControl ? (pressed ? "#55ffffff" : (hovered ? "#33ffffff" : "transparent"))
        : (!enabled ? theme.rowAlt : (pressed ? theme.buttonPressed : (hovered ? theme.buttonHover : theme.button)))
    border.width: isSystemControl ? 0 : 1
    border.color: (button.hovered || button.pressed) ? theme.focus : theme.buttonBorder
    antialiasing: true

    RoBasicMenuIcon {
        visible: button.effectiveIcon !== ""
        anchors.centerIn: parent
        anchors.verticalCenterOffset: button.pressed ? 1 : 0
        width: Math.min(20, Math.max(8, parent.width - 6))
        height: Math.min(20, Math.max(8, parent.height - 4))
        icon: button.effectiveIcon
        ink: !button.enabled ? theme.disabledText : (button.isSystemControl ? theme.titleText : theme.ink)
        paper: button.color
    }

    Text {
        anchors.fill: parent
        anchors.margins: 1
        visible: button.effectiveIcon === ""
        text: button.label
        color: button.enabled ? theme.ink : theme.disabledText
        font.family: button.labelFont
        font.pixelSize: Math.max(7, Math.min(button.labelSize, Math.max(7, button.height - 3)))
        font.bold: button.labelBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
