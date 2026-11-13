import QtQuick

Rectangle {
    id: window
    objectName: "memorialDungeonWindow"
    required property var dungeon
    visible: dungeon.title !== undefined
    x: dungeon.x || 0
    y: dungeon.y || 0
    z: dungeon.layer || 0
    width: dungeon.width || 0
    height: dungeon.height || 0
    color: "white"
    clip: true

    Rectangle {
        width: parent.width
        height: 18
        color: "#dee6f1"
        Text {
            x: 8
            width: parent.width - 30
            anchors.verticalCenter: parent.verticalCenter
            text: window.dungeon.title || ""
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: 12
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            text: "×"
            font.pixelSize: 12
        }
    }

    Column {
        x: 8
        y: 25
        width: parent.width - 16
        spacing: 5
        Text {
            width: parent.width
            text: window.dungeon.status || ""
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: 12
        }
        Repeater {
            model: window.dungeon.details || []
            delegate: Text {
                required property string modelData
                width: parent.width
                text: modelData
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                font.pixelSize: 12
            }
        }
    }
}
