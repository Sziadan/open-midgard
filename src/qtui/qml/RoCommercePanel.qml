import QtQuick 2.15

RoFrame {
    id: panel
    RoTheme { id: theme }
    showTitle: false
    property bool nativeInteractive: true
    property var info: ({})
    property string kind: ""
    property string caption: ""
    property string closeAction: "close"
    property alias body: content
    visible: !!info.visible
    width: info.width || 400; height: info.height || 420
    x: Math.min(info.x || 0, Math.max(0, parent.width - width))
    y: Math.min(info.y || 0, Math.max(0, parent.height - height))
    focus: visible
    Rectangle { x:0; y:0; width:parent.width; height:30; radius:6; z:-1; gradient:Gradient { GradientStop {position:0;color:theme.titleTop} GradientStop {position:1;color:theme.titleBottom} } }
    Keys.onEscapePressed: function(event) { uiState.commerceAction(closeAction, {to: kind}); event.accepted = true }
    Text { x: 14; y: 7; text: panel.caption; font.family: theme.fontFamily; font.pixelSize: 13; font.bold: true; color: theme.titleText }
    MouseArea {
        enabled: panel.kind!=="playerMenu"
        x: 0; y: 0; width: parent.width - 34; height: 30
        property point start
        property point origin
        onPressed: function(mouse) { start = mapToItem(panel.parent, mouse.x, mouse.y); origin = Qt.point(panel.x, panel.y) }
        onPositionChanged: function(mouse) { if (pressed) { var p = mapToItem(panel.parent, mouse.x, mouse.y); uiState.commerceAction("moveWindow", {to: panel.kind, x: origin.x + p.x - start.x, y: origin.y + p.y - start.y}) } }
    }
    Text { anchors.right: parent.right; anchors.rightMargin: 12; y: 5; text: "×"; color: theme.titleText; font.pixelSize: 18
        MouseArea { anchors.fill: parent; anchors.margins: -6; onClicked: uiState.commerceAction(panel.closeAction, {to: panel.kind}) }
    }
    Item { id: content; x: 12; y: 38; width: parent.width - 24; height: parent.height - 50 }
}
