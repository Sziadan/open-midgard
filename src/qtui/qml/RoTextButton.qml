import QtQuick 2.15

Rectangle {
    id: button

    RoTheme { id: theme }

    property bool pressed: false
    property bool hovered: false
    property string label: ""
    property int labelSize: 12
    property bool labelBold: false
    property bool primary: false

    radius: theme.buttonRadius
    color: !enabled ? theme.rowAlt : primary
        ? (pressed ? theme.titleTop : (hovered ? theme.titleBottom : theme.focus))
        : (pressed ? theme.buttonPressed : (hovered ? theme.buttonHover : theme.button))
    border.width: 1
    border.color: (hovered || pressed) ? theme.focus : theme.buttonBorder
    antialiasing: true

    Text {
        anchors.centerIn: parent
        text: button.label
        color: enabled ? (button.primary ? theme.titleText : theme.ink) : theme.disabledText
        font.family: theme.fontFamily
        font.pixelSize: button.labelSize
        font.bold: button.labelBold
    }
}
