import QtQuick

Item {
    id: whispers
    property var windows: []
    Repeater {
        model: whispers.windows
        delegate: Rectangle {
            id: window
            parent: whispers.parent
            required property var modelData
            required property int index
            objectName: "sourceWhisperWindow"+index
            x: modelData.x; y: modelData.y; width: modelData.width; height: modelData.height
            visible: modelData.visible; z: modelData.layer; color: "#e6e6e6"; clip: true
            Rectangle { width: parent.width; height: 17; color: "#5a646e" }
            Text { x: 9; y: 2; width: parent.width-30; height: 15; clip: true; text: "With  "+window.modelData.title; color: "white"; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12 }
            Rectangle {
                readonly property var bounds: window.modelData.history
                objectName: window.objectName+"History"
                x: bounds.x; y: bounds.y; width: bounds.width; height: bounds.height
                color: "white"; clip: true
                Repeater {
                    model: window.modelData.rows
                    delegate: Text {
                        required property var modelData; required property int index
                        objectName: window.objectName+"Row"+index
                        x: 2; y: 3+index*14; width: parent.width-2-(window.modelData.scrollVisible?13:0); height: 14; clip: true
                        text: modelData.text; color: modelData.color; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12
                    }
                }
            }
            Rectangle {
                readonly property var bounds: window.modelData.edit
                objectName: window.objectName+"Edit"
                x: bounds.x; y: bounds.y; width: bounds.width; height: bounds.height; color: "#fafafa"; clip: true
                Text { text: window.modelData.input; color: "#222222"; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12 }
            }
            Text { x: window.modelData.close.x; y: 3; text: "x"; color: "white"; font.pixelSize: 12 }
            Text { x: window.modelData.resizer.x; y: window.modelData.resizer.y; text: "/"; color: "#555555"; font.pixelSize: 12 }
            RoSourceScrollBar {
                trackBounds: window.modelData.scrollTrack; thumbBounds: window.modelData.scrollThumb
                upBounds: window.modelData.scrollUp; downBounds: window.modelData.scrollDown
                shown: window.modelData.scrollVisible; arrowPrefix: window.objectName+"ScrollArrow"
            }
        }
    }
}
