import QtQuick 2.15

RoFrame {
    id: panel
    property var info: ({})
    property bool mini: false
    readonly property bool compact: info.compact || false
    property string previewSource: ""
    RoTheme { id: theme }

    Text {
        x: 12; y: 2; text: "Equipment"
        color: theme.titleText
        font.family: theme.fontFamily; font.pixelSize: theme.titleSize; font.weight: Font.DemiBold
    }
    Text {
        x: 12; y: 26; text: "EQUIPPED ITEMS"
        visible: !panel.mini && !panel.compact
        color: theme.muted
        font.family: theme.fontFamily; font.pixelSize: 9; font.letterSpacing: 0.7
    }
    Item {
        anchors.fill: parent; visible: !panel.mini
        Rectangle {
            objectName: "equipmentPreview"
            x: panel.info.previewX || 0; y: panel.info.previewY || 0
            width: panel.info.previewWidth || 0; height: panel.info.previewHeight || 0
            radius: theme.buttonRadius
            color: theme.rowAlt; border.width: 1; border.color: theme.divider
            Image {
                anchors.centerIn: parent
                width: Math.max(1,parent.width-16); height: Math.max(1,parent.height-20)
                source: panel.visible && !panel.mini ? panel.previewSource : ""
                fillMode: Image.PreserveAspectFit; smooth: false; cache: false
            }
        }
        Repeater {
            model: panel.info.slots || []
            delegate: Rectangle {
                required property var modelData
                required property int index
                objectName: "equipmentCard" + index
                x: modelData.cardX - panel.x; y: modelData.cardY - panel.y
                width: modelData.cardWidth; height: modelData.cardHeight
                radius: theme.buttonRadius
                color: modelData.hovered ? theme.rowHover : theme.fieldFill
                border.width: 1
                border.color: modelData.hovered ? theme.focus : theme.divider
                Rectangle {
                    objectName: "equipmentIcon" + index
                    x: modelData.x - modelData.cardX; y: modelData.y - modelData.cardY
                    width: modelData.width; height: modelData.height
                    radius: theme.buttonRadius
                    color: modelData.occupied ? theme.rowSelected : theme.rowAlt
                    border.width: 1; border.color: theme.buttonBorder
                    Image {
                        anchors.centerIn: parent
                        width: 28; height: 28; smooth: false
                        fillMode: Image.PreserveAspectFit
                        source: modelData.occupied && modelData.itemId > 0
                            ? "image://openmidgard/item/" + modelData.itemId : ""
                    }
                    Text {
                        anchors.centerIn: parent; visible: !modelData.occupied
                        text: "–"; color: theme.disabledText
                        font.family: theme.fontFamily; font.pixelSize: 14
                    }
                }
                Text {
                    x: modelData.nameX - modelData.cardX; y: 5
                    visible: !panel.compact
                    width: modelData.nameWidth; height: 12
                    text: modelData.slotName
                    textFormat: Text.PlainText
                    color: theme.muted
                    font.family: theme.fontFamily; font.pixelSize: 9
                }
                Text {
                    objectName: "equipmentName" + index
                    visible: !panel.compact
                    x: modelData.nameX - modelData.cardX
                    y: modelData.nameY - modelData.cardY
                    width: modelData.nameWidth; height: modelData.nameHeight
                    text: modelData.occupied ? modelData.label : "Empty"
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    color: modelData.occupied ? theme.ink : theme.disabledText
                    font.family: theme.fontFamily; font.pixelSize: 11
                    verticalAlignment: Text.AlignTop
                }
            }
        }
    }
    Repeater {
        model: panel.info.systemButtons || []
        delegate: RoChromeButton {
            required property var modelData
            objectName: modelData.id === 138 ? "equipmentRemoveOption" : ""
            z: 1
            x: modelData.x - panel.x; y: modelData.y - panel.y
            width: modelData.width; height: modelData.height
            visible: modelData.visible; label: modelData.label
            icon: modelData.icon || ""
            titleControl: modelData.id === 145
            labelSize: modelData.id === 138 ? 10 : 8
        }
    }
}
