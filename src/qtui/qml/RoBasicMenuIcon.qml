import QtQuick 2.15

Item {
    id: glyph
    property string icon: ""
    property color ink: "#25313c"
    property color paper: "#fafaf8"
    readonly property real rasterScale: typeof uiState !== "undefined" ? Math.max(1, uiState.uiScale || 1) : 1
    readonly property var paths: ({
        close: '<path d="m4 4 8 8M12 4l-8 8"/>',
        minimize: '<path d="M3 9h10"/>',
        expand: '<path d="m3 5 5 5 5-5"/>',
        compact: '<rect x="2" y="2" width="4" height="4" rx=".5"/><rect x="10" y="2" width="4" height="4" rx=".5"/><rect x="2" y="10" width="4" height="4" rx=".5"/><rect x="10" y="10" width="4" height="4" rx=".5"/>',
        details: '<path d="M6 3h8M6 8h8M6 13h8M2 3h1M2 8h1M2 13h1"/>',
        status: '<rect x="1" y="2" width="14" height="12" rx="1.5"/><circle cx="5" cy="6" r="1.5"/><path d="M3 12v-1a2 2 0 0 1 4 0v1M10 5h3M10 8h3M10 11h2"/>',
        option: '<path d="m6.5 1-.4 1.8-1.5.9-1.8-.6-1.5 2.6 1.4 1.2v1.9l-1.4 1.3 1.5 2.6 1.8-.6 1.5.9.4 1.8h3l.4-1.8 1.5-.9 1.8.6 1.5-2.6-1.4-1.3V6.9l1.4-1.2-1.5-2.6-1.8.6-1.5-.9L9.5 1z"/><circle cx="8" cy="8" r="2.3"/>',
        items: '<path d="M5 3V1.5h6V3M3 14V6c0-4 10-4 10 0v8zM3 6l5 2 5-2"/><rect x="5" y="10" width="6" height="3" rx=".5"/>',
        equip: '<path d="m5 1.5 1.5 1.5h3L11 1.5 15 5l-2.5 3-1-1v7.5h-7V7l-1 1L1 5zM6 5l2 1 2-1M6 12h4"/>',
        skill: '<path d="m8 4-3-1-4 .3v10L5 13l3 1 3-1 4 .3v-10L11 3zM8 4v10M3 6l2.5.5M3 9l2.5.5M11.5 5.5v5M9.5 8h4"/>',
        map: '<path d="m1 3.5 4.5-2 4.5 2 5-2v11l-5 2-4.5-2-4.5 2zM5.5 2v10"/><path d="M7.5 7a2.5 2.5 0 0 1 5 0c0 1.5-2.5 4-2.5 4S7.5 8.5 7.5 7z" fill="PAPER"/><circle cx="10" cy="7" r=".6"/>',
        comm: '<path d="M5 5V1.5h10V9h-3M1 5h10v7H6l-4 3v-3H1zM3.5 8.5h.1M6 8.5h.1M8.5 8.5h.1"/>',
        friend: '<circle cx="5" cy="5" r="2"/><circle cx="11.5" cy="6" r="1.5"/><path d="M1 14v-2c0-4 8-4 8 0v2M11 10c3-.5 4 1.5 4 4M10 1h4v2"/>'
    })
    Image {
        anchors.fill: parent
        sourceSize: Qt.size(Math.ceil(width * glyph.rasterScale), Math.ceil(height * glyph.rasterScale))
        fillMode: Image.PreserveAspectFit
        smooth: true
        source: !glyph.paths[glyph.icon] ? "" : "data:image/svg+xml;utf8," + encodeURIComponent(
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="-1 -1 18 18"><g fill="none" stroke="' + glyph.ink + '" stroke-width="1.25" stroke-linecap="round" stroke-linejoin="round">' + glyph.paths[glyph.icon].replace('PAPER', glyph.paper) + '</g></svg>')
    }
}
