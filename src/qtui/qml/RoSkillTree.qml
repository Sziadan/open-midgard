import QtQuick 2.15

RoFrame {
    id: panel
    RoTheme { id: theme }
    property var tree: ({})
    readonly property var geometry: tree.geometry || ({})
    Text {
        x: 12; y: 2; width: parent.width - 54
        text: panel.tree.title || "Skill Tree"
        color: theme.titleText
        font.family: theme.fontFamily; font.pixelSize: theme.titleSize; font.weight: Font.DemiBold
        elide: Text.ElideRight
    }
    RoChromeButton { x: panel.width - 34; y: 2; width: 14; height: 13; label: panel.tree.collapsed ? "B" : "_" }
    RoChromeButton { x: panel.width - 18; y: 2; width: 14; height: 13; label: "X" }
    Item {
        visible: !panel.tree.collapsed
        anchors.fill: parent
        Text {
            x: 12; y: 27
            text: "Available " + (panel.tree.points || 0) + "  /  Planned " + (panel.tree.pending || 0)
            color: theme.ink
            font.family: theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold
        }
        Text {
            anchors.right: parent.right; anchors.rightMargin: 12; y: 28
            visible: panel.width > 570
            text: "Drag empty space to pan · Wheel to scroll"
            color: theme.muted
            font.family: theme.fontFamily; font.pixelSize: 10
        }
        Rectangle {
            id: viewport
            x: 8; y: 50; width: panel.width - 16; height: Math.max(1, panel.height - 142)
            color: theme.rowAlt
            radius: 6; border.width: 1; border.color: theme.fieldBorder
            clip: true
            Canvas {
                id: connections
                anchors.fill: parent
                property var edges: panel.tree.edges || []
                onEdgesChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset(); ctx.clearRect(0,0,width,height)
                    for (let i = 0; i < edges.length; ++i) {
                        const edge = edges[i]
                        ctx.strokeStyle = edge.highlight ? (edge.met ? theme.experience : theme.gold) : theme.scrollThumb
                        ctx.lineWidth = edge.highlight ? 3 : 1.2
                        const middle = (edge.y1 + edge.y2) / 2
                        ctx.beginPath(); ctx.moveTo(edge.x1,edge.y1)
                        if (edge.detour) {
                            // Bypass intermediate cards using the column gutter.
                            const gutter = edge.x2 + panel.geometry.width / 2 + 7
                            ctx.lineTo(edge.x1,edge.y1+8)
                            ctx.lineTo(gutter,edge.y1+8)
                            ctx.lineTo(gutter,edge.y2-8)
                            ctx.lineTo(edge.x2,edge.y2-8)
                            ctx.lineTo(edge.x2,edge.y2)
                        } else {
                            ctx.bezierCurveTo(edge.x1,middle,edge.x2,middle,edge.x2,edge.y2)
                        }
                        ctx.moveTo(edge.x2-3,edge.y2-4)
                        ctx.lineTo(edge.x2,edge.y2)
                        ctx.lineTo(edge.x2+3,edge.y2-4)
                        ctx.stroke()
                    }
                }
            }
            Repeater {
                model: panel.tree.nodes || []
                delegate: Rectangle {
                    id: node
                    required property var modelData
                    x: modelData.x; y: modelData.y; width: panel.geometry.width; height: panel.geometry.height
                    radius: 6
                    color: modelData.hovered ? theme.rowHover : theme.fieldFill
                    border.width: modelData.prerequisite || modelData.available || modelData.pending ? 2 : 1
                    border.color: modelData.prerequisite ? theme.gold : modelData.pending ? theme.warning
                        : modelData.available ? theme.focus : theme.fieldBorder
                    Text {
                        x: 4; y: 2; width: parent.width - 8; height: 26
                        text: node.modelData.name
                        color: theme.ink
                        font.family: theme.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold
                        wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Rectangle {
                        x: panel.geometry.iconX; y: panel.geometry.iconY; width: panel.geometry.iconSize; height: panel.geometry.iconSize
                        radius: 4; color: theme.bodyTop
                        border.width: node.modelData.available ? 2 : 1
                        border.color: node.modelData.available ? theme.focus : theme.buttonBorder
                        Image {
                            id: skillIcon
                            anchors.fill: parent; anchors.margins: 2
                            source: "image://openmidgard/skill/" + node.modelData.id
                                + "?symbol=" + encodeURIComponent(node.modelData.symbol)
                                + (node.modelData.learned > 0 ? "" : "&grey=1")
                            fillMode: Image.PreserveAspectFit; smooth: false
                        }
                        RoBasicMenuIcon {
                            anchors.centerIn: parent; width: 24; height: 24
                            visible: skillIcon.status !== Image.Ready
                            icon: "skill"; ink: theme.disabledText
                        }
                        Text {
                            anchors.right: parent.right; anchors.bottom: parent.bottom
                            text: node.modelData.pending ? "+" : node.modelData.available ? "+" : node.modelData.locked ? "×" : ""
                            color: node.modelData.pending || node.modelData.jobBlocked ? theme.warning : theme.focus
                            font.family: theme.fontFamily; font.pixelSize: 13; font.bold: true
                        }
                    }
                    RoTextButton { x: panel.geometry.leftX; y: panel.geometry.arrowY; width: panel.geometry.arrowSize; height: panel.geometry.arrowSize; label: "‹"; labelSize: 16; enabled: node.modelData.level > 0 && !panel.tree.applying }
                    RoTextButton { x: panel.geometry.rightX; y: panel.geometry.arrowY; width: panel.geometry.arrowSize; height: panel.geometry.arrowSize; label: "›"; labelSize: 16; enabled: node.modelData.level > 0 && !panel.tree.applying }
                    Row {
                        x: 26; y: panel.geometry.arrowY + 3; width: 52; spacing: 3
                        Text {
                            width: 18; horizontalAlignment: Text.AlignRight
                            text: node.modelData.selected
                            color: node.modelData.selected < node.modelData.level ? theme.warning : "#000000"
                            font.family: theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold
                        }
                        Text { text: "/"; color: "#000000"; font.family: theme.fontFamily; font.pixelSize: 11 }
                        Text {
                            text: node.modelData.level
                            color: node.modelData.pending ? theme.warning : "#000000"
                            font.family: theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold
                        }
                    }
                    Text {
                        x: 4; y: 86; width: parent.width - 8
                        text: node.modelData.required > 0 ? (node.modelData.level + " / " + node.modelData.required + " required")
                            : node.modelData.jobBlocked ? "Earlier-job points first"
                            : node.modelData.pending ? "Unapplied" : node.modelData.available ? "Point available"
                            : node.modelData.passive ? "Passive" : node.modelData.locked ? "Locked" : ""
                        horizontalAlignment: Text.AlignHCenter
                        color: node.modelData.required > node.modelData.level ? theme.warning : theme.muted
                        font.family: theme.fontFamily; font.pixelSize: 8
                    }
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom; height: 3; radius: 1
                visible: (panel.tree.contentWidth || 0) > viewport.width
                width: Math.max(16, viewport.width * viewport.width / Math.max(1,panel.tree.contentWidth || 1))
                x: (viewport.width - width) * (panel.tree.scrollX || 0) / Math.max(1,(panel.tree.contentWidth || 0) - viewport.width)
                color: theme.scrollThumb
            }
            Rectangle {
                anchors.right: parent.right; width: 3; radius: 1
                visible: (panel.tree.contentHeight || 0) > viewport.height
                height: Math.max(16, viewport.height * viewport.height / Math.max(1,panel.tree.contentHeight || 1))
                y: (viewport.height - height) * (panel.tree.scrollY || 0) / Math.max(1,(panel.tree.contentHeight || 0) - viewport.height)
                color: theme.scrollThumb
            }
        }
        Text {
            x: 12; y: panel.height - 85; width: panel.width - 24; height: 46
            text: panel.tree.tooltip || panel.tree.status || "Click an icon to plan one point. Right-click for skill details."
            color: theme.muted
            font.family: theme.fontFamily; font.pixelSize: 10
            wrapMode: Text.Wrap; maximumLineCount: 3; elide: Text.ElideRight
        }
        Text {
            x: 12; y: panel.height - 26; width: Math.max(0,panel.width - 186)
            text: panel.tree.applying ? "Applying…" : (panel.tree.status || "Changes are not applied yet.")
            color: theme.ink
            font.family: theme.fontFamily; font.pixelSize: 10; elide: Text.ElideRight
        }
        RoTextButton { x: panel.width - 164; y: panel.height - 31; width: 76; height: 24; label: "Apply"; enabled: panel.tree.canApply || false }
        RoTextButton { x: panel.width - 84; y: panel.height - 31; width: 76; height: 24; label: "Reset"; enabled: panel.tree.canReset || false }
    }
}
