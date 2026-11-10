import QtQuick

Rectangle {
    id: tip
    property var tipData: ({})
    objectName: "sourceTip"
    x: tipData.x || 0; y: tipData.y || 0; width: tipData.width || 0; height: tipData.height || 0
    z: tipData.layer || 0; visible: tipData.visible || false; color: "#e8e8e8"; clip: true
    Rectangle { width: parent.width; height: 17; color: "#828c96" }
    Repeater {
        model: tip.tipData.buttons || []
        delegate: Rectangle {
            required property var modelData
            objectName: "sourceTipButton"+modelData.id
            x: modelData.x; y: modelData.y; width: modelData.width; height: modelData.height
            color: modelData.artwork?"transparent":modelData.state===2?"#b4b4b4":"#d2d2d2"
            Image { width: parent.modelData.imageWidth || 0; height: parent.modelData.imageHeight || 0; source: parent.modelData.artwork || ""; smooth: false }
            Text { visible: !parent.modelData.artwork; text: parent.modelData.label; color: "black"; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12 }
        }
    }
    Rectangle {
        id: viewer
        objectName: "sourceTipViewer"
        x: 10; y: 22; width: 260; height: 125; color: "white"; clip: true
        Item {
            width: viewer.width-((tip.tipData.scroll || {}).visible?13:0); height: viewer.height; clip: true
            Repeater {
                model: tip.tipData.rows || []
                delegate: Row {
                    required property var modelData
                    required property int index
                    objectName: "sourceTipRow"+index
                    x: 3; y: 4+18*index; height: 14
                    Repeater {
                        model: parent.modelData.spans || []
                        delegate: Text {
                            required property var modelData
                            text: modelData.text; color: modelData.color; textFormat: Text.PlainText
                            font.family: "Segoe UI"; font.pixelSize: 12
                        }
                    }
                }
            }
        }
        RoSourceScrollBar {
            objectName: "sourceTipScroll"
            shown: !!(tip.tipData.scroll || {}).visible
            trackBounds: (tip.tipData.scroll || {}).track || ({})
            thumbBounds: (tip.tipData.scroll || {}).thumb || ({})
            upBounds: (tip.tipData.scroll || {}).up || ({})
            downBounds: (tip.tipData.scroll || {}).down || ({})
            arrowPrefix: "sourceTipScrollArrow"
        }
    }
    Item {
        readonly property var check: tip.tipData.check || ({})
        objectName: "sourceTipStartup"
        x: check.x || 0; y: check.y || 0; width: check.width || 0; height: check.height || 0; clip: true
        Image { y: Math.trunc((12-(parent.check.imageHeight || 0))/2); width: parent.check.imageWidth || 0; height: parent.check.imageHeight || 0; source: parent.check.artwork || ""; smooth: false }
        Rectangle { width: 10; height: 10; color: "white"; visible: !parent.check.artwork; Text { text: parent.parent.check.checked?"x":""; color: "black"; font.pixelSize: 12; font.family: "Segoe UI" } }
        Text { x: 11; width: parent.width-11; height: 12; clip: true; text: parent.check.label || ""; color: "black"; textFormat: Text.PlainText; font.family: "Segoe UI"; font.pixelSize: 12 }
    }

}
