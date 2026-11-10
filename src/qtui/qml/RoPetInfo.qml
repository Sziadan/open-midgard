import QtQuick

Item {
    id: pet
    property var petData: ({})
    objectName: "sourcePetInfo"
    x: petData.x || 0
    y: petData.y || 0
    width: petData.width || 0
    height: petData.height || 0
    z: petData.layer || 0
    visible: petData.visible || false
    clip: true

    Rectangle {
        x: 0
        y: 0
        width: pet.width
        height: pet.height
        color: "white"
    }

    Repeater {
        model: pet.petData.framePieces || []
        delegate: Image {
            required property var modelData
            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            source: modelData.artwork || ""
            z: modelData.phase ? 3 : 1
            smooth: false
        }
    }

    Rectangle {
        x: 0
        y: 17
        width: 1
        height: Math.max(0, pet.height - 38)
        color: "#808080"
        z: 2
    }

    Rectangle {
        x: pet.width - 1
        y: 17
        width: 1
        height: Math.max(0, pet.height - 38)
        color: "#808080"
        z: 2
    }

    Rectangle {
        x: 160
        y: 44
        width: 110
        height: 18
        color: pet.petData.commandBoxColor || "#f0f0f0"
        visible: pet.petData.commandBoxVisible || false
        z: 4
    }

    Repeater {
        model: pet.petData.texts || []
        delegate: Text {
            required property var modelData
            x: modelData.x
            y: modelData.y
            text: modelData.text || ""
            z: 5
            color: modelData.color || "black"
            font.family: "Segoe UI"
            font.pixelSize: 12
            textFormat: Text.PlainText
            renderType: Text.NativeRendering
        }
    }

    Image {
        z: 6
        x: 5
        y: 21
        width: pet.petData.portrait ? pet.petData.portrait.width : 0
        height: pet.petData.portrait ? pet.petData.portrait.height : 0
        source: pet.petData.portrait ? (pet.petData.portrait.artwork || "") : ""
        smooth: false
    }

    Repeater {
        model: pet.petData.buttons || []
        delegate: Item {
            z: 7
            required property var modelData
            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            Rectangle { anchors.fill: parent; visible: !modelData.artwork; color: modelData.pressed ? "#b4b4b4" : "#d2d2d2" }
            Image {
                width: modelData.imageWidth || 0
                height: modelData.imageHeight || 0
                source: modelData.artwork || ""
                smooth: false
            }
            Text { visible: !modelData.artwork; text: modelData.label || ""; font.family: "Segoe UI"; font.pixelSize: 12; color: "black"; textFormat: Text.PlainText }
        }
    }

    Rectangle {
        visible: pet.petData.edit ? pet.petData.edit.visible : false
        z: 7
        x: pet.petData.edit ? pet.petData.edit.x : 0
        y: pet.petData.edit ? pet.petData.edit.y : 0
        width: pet.petData.edit ? pet.petData.edit.width : 0
        height: pet.petData.edit ? pet.petData.edit.height : 0
        color: "#e8e8e8"
        border.color: "#808080"
        border.width: 1
        clip: true
        Text {
            id: editText
            x: 1
            y: 1
            text: pet.petData.edit ? (pet.petData.edit.text || "") : ""
            color: "black"
            font.family: "Segoe UI"
            font.pixelSize: 12
            textFormat: Text.PlainText
            renderType: Text.NativeRendering
        }
        Rectangle { x: 1 + editText.implicitWidth; y: 2; width: 1; height: parent.height - 4; color: "black"; visible: pet.petData.edit ? pet.petData.edit.focused : false }
    }

    Rectangle {
        parent: pet.parent
        objectName: "sourcePetCommandMenu"
        x: pet.petData.menu ? pet.petData.menu.x : 0
        y: pet.petData.menu ? pet.petData.menu.y : 0
        width: pet.petData.menu ? pet.petData.menu.width : 0
        height: pet.petData.menu ? pet.petData.menu.height : 0
        z: pet.petData.menu ? pet.petData.menu.layer : 0
        visible: pet.petData.visible && pet.petData.menu ? pet.petData.menu.visible : false
        color: "#f0f0f0"
        border.color: "#808080"
        border.width: 0
        clip: true
        Repeater {
            model: pet.petData.menu ? (pet.petData.menu.rows || []) : []
            delegate: Item {
                required property var modelData
                required property int index
                y: index * 16
                width: parent.width
                height: 16
                Rectangle {
                    x: 0
                    y: 0
                    width: parent.width
                    height: 16
                    color: "#32c8c8"
                    visible: index === pet.petData.menu.selection
                }
                Text {
                    x: 3
                    y: 0
                    width: parent.width - 3
                    height: 18
                    clip: true
                    text: modelData
                    color: "black"
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    textFormat: Text.PlainText
                    renderType: Text.NativeRendering
                }
            }
        }
    }
}
