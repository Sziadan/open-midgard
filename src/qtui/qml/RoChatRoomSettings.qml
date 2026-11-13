import QtQuick

Item {
    id: forms
    property var settings: []
    property var message: ({})
    property var menu: ({})
    Rectangle {
        // Commerce panes live above uiLayer. Shared source menu18 is a popup
        // above those regular panes, with modal prompts still above it.
        parent: forms.parent.parent
        objectName: "sourceRoomMenu"
        x: (forms.menu.x || 0)*forms.parent.scale; y: (forms.menu.y || 0)*forms.parent.scale
        width: forms.menu.width || 0; height: forms.menu.height || 0
        scale: forms.parent.scale; transformOrigin: Item.TopLeft
        z: 100+(forms.menu.layer || 0); visible: forms.menu.visible || false; color: "#f0f0f0"; clip: true
        Repeater {
            model: forms.menu.rows || []
            delegate: Item {
                required property var modelData; required property int index
                y: index*14; width: parent.width; height: 14
                Rectangle { x: 3; y: 1; width: parent.width-6; height: 12; color: "#32c8c8"; visible: index === forms.menu.hover }
                Text { x: 3; y: 1; width: parent.width-3; height: 14; clip: true; text: modelData.text; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12; color: "black" }
            }
        }
    }
    component SourceButton: Rectangle {
        property var button: ({})
        x: button.x || 0; y: button.y || 0; width: button.width || 0; height: button.height || 0
        color: button.artwork?"transparent":button.pressed?"#b4b4b4":"#d2d2d2"
        Image { width: parent.button.imageWidth || 0; height: parent.button.imageHeight || 0; source: parent.button.artwork || ""; smooth: false }
        Text { visible: !parent.button.artwork; text: parent.button.label || ""; font.pixelSize: 12; font.family: "Segoe UI"; color: "black"; textFormat: Text.PlainText }
    }
    Repeater {
        model: forms.settings
        delegate: Rectangle {
            id: form
            parent: forms.parent
            required property var modelData
            objectName: modelData.change?"sourceRoomChange":"sourceRoomMake"
            x: modelData.x; y: modelData.y; width: modelData.width; height: modelData.height
            z: modelData.layer; visible: modelData.visible; color: "#e8e8e8"; clip: true
            Image { y: 17; source: form.modelData.background; smooth: false }
            Image { source: form.modelData.titlebar; smooth: false }
            Text { x: 9; y: 4; width: 251; height: 15; clip: true; text: form.modelData.caption; color: "white"; font.pixelSize: 12; font.family: "Segoe UI"; textFormat: Text.PlainText }
            Text { x: 8; y: 3; width: 251; height: 15; clip: true; text: form.modelData.caption; color: "black"; font.pixelSize: 12; font.family: "Segoe UI"; textFormat: Text.PlainText }
            Text { visible: !form.modelData.background; x: 3; y: 24; width: 41; height: 16; clip: true; text: "Title"; font.pixelSize: 12 }
            Text { visible: !form.modelData.background; x: 3; y: 49; width: 41; height: 14; clip: true; text: "Limit"; font.pixelSize: 12 }
            Text { visible: !form.modelData.background; x: 143; y: 49; width: 38; height: 14; clip: true; text: "Type"; font.pixelSize: 12 }
            Rectangle {
                objectName: form.objectName+"TitleEdit"
                x: 45; y: 24; width: 228; height: 16; color: "#f2f2f2"; clip: true
                Text { text: form.modelData.title; font.pixelSize: 12; font.family: "Segoe UI"; textFormat: Text.PlainText }
            }
            Rectangle {
                objectName: form.objectName+"PasswordEdit"
                x: 182; y: 70; width: 90; height: 16; color: "#f2f2f2"; clip: true
                Text { text: form.modelData.masked; font.pixelSize: 12; font.family: "Segoe UI"; textFormat: Text.PlainText }
            }
            Rectangle {
                objectName: form.objectName+"People"
                x: 48; y: 49; width: 68; height: 14; color: "#f2f2f2"; clip: true
                Text { text: form.modelData.peopleLabel; font.pixelSize: 12; font.family: "Segoe UI"; textFormat: Text.PlainText }
            }
            Rectangle {
                x: 187; y: 49; width: 68; height: 14; color: "#f2f2f2"; clip: true
                Text { text: form.modelData.classLabel; font.pixelSize: 12; font.family: "Segoe UI"; textFormat: Text.PlainText }
            }
            Text { x: 44; y: 70; width: 40; height: 12; clip: true; text: (form.modelData.private?"o ":"* ")+form.modelData.publicLabel; font.pixelSize: 12; font.family: "Segoe UI"; textFormat: Text.PlainText }
            Text { x: 84; y: 70; width: 52; height: 12; clip: true; text: (form.modelData.private?"* ":"o ")+form.modelData.privateLabel; font.pixelSize: 12; font.family: "Segoe UI"; textFormat: Text.PlainText }
            Repeater {
                model: form.modelData.buttons
                delegate: SourceButton { required property var modelData; button: modelData }
            }
            Rectangle {
                objectName: form.objectName+"Popup"
                visible: form.modelData.popupVisible; x: form.modelData.popupX; y: form.modelData.popupY
                width: form.modelData.popupWidth; height: form.modelData.popupHeight; color: "white"; clip: true
                Repeater {
                    model: form.modelData.popupRows
                    delegate: Text { required property var modelData; required property int index; x: 2; y: index*16; width: 72; height: 16; clip: true; text: modelData; font.pixelSize: 12; font.family: "Segoe UI"; textFormat: Text.PlainText }
                }
            }
        }
    }
    Rectangle {
        parent: forms.parent.parent
        objectName: "sourceRoomValidation"
        x: (forms.message.x || 0)*forms.parent.scale; y: (forms.message.y || 0)*forms.parent.scale; width: forms.message.width || 0; height: forms.message.height || 0
        scale: forms.parent.scale; transformOrigin: Item.TopLeft
        z: 10000; visible: forms.message.visible || false; color: "#e8e8e8"; clip: true
        Rectangle {
            x: 10; y: 22; width: 260; height: forms.message.bodyHeight || 65; color: "#f2f2f2"; clip: true
            Text { objectName: "sourceRoomValidationText"; anchors.fill: parent; text: forms.message.text || ""; wrapMode: Text.WordWrap; font.pixelSize: 12; font.family: "Segoe UI"; textFormat: Text.PlainText }
        }
        SourceButton { button: forms.message.button || ({}) }
        SourceButton { visible: forms.message.hasCancel || false; button: forms.message.cancelButton || ({}) }
    }
}
