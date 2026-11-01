import QtQuick 2.15

RoTextButton {
    id: tab
    RoTheme { id: theme }
    property bool selected: false
    labelSize: 11
    labelBold: selected
    color: selected ? theme.rowSelected : (hovered ? theme.buttonHover : theme.button)
    border.color: selected ? theme.focus : theme.buttonBorder
    Rectangle {
        visible: tab.selected
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.max(0, parent.width - 16)
        height: 2
        radius: 1
        color: theme.focus
    }
}
