import QtQuick 2.15

Rectangle {
    id: field

    RoTheme { id: theme }

    property bool editing: false
    property bool emphasize: false
    property real chromeOpacity: 1.0

    function fade(baseColor) {
        return Qt.rgba(baseColor.r, baseColor.g, baseColor.b, baseColor.a * field.chromeOpacity)
    }

    radius: theme.buttonRadius
    color: emphasize
        ? (editing ? "#ffffff" : theme.bodyTop)
        : fade(theme.fieldFill)
    border.width: emphasize && editing ? 2 : 1
    border.color: emphasize
        ? (editing ? theme.focus : theme.fieldBorder)
        : fade(editing ? theme.focus : theme.fieldBorder)
    antialiasing: true
}
