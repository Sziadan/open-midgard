pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: window
    required property var list
    x: list.x
    y: list.y
    z: list.layer
    width: list.width
    height: list.height
    color: "white"
    clip: true

    Rectangle {
        width: parent.width
        height: 17
        color: "#dee6f1"
    }

    Text {
        x: 16
        y: 2
        text: window.list.title
        font.pixelSize: 12
        textFormat: Text.PlainText
    }

    Image {
        x: window.width - 14
        y: 3
        width: 11
        height: 11
        source: window.list.close
        smooth: false
    }

    Text {
        x: window.width - 14
        y: 1
        visible: !window.list.close
        text: "×"
        font.pixelSize: 13
    }

    Repeater {
        model: window.list.rows
        delegate: Rectangle {
            id: row
            required property var modelData
            x: 2
            y: modelData.y
            width: window.width - 4
            height: 32
            color: modelData.selected ? "#e1ebfc" : "white"
            Image {
                x: 42
                y: 4
                width: 24
                height: 24
                source: row.modelData.icon
                smooth: false
            }
            Text {
                x: 110
                y: 4
                width: 164
                text: row.modelData.name
                elide: Text.ElideRight
                font.pixelSize: 12
                textFormat: Text.PlainText
            }
            Text {
                x: 110
                y: 17
                text: "Lv " + row.modelData.level
                font.pixelSize: 12
            }
            Text {
                x: 173
                y: 17
                text: "SP " + row.modelData.spCost
                font.pixelSize: 12
            }
            Rectangle {
                visible: row.modelData.upgradeVisible
                x: row.modelData.upgradeX - row.x
                y: row.modelData.upgradeY
                width: row.modelData.upgradeSize
                height: width
                color: row.modelData.upgradeArt ? "transparent" : "#e8e8e8"
                Image {
                    anchors.fill: parent
                    source: row.modelData.upgradeArt
                    smooth: false
                }
                Text {
                    anchors.centerIn: parent
                    visible: !row.modelData.upgradeArt
                    text: "+"
                    font.pixelSize: 12
                }
            }
        }
    }

    Text {
        x: 16
        y: window.height - 20
        text: window.list.footer
        font.pixelSize: 12
    }
}
