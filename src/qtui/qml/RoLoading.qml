import QtQuick 2.15

RoFrame {
    id: panel
    objectName: "loadingPanel"
    property string title: "Loading"
    property string message: ""
    property real progress: 0
    readonly property real boundedProgress: isFinite(progress) ? Math.max(0, Math.min(1, progress)) : 0
    readonly property bool indeterminate: boundedProgress <= 0
    showTitle: false
    height: content.implicitHeight + 32
    RoTheme { id: theme }

    Column {
        id: content
        x: 16; y: 16; width: Math.max(0, parent.width - 32)
        spacing: 10
        Item {
            width: parent.width; height: 24
            Text {
                id: percent
                anchors.right: parent.right
                text: panel.indeterminate ? "" : Math.round(panel.boundedProgress * 100) + "%"
                color: theme.focus
                font.family: theme.fontFamily; font.pixelSize: 13; font.weight: Font.DemiBold
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                width: Math.max(0, parent.width - percent.implicitWidth - 12)
                text: panel.title
                color: theme.ink
                font.family: theme.fontFamily; font.pixelSize: 18; font.weight: Font.DemiBold
                elide: Text.ElideRight
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Text {
            objectName: "loadingMessage"
            width: parent.width
            visible: text.length > 0
            text: panel.message
            textFormat: Text.PlainText
            color: theme.muted
            font.family: theme.fontFamily; font.pixelSize: 12
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
        Rectangle {
            id: track
            objectName: "loadingTrack"
            width: parent.width; height: 14
            radius: theme.buttonRadius
            color: theme.gaugeTrack
            border.width: 1; border.color: theme.fieldBorder
            clip: true
            Rectangle {
                objectName: "loadingFill"
                property real travel: 0
                x: 2 + (panel.indeterminate ? travel * Math.max(0, track.width - width - 4) : 0)
                y: 2
                width: Math.max(0, track.width - 4) * (panel.indeterminate ? 0.24 : panel.boundedProgress)
                height: track.height - 4
                radius: 3
                color: theme.focus
                SequentialAnimation on travel {
                    running: panel.visible && panel.indeterminate
                    loops: Animation.Infinite
                    NumberAnimation { from: 0; to: 1; duration: 1000; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 1; to: 0; duration: 1000; easing.type: Easing.InOutSine }
                }
            }
        }
    }
}
