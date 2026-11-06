import QtQuick

Rectangle {
    id: room
    property var roomData: ({})
    objectName: "sourceChatRoomWindow"
    x: roomData.x || 0; y: roomData.y || 0
    width: roomData.width || 0; height: roomData.height || 0
    visible: roomData.visible || false
    z: roomData.layer || 0
    color: "#e6e6e6"; clip: true
    Rectangle { width: parent.width; height: 17; color: "#5a646e" }
    Text { objectName: "sourceChatRoomTitle"; x: 9; y: 2; width: (room.roomData.edit || {}).width || 0; height: 15; clip: true; text: room.roomData.title || ""; textFormat: Text.PlainText; color: "white"; font.pixelSize: 12 }
    Rectangle {
        readonly property var bounds: room.roomData.history || ({})
        objectName: "sourceChatRoomHistory"
        x: bounds.x || 0; y: bounds.y || 0; width: bounds.width || 0; height: bounds.height || 0
        color: "white"; clip: true
        Repeater {
            model: room.roomData.rows || []
            delegate: Text {
                required property var modelData
                required property int index
                objectName: "sourceChatRoomRow"+index
                x: 2; y: 3+14*index; width: Math.max(0,parent.width-2-(room.roomData.scrollVisible?13:0)); height: 14; clip: true
                text: modelData.text; color: modelData.color; font.pixelSize: 12; textFormat: Text.PlainText
            }
        }
    }
    Rectangle {
        readonly property var bounds: room.roomData.members || ({})
        objectName: "sourceChatRoomMembers"
        x: bounds.x || 0; y: bounds.y || 0; width: bounds.width || 0; height: bounds.height || 0
        color: "white"; clip: true
        Repeater {
            model: room.roomData.memberRows || []
            delegate: Rectangle {
                required property var modelData
                required property int index
                x: 0; y: 14*index; width: parent.width; height: 14
                color: "transparent"
                Rectangle { y: 1; width: parent.width; height: 14; color: parent.modelData.selected?"#32c8c8":"transparent" }
                Text { x: 2; y: 3; width: Math.max(0,parent.width-2-(room.roomData.memberScrollVisible?13:0)); height: 14; clip: true; text: parent.modelData.name; textFormat: Text.PlainText; color: parent.modelData.color; font.pixelSize: 12 }
            }
        }
    }
    Rectangle {
        readonly property var bounds: room.roomData.edit || ({})
        objectName: "sourceChatRoomEdit"
        x: bounds.x || 0; y: bounds.y || 0; width: bounds.width || 0; height: bounds.height || 0
        color: "#fafafa"; clip: true
        Text { text: room.roomData.input || ""; textFormat: Text.PlainText; color: "#222222"; font.pixelSize: 12 }
    }
    Text { x: (room.roomData.close || {}).x || 0; y: 3; text: "x"; color: "white"; font.pixelSize: 12 }
    Text { x: (room.roomData.resizer || {}).x || 0; y: (room.roomData.resizer || {}).y || 0; text: "/"; color: "#555555"; font.pixelSize: 12 }
    RoSourceScrollBar {
        trackBounds: room.roomData.scrollTrack || ({}); thumbBounds: room.roomData.scrollThumb || ({})
        upBounds: room.roomData.scrollUp || ({}); downBounds: room.roomData.scrollDown || ({})
        shown: room.roomData.scrollVisible || false; arrowPrefix: "roomScrollArrow"
    }
    RoSourceScrollBar {
        trackBounds: room.roomData.memberTrack || ({}); thumbBounds: room.roomData.memberThumb || ({})
        upBounds: room.roomData.memberUp || ({}); downBounds: room.roomData.memberDown || ({})
        shown: room.roomData.memberScrollVisible || false; arrowPrefix: "roomMemberArrow"
    }
}
