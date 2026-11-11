import QtQuick

Rectangle {
    id: window
    required property var status
    x: status.x
    y: status.y
    z: status.layer
    width: status.width
    height: status.height
    color: "white"
    clip: true

    Rectangle {
        width: parent.width
        height: 18
        color: "#dee6f1"
    }

    Repeater {
        model: window.status.gauges
        delegate: Rectangle {
            required property var modelData
            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            color: "#e4e5e8"
            Rectangle {
                width: parent.modelData.filledWidth
                height: parent.height
                color: parent.modelData.color
            }
        }
    }

    Repeater {
        model: window.status.texts
        delegate: Text {
            required property var modelData
            x: modelData.x
            y: modelData.y
            width: modelData.width
            text: modelData.text
            font.pixelSize: 12
            elide: Text.ElideRight
            horizontalAlignment: modelData.rightAligned ? Text.AlignRight : Text.AlignLeft
        }
    }

    Text {
        x: parent.width - 14
        y: 1
        width: 11
        height: 14
        text: "×"
        font.pixelSize: 13
        horizontalAlignment: Text.AlignHCenter
    }
}
