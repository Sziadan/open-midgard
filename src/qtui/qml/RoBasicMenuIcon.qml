import QtQuick 2.15

Canvas {
    id: canvas

    property string icon: ""
    property color ink: "#1c1914"
    property color paper: "#f7f1e6"

    onIconChanged: requestPaint()
    onInkChanged: requestPaint()
    onPaperChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        if (!icon || width < 4 || height < 4) {
            return
        }

        const span = Math.min(width, height) - 2
        const originX = (width - span) / 2
        const originY = (height - span) / 2
        function px(unit) { return originX + unit / 16 * span }
        function py(unit) { return originY + unit / 16 * span }
        function dot(unitX, unitY, radius) {
            ctx.beginPath()
            ctx.arc(px(unitX), py(unitY), radius, 0, Math.PI * 2)
            ctx.fill()
        }

        ctx.strokeStyle = ink
        ctx.fillStyle = ink
        ctx.lineWidth = Math.max(1.15, span / 14)
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        if (icon === "status") {
            dot(8, 4.6, span * 0.16)
            ctx.beginPath()
            ctx.moveTo(px(3.1), py(14.6))
            ctx.quadraticCurveTo(px(8), py(8.2), px(12.9), py(14.6))
            ctx.closePath()
            ctx.fill()
        } else if (icon === "option") {
            const rows = [4.1, 8, 11.9]
            const knobs = [10.6, 5.2, 8.4]
            for (let index = 0; index < rows.length; ++index) {
                ctx.beginPath()
                ctx.moveTo(px(2.3), py(rows[index]))
                ctx.lineTo(px(13.7), py(rows[index]))
                ctx.stroke()
                dot(knobs[index], rows[index], span * 0.13)
                ctx.fillStyle = paper
                dot(knobs[index], rows[index], span * 0.055)
                ctx.fillStyle = ink
            }
        } else if (icon === "items") {
            ctx.beginPath()
            ctx.moveTo(px(5.8), py(6.1))
            ctx.quadraticCurveTo(px(8), py(2.4), px(10.2), py(6.1))
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(px(4.2), py(6.4))
            ctx.lineTo(px(11.8), py(6.4))
            ctx.quadraticCurveTo(px(13.6), py(6.4), px(13.6), py(8.2))
            ctx.lineTo(px(13.6), py(12.6))
            ctx.quadraticCurveTo(px(13.6), py(14.4), px(11.8), py(14.4))
            ctx.lineTo(px(4.2), py(14.4))
            ctx.quadraticCurveTo(px(2.4), py(14.4), px(2.4), py(12.6))
            ctx.lineTo(px(2.4), py(8.2))
            ctx.quadraticCurveTo(px(2.4), py(6.4), px(4.2), py(6.4))
            ctx.closePath()
            ctx.fill()
            ctx.strokeStyle = paper
            ctx.beginPath()
            ctx.moveTo(px(3.2), py(9.1))
            ctx.lineTo(px(12.8), py(9.1))
            ctx.stroke()
        } else if (icon === "equip") {
            ctx.beginPath()
            ctx.moveTo(px(8), py(2.1))
            ctx.lineTo(px(13.3), py(4.7))
            ctx.lineTo(px(13.3), py(8.5))
            ctx.quadraticCurveTo(px(13.3), py(12.1), px(8), py(14.5))
            ctx.quadraticCurveTo(px(2.7), py(12.1), px(2.7), py(8.5))
            ctx.lineTo(px(2.7), py(4.7))
            ctx.closePath()
            ctx.fill()
            ctx.strokeStyle = paper
            ctx.beginPath()
            ctx.moveTo(px(8), py(5.2))
            ctx.lineTo(px(8), py(11.4))
            ctx.moveTo(px(5.4), py(7.4))
            ctx.lineTo(px(10.6), py(7.4))
            ctx.stroke()
        } else if (icon === "skill") {
            ctx.beginPath()
            ctx.moveTo(px(8), py(4.4))
            ctx.lineTo(px(4.1), py(11.4))
            ctx.moveTo(px(8), py(4.4))
            ctx.lineTo(px(11.9), py(11.4))
            ctx.moveTo(px(4.1), py(11.4))
            ctx.lineTo(px(11.9), py(11.4))
            ctx.stroke()
            dot(8, 4.2, span * 0.13)
            dot(4.1, 11.6, span * 0.13)
            dot(11.9, 11.6, span * 0.13)
            dot(8, 8.2, span * 0.1)
        } else if (icon === "map") {
            ctx.beginPath()
            ctx.moveTo(px(2.3), py(3.1))
            ctx.lineTo(px(10.1), py(3.1))
            ctx.lineTo(px(13.7), py(6.5))
            ctx.lineTo(px(13.7), py(13.7))
            ctx.lineTo(px(2.3), py(13.7))
            ctx.closePath()
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(px(10.1), py(3.1))
            ctx.lineTo(px(10.1), py(6.5))
            ctx.lineTo(px(13.7), py(6.5))
            ctx.stroke()
            dot(7.1, 8.3, span * 0.12)
            ctx.beginPath()
            ctx.moveTo(px(7.1), py(9.3))
            ctx.lineTo(px(7.1), py(12.2))
            ctx.stroke()
        } else if (icon === "comm") {
            ctx.beginPath()
            ctx.moveTo(px(4.2), py(2.4))
            ctx.lineTo(px(11.8), py(2.4))
            ctx.quadraticCurveTo(px(13.8), py(2.4), px(13.8), py(4.4))
            ctx.lineTo(px(13.8), py(8.2))
            ctx.quadraticCurveTo(px(13.8), py(10.2), px(11.8), py(10.2))
            ctx.lineTo(px(7.4), py(10.2))
            ctx.lineTo(px(3.6), py(13.6))
            ctx.lineTo(px(4.6), py(10.2))
            ctx.lineTo(px(4.2), py(10.2))
            ctx.quadraticCurveTo(px(2.2), py(10.2), px(2.2), py(8.2))
            ctx.lineTo(px(2.2), py(4.4))
            ctx.quadraticCurveTo(px(2.2), py(2.4), px(4.2), py(2.4))
            ctx.closePath()
            ctx.fill()
            ctx.fillStyle = paper
            dot(6.2, 6.3, span * 0.055)
            dot(8.1, 6.3, span * 0.055)
            dot(10, 6.3, span * 0.055)
        } else if (icon === "friend") {
            function person(centerX, headY) {
                dot(centerX, headY, span * 0.13)
                ctx.beginPath()
                ctx.moveTo(px(centerX - 2.7), py(14.6))
                ctx.quadraticCurveTo(px(centerX), py(headY + 2.4), px(centerX + 2.7), py(14.6))
                ctx.closePath()
                ctx.fill()
            }
            ctx.globalAlpha = 0.55
            person(10.4, 5.1)
            ctx.globalAlpha = 1
            person(6.2, 4.6)
        }
    }
}
