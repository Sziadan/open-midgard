import QtQuick 2.15

RoFrame {
    id: panel
    RoTheme { id: theme }
    property var info: ({})
    property bool mini: false

    Text {
        x: panel.mini ? 20 : 12; y: 2
        width: panel.width - 42
        text: panel.mini ? (panel.info.name || "Character") : "Basic Info"
        color: theme.titleText
        font.family: theme.fontFamily
        font.pixelSize: theme.titleSize
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }
    Repeater {
        model: panel.info.systemButtons || []
        delegate: RoChromeButton {
            required property var modelData
            x: modelData.x - panel.x; y: modelData.y - panel.y
            width: modelData.width; height: modelData.height
            visible: modelData.visible
            label: modelData.label
            hovered: modelData.hovered; pressed: modelData.pressed
        }
    }
    Column {
        visible: panel.mini
        x: 12; y: 23
        spacing: 3
        Text {
            text: panel.info.miniHeaderText || ""
            color: theme.ink
            font.family: theme.fontFamily; font.pixelSize: 11
        }
        Text {
            text: panel.info.miniStatusText || ""
            color: theme.muted
            font.family: theme.fontFamily; font.pixelSize: 10
        }
    }
    Item {
        visible: !panel.mini
        anchors.fill: parent
        Text {
            x: 12; y: 25; width: 208
            text: panel.info.name || "Character"
            color: theme.ink
            font.family: theme.fontFamily; font.pixelSize: 15; font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Text {
            x: 12; y: 45; width: 208
            text: panel.info.jobName || ""
            color: theme.muted
            font.family: theme.fontFamily; font.pixelSize: 11
            elide: Text.ElideRight
        }
        RoGauge {
            x: 12; y: 66; width: 208; height: 19
            value: panel.info.hp || 0; maximum: panel.info.maxHp || 0
            readonly property bool low: maximum > 0 && value / maximum < 0.25
            fillColor: low ? theme.warning : theme.hp
            label: low ? "HP · Low" : "HP"
            valueText: (panel.info.hp || 0) + " / " + (panel.info.maxHp || 0)
        }
        RoGauge {
            x: 12; y: 91; width: 208; height: 19
            value: panel.info.sp || 0; maximum: panel.info.maxSp || 0
            fillColor: theme.sp
            label: "SP"
            valueText: (panel.info.sp || 0) + " / " + (panel.info.maxSp || 0)
        }
        Repeater {
            model: [
                { label: "Base Lv. " + (panel.info.level || 0), value: panel.info.expPercent || 0, ink: theme.experience },
                { label: "Job Lv. " + (panel.info.jobLevel || 0), value: panel.info.jobExpPercent || 0, ink: theme.jobExperience }
            ]
            delegate: Item {
                required property int index
                required property var modelData
                x: 12; y: 118 + index * 22; width: 208; height: 19
                Text {
                    text: modelData.label
                    color: theme.muted
                    font.family: theme.fontFamily; font.pixelSize: 10
                }
                Text {
                    anchors.right: parent.right
                    text: modelData.value + "%"
                    color: theme.ink
                    font.family: theme.fontFamily; font.pixelSize: 10
                }
                RoGauge {
                    y: 15; width: parent.width; height: 4
                    value: modelData.value; fillColor: modelData.ink
                    showText: false
                }
            }
        }
        Rectangle { x: 228; y: 28; width: 1; height: 136; color: theme.divider }
        Rectangle { x: 12; y: 169; width: panel.width - 24; height: 1; color: theme.divider }
        Text {
            x: 12; y: 177; width: 144
            text: panel.info.weightText || ""
            color: panel.info.maxWeight > 0 && panel.info.weight / panel.info.maxWeight >= 0.5 ? theme.warning : theme.muted
            font.family: theme.fontFamily; font.pixelSize: 10
            elide: Text.ElideRight
        }
        Text {
            x: 164; y: 177; width: panel.width - 176
            text: panel.info.moneyText || ""
            color: theme.ink
            font.family: theme.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideLeft
        }
    }
    Repeater {
        model: panel.info.menuButtons || []
        delegate: RoChromeButton {
            required property var modelData
            x: modelData.x - panel.x; y: modelData.y - panel.y
            width: modelData.width; height: modelData.height
            visible: !panel.mini && modelData.visible
            icon: modelData.icon || ""
            label: modelData.label || ""
            hovered: modelData.hovered; pressed: modelData.pressed
        }
    }
}
