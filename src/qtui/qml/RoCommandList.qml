import QtQuick

Rectangle {
    id: commandList
    property var commandData: ({})
    objectName: "sourceCommandList"
    x: commandData.x || 0
    y: commandData.y || 0
    width: commandData.width || 0
    height: commandData.height || 0
    z: commandData.layer || 0
    visible: commandData.visible || false
    color: "transparent"
    clip: true

    Rectangle { z: 0; anchors.fill: parent; color: "#d7d7d7" }
    Rectangle { z: 2; x: 0; y: 17; width: parent.width; height: parent.height-17; color: "#d7d7d7" }
    Rectangle { z: 2; x: 1; y: 17; width: parent.width-2; height: parent.height-17; color: "white" }
    Rectangle { z: 4; x: 18; y: 17; width: 1; height: parent.height-21; color: "#d7d7d7" }

    Repeater {
        model: commandList.commandData.framePieces || []
        delegate: Image {
            required property var modelData
            x: modelData.x; y: modelData.y
            width: modelData.width; height: modelData.height
            z: modelData.phase === 0 ? 1 : modelData.phase === 1 ? 3 : modelData.phase === 2 ? 5 : 1
            source: modelData.artwork || ""
            smooth: false
            cache: true
        }
    }

    Text {
        z: 6
        x: 18; y: 3; width: parent.width-20; height: 14; clip: true
        text: commandList.commandData.title || ""
        color: "black"; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12
    }
    Text {
        z: 6
        objectName: "sourceCommandListTitle"
        x: 17; y: 2; width: parent.width-20; height: 14; clip: true
        text: commandList.commandData.title || ""
        color: "white"; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12
    }

    Item {
        z: 6
        readonly property var tabData: commandList.commandData.tab || ({})
        objectName: "sourceCommandListTab"
        x: tabData.x || 0; y: tabData.y || 0
        width: tabData.width || 0; height: tabData.height || 0
        Image {
            width: parent.tabData.imageWidth || parent.width
            height: parent.tabData.imageHeight || parent.height
            source: parent.tabData.artwork || ""
            smooth: false
            cache: true
        }
    }

    Repeater {
        model: commandList.commandData.buttons || []
        delegate: Rectangle {
            z: 6
            required property var modelData
            objectName: "sourceCommandListButton"+modelData.id
            x: modelData.x; y: modelData.y; width: modelData.width; height: modelData.height
            color: modelData.artwork ? "transparent" : modelData.state===2 ? "#b4b4b4" : "#d2d2d2"
            Image {
                width: parent.modelData.imageWidth || 0
                height: parent.modelData.imageHeight || 0
                source: parent.modelData.artwork || ""
                smooth: false; cache: true
            }
            Text {
                visible: !parent.modelData.artwork
                text: parent.modelData.label || ""
                color: "black"; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12
            }
        }
    }

    Repeater {
        model: commandList.commandData.rows || []
        delegate: Item {
            z: 6
            required property var modelData
            objectName: "sourceCommandListRow"+modelData.index
            x: modelData.x; y: modelData.y; width: modelData.width; height: modelData.height+2
            clip: true
            Rectangle {
                x: 0; y: 2; width: parent.width; height: 20
                color: "#808080"; visible: parent.modelData.selected
            }
            Rectangle {
                x: 1; y: 3; width: parent.width-2; height: 18
                color: "white"; visible: parent.modelData.selected
            }
            Text {
                x: parent.modelData.textOffset; y: 5; width: parent.width-parent.modelData.textOffset
                height: 14; clip: true; text: parent.modelData.caption || ""
                color: "black"; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12
            }
            Image {
                x: 2; y: 5
                width: parent.modelData.on ? (commandList.commandData.indicatorOnWidth || 0) : (commandList.commandData.indicatorOffWidth || 0)
                height: parent.modelData.on ? (commandList.commandData.indicatorOnHeight || 0) : (commandList.commandData.indicatorOffHeight || 0)
                source: parent.modelData.hasIndicator
                    ? (parent.modelData.on ? commandList.commandData.indicatorOn : commandList.commandData.indicatorOff)
                    : ""
                smooth: false; cache: true
            }
        }
    }

    RoSourceScrollBar {
        z: 6
        objectName: "sourceCommandListScroll"
        shown: !!(commandList.commandData.scroll || {}).visible
        trackBounds: (commandList.commandData.scroll || {}).track || ({})
        thumbBounds: (commandList.commandData.scroll || {}).thumb || ({})
        upBounds: (commandList.commandData.scroll || {}).up || ({})
        downBounds: (commandList.commandData.scroll || {}).down || ({})
        arrowPrefix: "commandListScrollArrow"
    }
}
