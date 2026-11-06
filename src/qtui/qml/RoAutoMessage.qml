import QtQuick

Rectangle {
    id: editor
    property var editorData: ({})
    objectName: "sourceAutoMessage"
    x: editorData.x || 0; y: editorData.y || 0; width: editorData.width || 0; height: editorData.height || 0
    visible: editorData.visible || false; z: editorData.layer || 0; color: "#e8e8e8"; clip: true
    Text { objectName: "sourceAutoPrompt"; x: 15; y: 6; width: 265; height: 16; clip: true; text: editor.editorData.prompt || ""; textFormat: Text.PlainText; color: "black"; font.family: "Segoe UI"; font.pixelSize: 12 }
    Item {
        objectName: "sourceAutoEdit"; x: 15; y: 22; width: 203; height: 16; clip: true
        Rectangle { width: Math.min(203, input.implicitWidth); height: 16; color: "#0078d7"; visible: !!editor.editorData.selected && input.text.length > 0 }
        Text { id: input; text: editor.editorData.input || ""; textFormat: Text.PlainText; color: editor.editorData.selected?"white":"black"; font.family: "Segoe UI"; font.pixelSize: 12 }
    }
    Rectangle {
        readonly property var button: editor.editorData.button || ({})
        objectName: "sourceAutoOK"; x: button.x || 0; y: button.y || 0; width: button.width || 0; height: button.height || 0
        color: button.artwork?"transparent":button.pressed?"#b4b4b4":"#d2d2d2"
        Image { width: parent.button.imageWidth || 0; height: parent.button.imageHeight || 0; source: parent.button.artwork || ""; smooth: false }
        Text { visible: !parent.button.artwork; text: "OK"; color: "black"; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12 }
    }
}
