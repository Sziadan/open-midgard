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
    readonly property bool inventoryPanel: kind==="inventory"
    readonly property bool collapsed: inventoryPanel && !!info.mini
    readonly property int titleHeight: inventoryPanel ? 17 : 30
    visible: !!info.visible
    width: info.width || 400; height: info.height || 420
    x: inventoryPanel ? (info.x || 0) : Math.min(info.x || 0, Math.max(0, parent.width - width))
    y: inventoryPanel ? (info.y || 0) : Math.min(info.y || 0, Math.max(0, parent.height - height))
    focus: visible
    Rectangle { x:0; y:0; width:parent.width; height:panel.titleHeight; radius:6; z:-1; gradient:Gradient { GradientStop {position:0;color:theme.titleTop} GradientStop {position:1;color:theme.titleBottom} } }
    Keys.onEscapePressed: function(event) { uiState.commerceAction(closeAction, {to: kind}); event.accepted = true }
    Text { x: panel.inventoryPanel ? 8 : 14; y: panel.inventoryPanel ? 1 : 7; text: panel.caption; font.family: theme.fontFamily; font.pixelSize: panel.inventoryPanel ? 11 : 13; font.bold: true; color: theme.titleText }
    MouseArea {
        enabled: panel.kind!=="playerMenu"
        x: 0; y: 0; width: parent.width - (panel.inventoryPanel ? 52 : 34); height: panel.titleHeight
        property point start
        property point origin
        onPressed: function(mouse) { start = mapToItem(panel.parent, mouse.x, mouse.y); origin = Qt.point(panel.x, panel.y) }
        onPositionChanged: function(mouse) { if (pressed) { var p = mapToItem(panel.parent, mouse.x, mouse.y); uiState.commerceAction("moveWindow", {to: panel.kind, x: origin.x + p.x - start.x, y: origin.y + p.y - start.y}) } }
        onReleased: if(panel.inventoryPanel)uiState.commerceAction("moveWindow",{to:panel.kind,x:panel.x,y:panel.y,amount:1})
    }
    Text { anchors.right: parent.right; anchors.rightMargin: panel.inventoryPanel ? 5 : 12; y: panel.inventoryPanel ? -2 : 5; text: "×"; color: theme.titleText; font.pixelSize: 18
        objectName: panel.inventoryPanel ? "commerceInventoryClose" : ""
        MouseArea { anchors.fill: parent; anchors.margins: panel.inventoryPanel ? 0 : -6; onClicked: uiState.commerceAction(panel.closeAction, {to: panel.kind}) }
    }
    Text {
        objectName: panel.inventoryPanel ? "commerceInventoryMini" : ""
        visible: panel.inventoryPanel
        x: parent.width-34; y: 0; width: 17; height: 17
        text: panel.collapsed ? "+" : "−"; color: theme.titleText; horizontalAlignment: Text.AlignHCenter
        MouseArea { anchors.fill: parent; onClicked: uiState.commerceAction("inventoryMini", {}) }
    }
    Item { id: content; x: panel.inventoryPanel ? 0 : 12; y: panel.inventoryPanel ? 17 : 38
        width: parent.width - 2*x; height: Math.max(0,parent.height-y-(panel.inventoryPanel ? 0 : 12)); visible: !panel.collapsed }
    MouseArea {
        objectName: panel.inventoryPanel ? "commerceInventoryResize" : ""
        visible: panel.inventoryPanel && !panel.collapsed
        anchors.right: parent.right; anchors.bottom: parent.bottom; width: 15; height: 15
        cursorShape: Qt.SizeFDiagCursor
        Repeater { model: 3
            Rectangle { required property int index; width: 1; height: 4+index*4
                x: 12-index*3; y: 15-height; rotation: 45; color: theme.muted }
        }
        property point start
        property size original
        function apply(mouse,finished) {
            var p=mapToItem(panel.parent,mouse.x,mouse.y)
            uiState.commerceAction("resizeInventory",{x:original.width+p.x-start.x,y:original.height+p.y-start.y,amount:finished ? 1 : 0})
        }
        onPressed: function(mouse) { start=mapToItem(panel.parent,mouse.x,mouse.y); original=Qt.size(panel.width,panel.height) }
        onPositionChanged: function(mouse) { if(pressed)apply(mouse,false) }
        onReleased: function(mouse) { apply(mouse,true) }
    }
}
