import QtQuick 2.15

RoFrame {
    id: panel
    property var info: ({})
    property bool mini: false
    RoTheme { id: theme }

    Text {
        x: 12; y: 2
        text: "Status"
        color: theme.titleText
        font.family: theme.fontFamily; font.pixelSize: theme.titleSize
        font.weight: Font.DemiBold
    }
    Text {
        x: 92; y: 2; width: panel.width - 140
        visible: panel.mini
        text: panel.info.miniPointsText || ""
        color: theme.titleText
        font.family: theme.fontFamily; font.pixelSize: 11
    }
    Repeater {
        model: panel.info.systemButtons || []
        delegate: RoChromeButton {
            required property var modelData
            x: modelData.x - panel.x; y: modelData.y - panel.y
            width: modelData.width; height: modelData.height
            visible: modelData.visible; label: modelData.label
        }
    }

    Item {
        anchors.fill: parent
        visible: !panel.mini

        Text {
            x: 12; y: 31
            text: "ATTRIBUTES"
            color: theme.muted
            font.family: theme.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold
            font.letterSpacing: 0.7
        }
        Text {
            x: 176; y: 31; width: 28
            text: "COST"
            color: theme.muted
            font.family: theme.fontFamily; font.pixelSize: 9
            horizontalAlignment: Text.AlignRight
        }
        Rectangle { x: 218; y: 31; width: 1; height: 181; color: theme.divider }
        Repeater {
            model: panel.info.stats || []
            delegate: Item {
                required property var modelData
                required property int index
                x: 8; y: modelData.rowY
                width: 200; height: 24
                Rectangle {
                    anchors.fill: parent; radius: theme.buttonRadius
                    color: index % 2 === 0 ? theme.rowAlt : "transparent"
                }
                Text {
                    x: 4; width: 32; height: parent.height
                    text: modelData.label
                    color: theme.ink
                    font.family: theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold
                    verticalAlignment: Text.AlignVCenter
                }
                Text {
                    x: 40; width: 30; height: parent.height
                    text: modelData.base
                    color: theme.ink
                    font.family: theme.fontFamily; font.pixelSize: 12
                    horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                    fontSizeMode: Text.Fit; minimumPixelSize: 9
                }
                Text {
                    x: 74; width: 14; height: parent.height
                    text: modelData.bonus < 0 ? "−" : "+"
                    color: theme.muted
                    font.family: theme.fontFamily; font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                Text {
                    x: 88; width: 36; height: parent.height
                    text: Math.abs(modelData.bonus)
                    color: modelData.bonus < 0 ? theme.warning : (modelData.bonus > 0 ? theme.focus : theme.muted)
                    font.family: theme.fontFamily; font.pixelSize: 12
                    horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                    fontSizeMode: Text.Fit; minimumPixelSize: 9
                }
                RoChromeButton {
                    objectName: "statusIncrease" + index
                    x: modelData.increaseX - panel.x - parent.x
                    y: modelData.increaseY - panel.y - parent.y
                    width: modelData.increaseWidth; height: modelData.increaseHeight
                    label: "+"; labelSize: 14
                    enabled: modelData.canIncrease
                    color: enabled ? theme.rowSelected : theme.rowAlt
                    border.color: enabled ? theme.focus : theme.buttonBorder
                }
                Text {
                    x: 168; width: 28; height: parent.height
                    text: modelData.cost > 0 ? modelData.cost : "–"
                    color: modelData.canIncrease ? theme.ink : theme.muted
                    font.family: theme.fontFamily; font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                    fontSizeMode: Text.Fit; minimumPixelSize: 9
                }
            }
        }
        Text {
            x: 230; y: 31
            text: "COMBAT"
            color: theme.muted
            font.family: theme.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold
            font.letterSpacing: 0.7
        }
        Repeater {
            model: [
                { label: "Atk", value: panel.info.attackText || "0" },
                { label: "Def", value: panel.info.itemDefText || "0" },
                { label: "Matk", value: panel.info.matkText || "0" },
                { label: "Mdef", value: panel.info.itemMdefText || "0" },
                { label: "Hit", value: String(panel.info.hit || 0) },
                { label: "Flee", value: panel.info.fleeText || "0" },
                { label: "Crit", value: String(panel.info.critical || 0) },
                { label: "Aspd", value: panel.info.aspdText || "0" }
            ]
            delegate: Rectangle {
                required property var modelData
                required property int index
                x: 230 + (index % 2) * 99; y: 54 + Math.floor(index / 2) * 42
                width: 91; height: 36
                radius: theme.buttonRadius
                color: theme.fieldFill
                border.width: 1; border.color: theme.divider
                Text {
                    x: 8; y: 3; width: parent.width - 16; height: 12
                    text: modelData.label
                    color: theme.muted
                    font.family: theme.fontFamily; font.pixelSize: 9
                }
                Text {
                    objectName: "statusCombatValue" + index
                    x: 8; y: 16; width: parent.width - 16; height: 16
                    text: modelData.value
                    color: theme.ink
                    font.family: theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold
                    fontSizeMode: Text.Fit; minimumPixelSize: 8
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
        Rectangle {
            x: 12; y: 224; width: panel.width - 24; height: 1; color: theme.divider
        }
        Text {
            x: 12; y: 230; height: 14
            text: "Status points"
            color: theme.muted
            font.family: theme.fontFamily; font.pixelSize: 11
        }
        Text {
            x: 142; y: 229; width: panel.width - 154; height: 16
            text: String(panel.info.statusPoint || 0)
            color: (panel.info.statusPoint || 0) > 0 ? theme.focus : theme.muted
            font.family: theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignRight
        }
    }
}
