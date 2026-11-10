import QtQuick

Rectangle {
    id: helpBalloon
    property var helpData: ({})
    objectName: "sourceHelpBalloon"
    x: helpData.x || 0
    y: helpData.y || 0
    width: helpData.width || 0
    height: helpData.height || 0
    z: helpData.layer || 0
    visible: helpData.visible || false
    color: "#88000000"
    clip: true

    Repeater {
        model: helpBalloon.helpData.lines || []
        delegate: Text {
            required property string modelData
            required property int index
            x: 7
            y: 5 + index*((helpBalloon.helpData.lineHeight || 12)+4)
            width: parent.width-8
            height: helpBalloon.helpData.lineHeight || 12
            clip: true
            text: modelData
            color: "black"
            textFormat: Text.PlainText
            font.family: "Segoe UI"
            font.pixelSize: 12
        }
    }
    Repeater {
        model: helpBalloon.helpData.lines || []
        delegate: Text {
            required property string modelData
            required property int index
            x: 6
            y: 4 + index*((helpBalloon.helpData.lineHeight || 12)+4)
            width: parent.width-8
            height: helpBalloon.helpData.lineHeight || 12
            clip: true
            text: modelData
            color: helpBalloon.helpData.fontColor || "white"
            textFormat: Text.PlainText
            font.family: "Segoe UI"
            font.pixelSize: 12
        }
    }
}
