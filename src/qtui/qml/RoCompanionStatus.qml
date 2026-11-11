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
            textFormat: Text.PlainText
            font.pixelSize: 12
            elide: Text.ElideRight
            horizontalAlignment: modelData.rightAligned ? Text.AlignRight : Text.AlignLeft
        }
    }

    Repeater {
        model: window.status.buttons
        delegate: Rectangle {
            required property var modelData
            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            color: modelData.artwork ? "transparent" : "#e8e8e8"
            Image { anchors.fill: parent; source: parent.modelData.artwork; smooth: false }
            Text {
                anchors.centerIn: parent
                visible: !parent.modelData.artwork
                text: parent.modelData.label
                font.pixelSize: 12
            }
        }
    }

    Rectangle {
        readonly property var edit: window.status.nameEdit
        visible: edit.visible || false
        x: edit.x || 0
        y: edit.y || 0
        width: edit.width || 0
        height: edit.height || 0
        color: edit.focused ? "#ffffc8" : "#e8e8e8"
        border.color: "#808080"
        clip: true
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            text: parent.edit.text || ""
            textFormat: Text.PlainText
            font.pixelSize: 12
        }
    }
}
