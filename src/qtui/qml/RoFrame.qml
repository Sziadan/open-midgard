import QtQuick 2.15

Item {
    id: frame

    RoTheme { id: theme }

    property bool showTitle: true
    property bool showShadow: true
    property bool showBody: true
    property real chromeOpacity: 1.0

    function fade(baseColor, alphaScale) {
        var scale = alphaScale === undefined ? frame.chromeOpacity : alphaScale
        return Qt.rgba(baseColor.r, baseColor.g, baseColor.b, baseColor.a * scale)
    }

    readonly property int frameRadius: Math.min(theme.radius, Math.max(0, height) / 2)

    Rectangle {
        z: -2
        x: 1
        y: 5
        width: frame.width
        height: frame.height
        radius: frame.frameRadius
        color: theme.shadow
        visible: frame.showShadow && frame.chromeOpacity >= 0.97 && frame.width > 0 && frame.height > 0
    }

    Rectangle {
        id: card
        z: -1
        anchors.fill: parent
        radius: frame.frameRadius
        visible: frame.showBody
        border.width: 1
        border.color: frame.fade(theme.bodyBorder)
        antialiasing: true

        gradient: Gradient {
            GradientStop { position: 0.0; color: frame.fade(theme.bodyTop) }
            GradientStop { position: 1.0; color: frame.fade(theme.bodyBottom) }
        }
    }

    Item {
        visible: frame.showTitle
        x: 1
        y: 1
        width: Math.max(0, frame.width - 2)
        height: Math.max(0, theme.titleHeight - 1)
        clip: true

        Rectangle {
            width: parent.width
            height: parent.height + theme.radius
            radius: frame.frameRadius
            antialiasing: true

            gradient: Gradient {
                GradientStop { position: 0.0; color: frame.fade(theme.titleTop) }
                GradientStop { position: 1.0; color: frame.fade(theme.titleBottom) }
            }
        }
    }

    Rectangle {
        visible: !frame.showBody
        anchors.fill: parent
        radius: frame.frameRadius
        color: "transparent"
        border.width: 1
        border.color: frame.fade(theme.bodyBorder)
        antialiasing: true
    }

    Rectangle {
        visible: frame.showTitle
        x: 1
        y: theme.titleHeight
        z: 1
        width: Math.max(0, frame.width - 2)
        height: 1
        color: frame.fade(theme.gold)
    }
}
