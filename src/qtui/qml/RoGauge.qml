import QtQuick 2.15

Rectangle {
    id: gauge
    RoTheme { id: theme }
    property real value: 0
    property real maximum: 100
    property color fillColor: theme.sp
    property string label: ""
    property string valueText: ""
    property bool showText: true
    radius: Math.min(4, height / 2)
    color: theme.gaugeTrack
    clip: true
    Rectangle {
        width: gauge.width * Math.max(0, Math.min(1, gauge.maximum > 0 ? gauge.value / gauge.maximum : 0))
        height: gauge.height
        color: gauge.fillColor
        radius: gauge.radius
    }
    Text {
        visible: gauge.showText
        x: 7
        anchors.verticalCenter: parent.verticalCenter
        text: gauge.label
        color: theme.ink
        font.family: theme.fontFamily
        font.pixelSize: theme.captionSize
        font.bold: true
    }
    Rectangle {
        visible: gauge.showText
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 3
        width: Math.min(parent.width - 38, gaugeValue.implicitWidth + 10)
        height: parent.height - 4
        radius: 3
        color: "#edffffff"
        Text {
            id: gaugeValue
            anchors.fill: parent
            text: gauge.valueText
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: theme.numberSize
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideLeft
        }
    }
}
