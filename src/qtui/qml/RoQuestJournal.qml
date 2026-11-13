import QtQuick

Rectangle {
    id: window
    objectName: "questJournalWindow"
    required property var journal
    visible: journal.title !== undefined
    x: journal.x || 0
    y: journal.y || 0
    width: journal.width || 0
    height: journal.height || 0
    z: journal.layer || 0
    color: "white"
    clip: true

    Rectangle {
        width: parent.width
        height: 18
        color: "#dee6f1"
        Text { x: 8; anchors.verticalCenter: parent.verticalCenter; text: "Quest Journal"; font.pixelSize: 12 }
        Text {
            x: parent.width - 18
            width: 18
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignHCenter
            text: "×"
            font.pixelSize: 12
        }
    }

    Rectangle {
        y: 18
        width: 205
        height: parent.height - y
        color: "#f3f6fa"

        Column {
            x: 4
            y: 16
            width: parent.width - 8
            Repeater {
                model: window.journal.rows || []
                delegate: Rectangle {
                    required property var modelData
                    width: parent.width
                    height: 22
                    color: modelData.selected ? "#d7e6f5" : "transparent"
                    Text {
                        x: 5
                        width: parent.width - 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.title + (modelData.active ? "" : " (Inactive)")
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        font.pixelSize: 12
                    }
                }
            }
        }
        Text { x: 8; y: 255; text: "Previous"; visible: !!window.journal.previous; font.pixelSize: 12 }
        Text { x: 110; y: 255; text: "Next"; visible: !!window.journal.next; font.pixelSize: 12 }
    }

    Text {
        x: 219; y: 24; width: parent.width - x - 8
        text: window.journal.title || ""
        textFormat: Text.PlainText
        elide: Text.ElideRight
        font.pixelSize: 14
        font.bold: true
    }
    Text {
        x: 219; y: 47; width: parent.width - x - 8
        text: window.journal.expiry || ""
        textFormat: Text.PlainText
        font.pixelSize: 12
    }
    Text {
        x: 219; y: 72; width: parent.width - x - 10; height: 113
        text: window.journal.description || ""
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        elide: Text.ElideRight
        font.pixelSize: 12
    }
    Column {
        x: 219; y: 190; width: parent.width - x - 8
        Repeater {
            model: window.journal.objectives || []
            delegate: Text {
                required property string modelData
                width: parent.width
                height: 20
                text: modelData
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font.pixelSize: 12
            }
        }
    }
    Rectangle {
        x: 219; y: 268; width: 100; height: 26
        color: "#dee6f1"
        visible: !!window.journal.action
        Text { anchors.centerIn: parent; text: window.journal.action || ""; font.pixelSize: 12 }
    }
}
