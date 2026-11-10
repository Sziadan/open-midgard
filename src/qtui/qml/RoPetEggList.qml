import QtQuick

Item {
    id: eggList
    objectName: "sourcePetEggList"
    property var eggData: ({})
    property real uiScale: 1.0

    x: (eggData.x || 0) * uiScale
    y: (eggData.y || 0) * uiScale
    width: eggData.width || 0
    height: eggData.height || 0
    scale: uiScale
    transformOrigin: Item.TopLeft
    z: 40 + (eggData.layer || 0)
    visible: eggData.visible || false
    clip: true

    // Match the native draw phases: top frame and title, body, bottom frame,
    // then the rows and passive child controls.
    Repeater {
        model: (eggList.eggData.framePieces || []).filter(function(piece) { return piece.phase === 0 })
        delegate: Image {
            required property var modelData
            x: modelData.x; y: modelData.y
            width: modelData.width; height: modelData.height
            source: modelData.artwork || ""
            smooth: false
        }
    }

    Text {
        x: 17; y: 3; width: eggList.width - 17; height: 14
        text: eggList.eggData.title || ""
        color: "white"
        font.family: "Segoe UI"; font.pixelSize: 12
        textFormat: Text.PlainText
        renderType: Text.NativeRendering
    }
    Text {
        x: 16; y: 2; width: eggList.width - 16; height: 15
        text: eggList.eggData.title || ""
        color: "black"
        font.family: "Segoe UI"; font.pixelSize: 12
        textFormat: Text.PlainText
        renderType: Text.NativeRendering
    }

    Rectangle {
        x: 0; y: 17; width: eggList.width; height: Math.max(0, eggList.height - 17)
        color: "#808080"
    }
    Rectangle {
        x: 1; y: 17; width: Math.max(0, eggList.width - 2); height: Math.max(0, eggList.height - 17)
        color: "#f5f2ea"
    }

    Repeater {
        model: (eggList.eggData.framePieces || []).filter(function(piece) { return piece.phase !== 0 })
        delegate: Image {
            required property var modelData
            x: modelData.x; y: modelData.y
            width: modelData.width; height: modelData.height
            source: modelData.artwork || ""
            smooth: false
        }
    }

    Repeater {
        model: eggList.eggData.rows || []
        delegate: Item {
            required property var modelData
            objectName: "sourcePetEggRow" + modelData.index
            x: 0; y: modelData.y; width: eggList.width; height: 32

            Rectangle {
                x: 5; y: -3; width: parent.width - 24; height: 30
                color: "#d7dff0"
                visible: modelData.selected || false
            }
            Image {
                x: 8; y: 0
                width: modelData.imageWidth || 0; height: modelData.imageHeight || 0
                source: modelData.artwork || ""
                smooth: false
            }
            Text {
                x: 40; y: 0; width: Math.max(0, parent.width - 56); height: 30
                text: modelData.text || ""
                color: "black"
                clip: true
                font.family: "Segoe UI"; font.pixelSize: 12
                textFormat: Text.PlainText
                renderType: Text.NativeRendering
            }
        }
    }

    RoSourceScrollBar {
        id: eggScrollBar
        arrowPrefix: "sourcePetEggScrollArrow"
        trackBounds: eggList.eggData.scrollTrack || ({})
        thumbBounds: eggList.eggData.scrollThumb || ({})
        upBounds: eggList.eggData.scrollUp || ({})
        downBounds: eggList.eggData.scrollDown || ({})
        shown: true
    }

    Repeater {
        model: eggList.eggData.buttons || []
        delegate: Item {
            required property var modelData
            required property int index
            objectName: "sourcePetEggButton" + index
            x: modelData.x; y: modelData.y
            width: modelData.width; height: modelData.height
            Rectangle {
                anchors.fill: parent
                visible: !modelData.artwork
                color: modelData.pressed ? "#b4b4b4" : "#d2d2d2"
            }
            Image {
                width: modelData.imageWidth || 0; height: modelData.imageHeight || 0
                source: modelData.artwork || ""
                smooth: false
            }
            Text {
                visible: !modelData.artwork
                text: modelData.label || ""
                font.family: "Segoe UI"; font.pixelSize: 12
                color: "black"; textFormat: Text.PlainText
            }
        }
    }
}
