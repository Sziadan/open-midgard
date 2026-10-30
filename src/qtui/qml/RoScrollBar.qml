import QtQuick 2.15

Rectangle {
    id: bar

    RoTheme { id: theme }

    property real thumbX: 0
    property real thumbY: 0
    property real thumbWidth: 0
    property real thumbHeight: 0

    color: theme.scrollTrack
    radius: 3
    border.width: 0
    antialiasing: true

    Rectangle {
        x: bar.thumbX + 2
        y: bar.thumbY + 1
        width: Math.max(4, bar.thumbWidth - 4)
        height: Math.max(6, bar.thumbHeight - 2)
        radius: Math.min(width, height) / 2
        color: theme.scrollThumb
        antialiasing: true
    }
}
