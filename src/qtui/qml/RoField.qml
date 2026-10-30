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
        ? (editing ? "#ffffff" : "#f6f1e7")
        : fade(theme.fieldFill)
    border.width: emphasize && editing ? 2 : 1
    border.color: emphasize
        ? (editing ? "#a6843a" : "#6f6252")
        : fade(editing ? theme.gold : theme.fieldBorder)
    antialiasing: true
}
