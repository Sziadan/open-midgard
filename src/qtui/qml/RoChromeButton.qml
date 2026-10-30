import QtQuick 2.15

Rectangle {
    id: button

    RoTheme { id: theme }

    property bool pressed: false
    property bool hovered: false
    property string label: ""
    property int labelSize: 8
    property bool labelBold: true
    property string labelFont: theme.fontFamily

    radius: Math.min(theme.buttonRadius, Math.max(1, Math.floor(height / 2)))
    clip: true
    color: !enabled ? theme.rowAlt : (pressed ? theme.buttonPressed : (hovered ? theme.buttonHover : theme.button))
    border.width: 1
    border.color: theme.buttonBorder
    antialiasing: true

    Text {
        anchors.fill: parent
        anchors.margins: 1
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
