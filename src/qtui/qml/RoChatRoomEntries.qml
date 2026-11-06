import QtQuick

Item {
    id: entries
    property var titleItems: []
    property var passwordData: ({})
    Repeater {
        model: entries.titleItems
        delegate: Item {
            parent: entries.parent
            required property var modelData
            required property int index
            objectName: "sourceChatRoomWorldTitle"+index
            x: modelData.x; y: modelData.y; width: modelData.width; height: modelData.height
            z: modelData.layer; visible: modelData.visible; clip: true
            Rectangle { width: parent.width; height: 28; color: "#f5f5eb"; visible: !parent.modelData.artwork }
            Image { anchors.fill: parent; source: parent.modelData.artwork; smooth: false }
            Text {
                objectName: "sourceChatRoomWorldTitleText"+parent.index
                x: 31; y: 8; width: parent.width-34; height: 15; clip: true
                text: parent.modelData.text; color: "black"; font.pixelSize: 12; font.family: "Segoe UI"; textFormat: Text.PlainText
            }
        }
    }
    Rectangle {
        parent: entries.parent
        id: password
        objectName: "sourceChatRoomPassword"
        x: entries.passwordData.x || 0; y: entries.passwordData.y || 0
        width: entries.passwordData.width || 0; height: entries.passwordData.height || 0
        z: entries.passwordData.layer || 0; visible: entries.passwordData.visible || false
        color: "#e8e8e8"; clip: true
        Text {
            objectName: "sourceChatRoomPasswordPrompt"
            x: 15; y: 6; width: parent.width-30; height: 15; clip: true
            text: entries.passwordData.prompt || ""; color: "black"; font.pixelSize: 12; font.family: "Segoe UI"; textFormat: Text.PlainText
        }
        Rectangle {
            x: 15; y: 22; width: 90; height: 16; color: "white"; clip: true
            Text {
                objectName: "sourceChatRoomPasswordMask"
                text: entries.passwordData.masked || ""; color: "black"; font.pixelSize: 12; font.family: "Segoe UI"; textFormat: Text.PlainText
            }
        }
        Rectangle {
            objectName: "sourceChatRoomPasswordOk"
            x: entries.passwordData.okX || 0; y: entries.passwordData.okY || 0
            width: entries.passwordData.okWidth || 0; height: entries.passwordData.okHeight || 0
            color: entries.passwordData.okArtwork?"transparent":entries.passwordData.pressedOk?"#b4b4b4":"#d2d2d2"
            Image { width: entries.passwordData.okImageWidth || 0; height: entries.passwordData.okImageHeight || 0; source: entries.passwordData.okArtwork || ""; smooth: false }
            Text { visible: !entries.passwordData.okArtwork; x: 4; y: 2; text: "OK"; color: "black"; font.pixelSize: 12; font.family: "Segoe UI" }
        }
    }
}
