import QtQuick

Item {
    id: taming
    objectName: "sourcePetTaming"
    property var tamingData: ({})
    property real uiScale: 1.0
    x: (tamingData.x || 0) * uiScale
    y: (tamingData.y || 0) * uiScale
    width: tamingData.width || 0
    height: tamingData.height || 0
    scale: uiScale
    transformOrigin: Item.TopLeft
    z: 40 + (tamingData.layer || 0)
    visible: tamingData.visible || false
    clip: true
    Image {
        anchors.fill: parent
        source: taming.tamingData.artwork || ""
        smooth: false
        cache: false
    }
}
