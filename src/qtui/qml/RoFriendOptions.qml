import QtQuick

Rectangle {
    id: options
    property var optionData: ({})
    objectName: "sourceFriendOptions"
    x: optionData.x || 0; y: optionData.y || 0; width: optionData.width || 0; height: optionData.height || 0
    z: optionData.layer || 0; visible: optionData.visible || false; color: "#e8e8e8"; clip: true
    Rectangle { width: parent.width; height: 17; color: "#828c96" }
    Text { x: 5; y: 3; width: parent.width-5; height: 14; clip: true; text: options.optionData.caption || ""; color: "white"; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12 }
    Text { objectName: "sourceFriendOptionsCaption"; x: 4; y: 2; width: parent.width-4; height: 15; clip: true; text: options.optionData.caption || ""; color: "black"; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12 }
    Rectangle { x: 1; y: 17; width: parent.width-2; height: parent.height-17; color: "#f2f2f2" }
    Repeater {
        model: options.optionData.checks || []
        delegate: Item {
            required property var modelData; required property int index
            objectName: "sourceFriendCheck"+index
            x: modelData.x; y: modelData.y; width: modelData.width; height: modelData.height; clip: true
            Image { y: Math.trunc((12-(parent.modelData.imageHeight || 0))/2); width: parent.modelData.imageWidth || 0; height: parent.modelData.imageHeight || 0; source: parent.modelData.artwork || ""; smooth: false }
            Rectangle { width: 10; height: 10; color: "white"; visible: !parent.modelData.artwork; Text { text: parent.parent.modelData.checked?"x":""; color: "black"; font.pixelSize: 12; font.family: "Segoe UI" } }
            Text { x: 11; width: parent.width-11; height: 12; clip: true; text: parent.modelData.label; color: "black"; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12 }
        }
    }
    Rectangle {
        readonly property var button: options.optionData.close || ({})
        objectName: "sourceFriendOptionsClose"
        x: button.x || 0; y: button.y || 0; width: button.width || 0; height: button.height || 0
        color: button.artwork?"transparent":button.state===2?"#b4b4b4":"#d2d2d2"
        Image { width: parent.button.imageWidth || 0; height: parent.button.imageHeight || 0; source: parent.button.artwork || ""; smooth: false }
        Text { visible: !parent.button.artwork; text: "x"; color: "black"; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12 }
    }
}
