import QtQuick

Item {
    id: bar
    property var trackBounds: ({})
    property var thumbBounds: ({})
    property var upBounds: ({})
    property var downBounds: ({})
    property string arrowPrefix: "sourceScrollArrow"
    property bool shown: false
    x: trackBounds.x || 0; y: trackBounds.y || 0
    width: Math.max(0,trackBounds.width || 0); height: Math.max(0,trackBounds.height || 0)
    visible: shown; clip: true
    Rectangle { anchors.fill: parent; color: "#404040" }
    Rectangle {
        objectName: bar.arrowPrefix + "Thumb"
        x: (bar.thumbBounds.x || 0)-bar.x; y: (bar.thumbBounds.y || 0)-bar.y
        width: bar.thumbBounds.width || 0; height: bar.thumbBounds.height || 0
        color: "#b8b8b8"
    }
    Repeater {
        model: [bar.upBounds,bar.downBounds]
        delegate: Rectangle {
            required property var modelData
            required property int index
            objectName: bar.arrowPrefix+index
            x: (modelData.x || 0)-bar.x; y: (modelData.y || 0)-bar.y
            width: modelData.width || 0; height: modelData.height || 0
            color: "#606060"
            Text { anchors.fill: parent; text: parent.index === 0 ? "^" : "v"; color: "#f0f0f0"; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        }
    }
}
