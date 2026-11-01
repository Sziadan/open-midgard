import QtQuick 2.15

Item {
    id: header
    RoTheme { id: theme }
    property real chromeOpacity: 1.0
    height: theme.titleHeight
    clip: true
    Rectangle {
        width: parent.width
        height: parent.height + theme.radius
        radius: theme.radius
        color: Qt.rgba(theme.titleBottom.r, theme.titleBottom.g, theme.titleBottom.b, header.chromeOpacity)
        antialiasing: true
    }
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Qt.rgba(0, 0, 0, 0.12 * header.chromeOpacity)
    }
}
