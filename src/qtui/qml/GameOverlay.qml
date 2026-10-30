import QtQuick 2.15

Item {
    id: root
    RoTheme { id: theme }
    width: parent ? parent.width : 1280
    height: parent ? parent.height : 720
    property bool loginCaretVisible: true
    property real uiScale: Math.max(0.5, uiState.uiScale || 1.0)

    function rgbaColor(red, green, blue, alpha) {
        return Qt.rgba(red / 255.0, green / 255.0, blue / 255.0, Math.max(0.0, Math.min(1.0, alpha)))
    }

    function chatChromeOpacity(chatUi) {
        const focused = !!(chatUi && chatUi.focused)
        const percent = focused
            ? ((chatUi && chatUi.windowOpacityPercent) || 84)
            : ((chatUi && chatUi.idleOpacityPercent) || 45)
        return Math.max(0.2, Math.min(1.0, percent / 100.0))
    }

    function clampValue(value, minValue, maxValue) {
        if (maxValue < minValue) {
            return minValue
        }
        return Math.max(minValue, Math.min(maxValue, value))
    }

    function clampOverlayX(x, overlayWidth) {
        return clampValue(x, 0, root.width - overlayWidth)
    }

    function clampOverlayY(y, overlayHeight) {
        return clampValue(y, 0, root.height - overlayHeight)
    }

    function itemIconSource(itemId, identified) {
        if (!(itemId > 0)) {
            return ""
        }
        let source = "image://openmidgard/item/" + itemId
        if (identified !== undefined) {
            source += "?identified=" + (identified ? "1" : "0")
        }
        return source
    }

    function collectionImageSource(itemId, identified) {
        if (!(itemId > 0)) {
            return ""
        }
        let source = "image://openmidgard/collection/" + itemId
        if (identified !== undefined) {
            source += "?identified=" + (identified ? "1" : "0")
        }
        return source
    }

    function illustImageSource(itemId) {
        return itemId > 0 ? "image://openmidgard/illust/" + itemId : ""
    }

    function isHexDigit(ch) {
        return (ch >= "0" && ch <= "9")
            || (ch >= "a" && ch <= "f")
            || (ch >= "A" && ch <= "F")
    }

    function escapeHtml(text) {
        return String(text)
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/\"/g, "&quot;")
            .replace(/'/g, "&#39;")
    }

    function roColorTextToRichText(text) {
        if (!text) {
            return ""
        }

        let out = "<span style=\"color:#1c1914\">"
        let currentColor = "2a261f"
        for (let index = 0; index < text.length; ++index) {
            const ch = text[index]
            if (ch === "^" && index + 6 < text.length) {
                let isColor = true
                for (let colorIndex = 1; colorIndex <= 6; ++colorIndex) {
                    if (!isHexDigit(text[index + colorIndex])) {
                        isColor = false
                        break
                    }
                }
                if (isColor) {
                    currentColor = text.substring(index + 1, index + 7)
                    out += "</span><span style=\"color:#" + currentColor + "\">"
                    index += 6
                    continue
                }
            }

            if (ch === "\r") {
                continue
            }
            if (ch === "\n") {
                out += "<br>"
                continue
            }

            out += escapeHtml(ch === "_" ? " " : ch)
        }

        out += "</span>"
        return out
    }

    function skillIconSource(skillId) {
        return skillId > 0 ? "image://openmidgard/skill/" + skillId : ""
    }

    function statusIconSource(statusType) {
        return statusType > 0 ? "image://openmidgard/status/" + statusType : ""
    }

    function formatStatusDuration(remainingMs) {
        var totalSeconds = Math.max(0, Math.ceil((remainingMs || 0) / 1000))
        var seconds = totalSeconds % 60
        var totalMinutes = Math.floor(totalSeconds / 60)
        var hours = Math.floor(totalMinutes / 60)
        var minutes = totalMinutes % 60

        if (hours > 0) {
            return hours + ":" + ("0" + minutes).slice(-2) + ":" + ("0" + seconds).slice(-2)
        }
        return totalMinutes + ":" + ("0" + seconds).slice(-2)
    }

    function statusTooltipText(statusData) {
        if (!statusData) {
            return ""
        }
        if (statusData.timed) {
            return (statusData.name || "") + "\n" + formatStatusDuration(statusData.remainingMs || 0) + " remaining"
        }
        return (statusData.name || "") + "\nActive"
    }

    function statusExpiryTintFraction(statusData) {
        if (!statusData || !statusData.timed) {
            return 0
        }
        var remainingMs = Math.max(0, statusData.remainingMs || 0)
        if (remainingMs >= 60000) {
            return 0
        }
        return 1 - (remainingMs / 60000)
    }

    function equipPreviewSource() {
        const revision = uiState.equipData.previewRevision || "0"
        return "image://openmidgard/equippreview?rev=" + revision
    }

    function makeCharPanelButtonsKey() {
        var buttons = uiState && uiState.makeCharButtons ? uiState.makeCharButtons : []
        var key = ""
        for (var i = 0; i < buttons.length; ++i) {
            key += (buttons[i].id || i) + ":" + (buttons[i].pressed ? "1" : "0") + ";"
        }
        return key
    }

    function charSelectPanelKey() {
        var details = uiState && uiState.charSelectSelectedDetails ? uiState.charSelectSelectedDetails : {}
        return details.imageRevision || "0"
    }

    Timer {
        interval: 500
        running: uiState.loginPanelVisible || uiState.makeCharVisible
        repeat: true
        onTriggered: root.loginCaretVisible = !root.loginCaretVisible
    }
    Image {
        anchors.fill: parent
        visible: source !== ""
        fillMode: Image.Stretch
        smooth: false
        cache: false
        source: (uiState.loginPanelVisible
            || uiState.serverSelectVisible
            || uiState.charSelectVisible
            || uiState.makeCharVisible
            || uiState.loadingVisible)
            ? "image://openmidgard/wallpaper?rev=" + encodeURIComponent(uiState.wallpaperRevision || "none")
            : ""
    }

    Repeater {
        model: uiState.anchors

        delegate: Item {
            required property var modelData
            property bool isPartyHpBar: modelData.kind === "partyHpBar"
            property int bubblePaddingX: modelData.showBubble === false ? 0 : (modelData.paddingX || 12)
            property int bubblePaddingY: modelData.showBubble === false ? 0 : (modelData.paddingY || 8)
            property int maxTextWidth: modelData.maxTextWidth || 0
            property bool wrapText: modelData.wrap === true
            property int textWidth: {
                const measured = Math.max(1, Math.ceil(label.contentWidth || label.implicitWidth || 1))
                return maxTextWidth > 0 ? Math.min(maxTextWidth, measured) : measured
            }
            property int anchorX: modelData.centerX !== undefined
                ? Math.round((modelData.centerX || 0) - width / 2)
                : (modelData.x || 0)
            property int anchorY: modelData.bottomY !== undefined
                ? Math.round((modelData.bottomY || 0) - height)
                : (modelData.y || 0)
            x: root.clampOverlayX(anchorX, width)
            y: root.clampOverlayY(anchorY, height)
            z: modelData.z || 2000
            width: isPartyHpBar ? (modelData.width || 60) : (textWidth + bubblePaddingX)
            height: isPartyHpBar ? (modelData.height || 5) : (Math.max(1, Math.ceil(label.implicitHeight)) + bubblePaddingY)

            Rectangle {
                visible: parent.isPartyHpBar
                x: 0
                y: 0
                width: parent.width
                height: parent.height
                color: "#14199b"

                Rectangle {
                    x: 1
                    y: 1
                    width: parent.width - 2
                    height: parent.height - 2
                    color: "#1e243a"
                }

                Rectangle {
                    x: 1
                    y: 1
                    width: {
                        const maxHp = Math.max(1, modelData.maxHp || 0)
                        const currentHp = Math.max(0, Math.min(modelData.currentHp || 0, maxHp))
                        return Math.max(0, Math.floor((parent.width - 2) * currentHp / maxHp))
                    }
                    height: parent.height - 2
                    color: (modelData.maxHp > 0 && (modelData.currentHp || 0) * 4 < (modelData.maxHp || 0)) ? "#ea1616" : "#16ea25"
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: modelData.radius || 6
                color: modelData.background
                visible: !parent.isPartyHpBar && modelData.showBubble !== false
                border.width: 1
                border.color: modelData.borderColor || "#80ffffff"
            }

            Text {
                id: label
                x: Math.floor(parent.bubblePaddingX / 2)
                y: Math.floor(parent.bubblePaddingY / 2)
                width: parent.textWidth
                text: modelData.text
                visible: !parent.isPartyHpBar
                color: modelData.foreground
                font.family: theme.fontFamily
                font.pixelSize: modelData.fontPixelSize || 14
                font.bold: modelData.bold !== false
                wrapMode: parent.wrapText ? Text.Wrap : Text.NoWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    RoFrame {
        showTitle: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 42
        width: Math.min(540, parent.width - 80)
        height: 92
        visible: uiState.loadingVisible

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 8

            Row {
                width: parent.width

                Text {
                    id: loadingTitle
                    text: uiState.loadingProgress > 0 ? "Entering World" : uiState.modeName
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 22
                    font.bold: true
                }

                Item {
                    width: Math.max(0, parent.width - loadingTitle.implicitWidth - loadingPercent.implicitWidth)
                    height: 1
                }

                Text {
                    id: loadingPercent
                    text: Math.round(uiState.loadingProgress * 100) + "%"
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 22
                    font.bold: true
                    visible: uiState.loadingProgress > 0
                }
            }

            Text {
                width: parent.width
                text: uiState.loadingMessage
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 16
                elide: Text.ElideRight
            }

            Rectangle {
                width: parent.width
                height: 20
                radius: theme.buttonRadius
                color: theme.scrollTrack
                border.width: 1
                border.color: theme.gold
                visible: uiState.loadingProgress > 0

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(0, (parent.width - 4) * uiState.loadingProgress)
                    height: parent.height - 4
                    radius: 3
                    color: theme.gold
                }
            }
        }
    }

    Item {
        id: uiLayer
        x: 0
        y: 0
        width: root.uiScale > 0 ? root.width / root.uiScale : root.width
        height: root.uiScale > 0 ? root.height / root.uiScale : root.height
        scale: root.uiScale
        transformOrigin: Item.TopLeft

    Repeater {
        model: uiState.notifications

        delegate: RoFrame {
            required property var modelData
            showTitle: false
            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height

            Rectangle {
                width: 3
                height: parent.height
                color: modelData.accent
            }

            Text {
                anchors.centerIn: parent
                text: modelData.title
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 18
                font.bold: true
            }
        }
    }

    RoFrame {
        showTitle: false
        x: uiState.npcMenuX
        y: uiState.npcMenuY
        width: uiState.npcMenuWidth
        height: uiState.npcMenuHeight
        visible: uiState.npcMenuVisible
        Column {
            x: 10
            y: 10
            width: parent.width - 20
            spacing: 0

            Repeater {
                model: uiState.npcMenuOptions

                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: parent.width
                    height: 18
                    color: index === uiState.npcMenuSelectedIndex
                        ? theme.rowSelected
                        : (index === uiState.npcMenuHoverIndex ? theme.rowHover : "transparent")
                    border.width: (index === uiState.npcMenuSelectedIndex || index === uiState.npcMenuHoverIndex) ? 1 : 0
                    border.color: theme.gold

                    Rectangle {
                        width: index === uiState.npcMenuSelectedIndex ? 3 : 0
                        height: parent.height
                        color: theme.gold
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 8
                        text: modelData
                        textFormat: Text.RichText
                        font.family: theme.fontFamily
                        font.pixelSize: 12
                        clip: true
                    }
                }
            }
        }

        Repeater {
            model: uiState.npcMenuButtons

            delegate: RoTextButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.npcMenuX
                y: (modelData.y || 0) - uiState.npcMenuY
                width: modelData.width || 0
                height: modelData.height || 0
                pressed: modelData.pressed || false
                label: modelData.label || ""
            }
        }
    }

    RoFrame {
        showTitle: false
        x: uiState.sayDialogX
        y: uiState.sayDialogY
        width: uiState.sayDialogWidth
        height: uiState.sayDialogHeight
        visible: uiState.sayDialogVisible
        Text {
            x: 10
            y: 10
            width: parent.width - 20
            height: uiState.sayDialogHasAction ? (parent.height - 10 - 10 - 22 - 8) : (parent.height - 20)
            text: uiState.sayDialogText
            textFormat: Text.RichText
            font.family: theme.fontFamily
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            verticalAlignment: Text.AlignTop
        }

        RoTextButton {
            x: (uiState.sayDialogActionButton.x || 0) - uiState.sayDialogX
            y: (uiState.sayDialogActionButton.y || 0) - uiState.sayDialogY
            width: uiState.sayDialogActionButton.width || 0
            height: uiState.sayDialogActionButton.height || 0
            visible: uiState.sayDialogActionButton.visible || false
            pressed: uiState.sayDialogActionButton.pressed || false
            hovered: uiState.sayDialogActionButton.hovered || false
            label: uiState.sayDialogActionButton.label || ""
        }
    }

    RoFrame {
        showTitle: false
        x: uiState.npcInputX
        y: uiState.npcInputY
        width: uiState.npcInputWidth
        height: uiState.npcInputHeight
        visible: uiState.npcInputVisible
        Text {
            x: 10
            y: 9
            width: parent.width - 20
            text: uiState.npcInputLabel
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 12
        }

        RoField {
            x: 10
            y: 26
            width: parent.width - 20
            height: 22
            editing: true

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 12
                text: uiState.npcInputText
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        Repeater {
            model: uiState.npcInputButtons

            delegate: RoTextButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.npcInputX
                y: (modelData.y || 0) - uiState.npcInputY
                width: modelData.width || 0
                height: modelData.height || 0
                pressed: modelData.pressed || false
                label: modelData.label || ""
            }
        }
    }

    RoFrame {
        showTitle: false
        x: uiState.chooseMenuX
        y: uiState.chooseMenuY
        width: uiState.chooseMenuWidth
        height: uiState.chooseMenuHeight
        visible: uiState.chooseMenuVisible
        Repeater {
            model: uiState.chooseMenuOptions

            delegate: Rectangle {
                required property int index
                required property var modelData
                x: (parent.width - 221) / 2
                y: 12 + index * 23
                width: 221
                height: 20
                radius: theme.buttonRadius
                color: index === uiState.chooseMenuPressedIndex
                    ? theme.buttonPressed
                    : (index === uiState.chooseMenuSelectedIndex ? theme.rowSelected : theme.button)
                border.width: 1
                border.color: index === uiState.chooseMenuSelectedIndex ? theme.gold : theme.buttonBorder

                Text {
                    anchors.centerIn: parent
                    text: modelData
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 12
                }
            }
        }
    }

    RoFrame {
        x: uiState.itemShopX
        y: uiState.itemShopY
        width: uiState.itemShopWidth
        height: uiState.itemShopHeight
        visible: uiState.itemShopVisible
        Text {
            x: 6
            y: 1
            text: uiState.itemShopData.title || uiState.itemShopTitle
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        Rectangle {
            x: 8
            y: 22
            width: parent.width - 16
            height: parent.height - 34
            color: theme.fieldFill
            border.width: 1
            border.color: theme.buttonBorder

            Column {
                x: 1
                y: 1
                width: parent.width - 2
                spacing: 0

                Rectangle {
                    width: parent.width
                    height: 17
                    color: theme.rowAlt

                    Text {
                        x: 26
                        y: 2
                        width: (uiState.itemShopData.showQuantity || false) ? 160 : (parent.width - 110)
                        text: uiState.itemShopData.nameLabel || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 11
                    }

                    Text {
                        x: 190
                        y: 2
                        width: 38
                        visible: uiState.itemShopData.showQuantity || false
                        text: uiState.itemShopData.quantityLabel || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        x: width - 60
                        y: 2
                        width: 54
                        text: uiState.itemShopData.priceLabel || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Repeater {
                    model: uiState.itemShopRows

                    delegate: Rectangle {
                        required property var modelData
                        width: parent.width
                        height: 18
                        color: modelData.selected ? theme.rowSelected : (modelData.hover ? theme.rowHover : "transparent")

                        Rectangle {
                            width: modelData.selected ? 3 : 0
                            height: parent.height
                            color: theme.gold
                        }

                        Text {
                            x: 24
                            y: 2
                            width: (uiState.itemShopData.showQuantity || false) ? 160 : (parent.width - 110)
                            text: modelData.name
                            color: theme.ink
                            font.family: theme.fontFamily
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        Text {
                            x: 188
                            y: 2
                            width: 38
                            visible: uiState.itemShopData.showQuantity || false
                            text: modelData.quantity
                            color: theme.ink
                            font.family: theme.fontFamily
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            x: parent.width - 76
                            y: 2
                            width: 70
                            text: modelData.price
                            color: theme.titleBottom
                            font.family: theme.fontFamily
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }
        }
    }

    RoFrame {
        x: uiState.itemPurchaseX
        y: uiState.itemPurchaseY
        width: uiState.itemPurchaseWidth
        height: uiState.itemPurchaseHeight
        visible: uiState.itemPurchaseVisible
        Text {
            x: 6
            y: 1
            text: uiState.itemPurchaseData.title || ""
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        Rectangle {
            x: 8
            y: 22
            width: parent.width - 16
            height: parent.height - 80
            color: theme.fieldFill
            border.width: 1
            border.color: theme.buttonBorder

            Column {
                x: 1
                y: 1
                width: parent.width - 2
                spacing: 0

                Rectangle {
                    width: parent.width
                    height: 17
                    color: theme.rowAlt

                    Text {
                        x: 4
                        y: 2
                        text: uiState.itemPurchaseData.nameLabel || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 11
                    }

                    Text {
                        x: 126
                        y: 2
                        width: 34
                        text: uiState.itemPurchaseData.quantityLabel || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        x: width - 64
                        y: 2
                        width: 56
                        text: uiState.itemPurchaseData.amountLabel || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Repeater {
                    model: uiState.itemPurchaseRows

                    delegate: Rectangle {
                        required property var modelData
                        width: parent.width
                        height: 18
                        color: modelData.selected ? theme.rowSelected : (modelData.hover ? theme.rowHover : "transparent")

                        Rectangle {
                            width: modelData.selected ? 3 : 0
                            height: parent.height
                            color: theme.gold
                        }

                        Text {
                            x: 4
                            y: 2
                            width: 126
                            text: modelData.name
                            color: theme.ink
                            font.family: theme.fontFamily
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        Text {
                            x: 126
                            y: 2
                            width: 34
                            text: modelData.quantity
                            color: theme.ink
                            font.family: theme.fontFamily
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            x: parent.width - 64
                            y: 2
                            width: 56
                            text: modelData.cost
                            color: theme.titleBottom
                            font.family: theme.fontFamily
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }
        }

        Text {
            x: 10
            y: height - 75
            text: uiState.itemPurchaseData.totalLabel || ""
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 11
        }

        Text {
            x: 90
            y: height - 75
            width: 138
            text: uiState.itemPurchaseTotal
            color: theme.titleBottom
            font.family: theme.fontFamily
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
        }

        Repeater {
            model: uiState.itemPurchaseButtons

            delegate: RoTextButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.itemPurchaseX
                y: (modelData.y || 0) - uiState.itemPurchaseY
                width: modelData.width || 0
                height: modelData.height || 0
                pressed: modelData.pressed || false
                hovered: modelData.hot || false
                label: modelData.label || ""
                labelSize: 12
                labelBold: false
            }
        }
    }

    RoFrame {
        x: uiState.itemSellX
        y: uiState.itemSellY
        width: uiState.itemSellWidth
        height: uiState.itemSellHeight
        visible: uiState.itemSellVisible
        Text {
            x: 6
            y: 1
            text: uiState.itemSellData.title || ""
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        Rectangle {
            x: 8
            y: 22
            width: parent.width - 16
            height: parent.height - 80
            color: theme.fieldFill
            border.width: 1
            border.color: theme.buttonBorder

            Column {
                x: 1
                y: 1
                width: parent.width - 2
                spacing: 0

                Rectangle {
                    width: parent.width
                    height: 17
                    color: theme.rowAlt

                    Text {
                        x: 4
                        y: 2
                        text: uiState.itemSellData.nameLabel || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 11
                    }

                    Text {
                        x: 126
                        y: 2
                        width: 34
                        text: uiState.itemSellData.quantityLabel || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        x: width - 64
                        y: 2
                        width: 56
                        text: uiState.itemSellData.amountLabel || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Repeater {
                    model: uiState.itemSellRows

                    delegate: Rectangle {
                        required property var modelData
                        width: parent.width
                        height: 18
                        color: modelData.selected ? theme.rowSelected : (modelData.hover ? theme.rowHover : "transparent")

                        Rectangle {
                            width: modelData.selected ? 3 : 0
                            height: parent.height
                            color: theme.gold
                        }

                        Text {
                            x: 4
                            y: 2
                            width: 126
                            text: modelData.name
                            color: theme.ink
                            font.family: theme.fontFamily
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        Text {
                            x: 126
                            y: 2
                            width: 34
                            text: modelData.quantity
                            color: theme.ink
                            font.family: theme.fontFamily
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            x: parent.width - 64
                            y: 2
                            width: 56
                            text: modelData.gain
                            color: theme.titleBottom
                            font.family: theme.fontFamily
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }
        }

        Text {
            x: 10
            y: height - 75
            text: uiState.itemSellData.totalLabel || ""
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 11
        }

        Text {
            x: 90
            y: height - 75
            width: 138
            text: uiState.itemSellTotal
            color: theme.titleBottom
            font.family: theme.fontFamily
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
        }

        Repeater {
            model: uiState.itemSellButtons

            delegate: RoTextButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.itemSellX
                y: (modelData.y || 0) - uiState.itemSellY
                width: modelData.width || 0
                height: modelData.height || 0
                pressed: modelData.pressed || false
                hovered: modelData.hot || false
                label: modelData.label || ""
                labelSize: 12
                labelBold: false
            }
        }
    }

    RoFrame {
        showTitle: false
        x: uiState.shortCutX
        y: uiState.shortCutY
        width: uiState.shortCutWidth
        height: uiState.shortCutHeight
        visible: uiState.shortCutVisible
        Repeater {
            model: uiState.shortCutSlots

            delegate: Rectangle {
                required property int index
                required property var modelData
                x: 5 + index * 29
                y: 4
                width: 24
                height: 24
                color: modelData.hover ? theme.rowHover : theme.fieldFill
                border.width: 1
                border.color: modelData.hover ? theme.gold : (modelData.isSkill ? theme.titleBottom : theme.buttonBorder)
                radius: theme.buttonRadius

                Image {
                    id: shortCutIcon
                    anchors.fill: parent
                    anchors.margins: 1
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                    source: modelData.isSkill
                        ? root.skillIconSource(modelData.skillId || 0)
                        : root.itemIconSource(modelData.itemId || 0)
                    visible: modelData.occupied && source !== ""
                }

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 4
                    text: modelData.occupied ? (modelData.isSkill ? "S" : "I") : ""
                    visible: !shortCutIcon.visible
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                    font.bold: true
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 2
                    anchors.bottom: parent.bottom
                    text: modelData.count > 0 ? modelData.count : ""
                    color: "#ffffff"
                    font.family: theme.fontFamily
                    font.pixelSize: 9
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#000000"
                }
            }
        }

        Text {
            x: width - 25
            y: 16
            width: 12
            text: uiState.shortCutPage
            color: "#ffffff"
            font.family: theme.fontFamily
            font.pixelSize: 11
            font.bold: true
            style: Text.Outline
            styleColor: "#000000"
            horizontalAlignment: Text.AlignRight
        }
    }

    RoFrame {
        x: uiState.basicInfoX
        y: uiState.basicInfoY
        width: uiState.basicInfoWidth
        height: uiState.basicInfoHeight
        visible: uiState.basicInfoVisible
        Text {
            x: uiState.basicInfoMini ? 9 : 17
            y: 3
            text: uiState.basicInfoMini ? (uiState.basicInfoData.name || "") : "Basic Info"
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        Repeater {
            model: uiState.basicInfoData.systemButtons || []

            delegate: RoChromeButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.basicInfoX
                y: (modelData.y || 0) - uiState.basicInfoY
                width: modelData.width || 0
                height: modelData.height || 0
                visible: modelData.visible || false
                label: modelData.label || ""
            }
        }

        Text {
            x: 17
            y: 18
            visible: uiState.basicInfoMini
            text: uiState.basicInfoData.miniHeaderText || ""
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 11
        }

        Text {
            x: 17
            y: 33
            visible: uiState.basicInfoMini
            text: uiState.basicInfoData.miniStatusText || ""
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 11
        }

        Text {
            x: 8
            y: 22
            width: 76
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.name || ""
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 13
            font.bold: true
            elide: Text.ElideRight
        }

        Text {
            x: 8
            y: 37
            width: 76
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.jobName || ""
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        Rectangle {
            x: 86
            y: 20
            width: 117
            height: 16
            visible: !uiState.basicInfoMini
            radius: 3
            clip: true
            color: "#2e2924"
            border.width: 1
            border.color: "#6d6154"

            Rectangle {
                width: Math.max(0, parent.width * ((uiState.basicInfoData.maxHp || 0) > 0 ? (uiState.basicInfoData.hp || 0) / uiState.basicInfoData.maxHp : 0))
                height: parent.height
                color: ((uiState.basicInfoData.maxHp || 0) > 0 && (uiState.basicInfoData.hp || 0) * 100 < uiState.basicInfoData.maxHp * 25) ? "#c43434" : "#e15b55"
            }

            Rectangle {
                width: parent.width
                height: 5
                color: "#18ffffff"
            }

            Text {
                x: 6
                anchors.verticalCenter: parent.verticalCenter
                text: "HP"
                color: "#fff6ec"
                font.family: theme.fontFamily
                font.pixelSize: 10
                font.bold: true
                style: Text.Outline
                styleColor: "#1a120e"
            }

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 26
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: (uiState.basicInfoData.hp || 0) + " / " + (uiState.basicInfoData.maxHp || 0)
                color: "#fff8f2"
                font.family: theme.fontFamily
                font.pixelSize: 11
                font.bold: true
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
                style: Text.Outline
                styleColor: "#1a120e"
            }
        }

        Rectangle {
            x: 86
            y: 44
            width: 117
            height: 16
            visible: !uiState.basicInfoMini
            radius: 3
            clip: true
            color: "#2e2924"
            border.width: 1
            border.color: "#6d6154"

            Rectangle {
                width: Math.max(0, parent.width * ((uiState.basicInfoData.maxSp || 0) > 0 ? (uiState.basicInfoData.sp || 0) / uiState.basicInfoData.maxSp : 0))
                height: parent.height
                color: "#4d86e0"
            }

            Rectangle {
                width: parent.width
                height: 5
                color: "#18ffffff"
            }

            Text {
                x: 6
                anchors.verticalCenter: parent.verticalCenter
                text: "SP"
                color: "#f4f8ff"
                font.family: theme.fontFamily
                font.pixelSize: 10
                font.bold: true
                style: Text.Outline
                styleColor: "#101820"
            }

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 26
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: (uiState.basicInfoData.sp || 0) + " / " + (uiState.basicInfoData.maxSp || 0)
                color: "#f7fbff"
                font.family: theme.fontFamily
                font.pixelSize: 11
                font.bold: true
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
                style: Text.Outline
                styleColor: "#101820"
            }
        }

        Text {
            x: 17
            y: 72
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.baseLevelText || ""
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 11
        }

        Rectangle {
            x: 84
            y: 77
            width: 102
            height: 6
            visible: !uiState.basicInfoMini
            color: theme.fieldFill
            border.width: 1
            border.color: theme.buttonBorder

            Rectangle {
                x: 1
                y: 1
                width: Math.max(0, (parent.width - 2) * ((uiState.basicInfoData.expPercent || 0) / 100.0))
                height: parent.height - 2
                color: "#4fd17f"
            }
        }

        Text {
            x: 17
            y: 84
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.jobLevelText || ""
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 11
        }

        Rectangle {
            x: 84
            y: 87
            width: 102
            height: 6
            visible: !uiState.basicInfoMini
            color: theme.fieldFill
            border.width: 1
            border.color: theme.buttonBorder

            Rectangle {
                x: 1
                y: 1
                width: Math.max(0, (parent.width - 2) * ((uiState.basicInfoData.jobExpPercent || 0) / 100.0))
                height: parent.height - 2
                color: "#6aa8ff"
            }
        }

        Text {
            x: 5
            y: 103
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.weightText || ""
            color: ((uiState.basicInfoData.maxWeight || 0) > 0 && (uiState.basicInfoData.weight || 0) * 100 >= uiState.basicInfoData.maxWeight * 50) ? "#c44848" : theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 11
        }

        Text {
            x: 107
            y: 103
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.moneyText || ""
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 11
        }

        Repeater {
            model: uiState.basicInfoData.menuButtons || []

            delegate: RoChromeButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.basicInfoX
                y: (modelData.y || 0) - uiState.basicInfoY
                width: modelData.width || 0
                height: modelData.height || 0
                visible: modelData.visible || false
                label: modelData.label || ""
            }
        }
    }

    RoFrame {
        x: uiState.statusX
        y: uiState.statusY
        width: uiState.statusWidth
        height: uiState.statusHeight
        visible: uiState.statusVisible
        Text {
            x: 17
            y: 3
            text: uiState.statusData.title || ""
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        Repeater {
            model: uiState.statusData.systemButtons || []

            delegate: RoChromeButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.statusX
                y: (modelData.y || 0) - uiState.statusY
                width: modelData.width || 0
                height: modelData.height || 0
                visible: modelData.visible || false
                label: modelData.label || ""
            }
        }

        Text {
            x: 96
            y: 3
            visible: uiState.statusMini
            text: uiState.statusData.miniPointsText || ""
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: 11
        }

        Rectangle {
            x: 0
            y: 17
            width: parent.width
            height: parent.height - 17
            color: "transparent"
            border.width: 0
            visible: !uiState.statusMini

            Rectangle {
                x: 0
                y: 0
                width: 20
                height: parent.height
                color: theme.rowAlt
            }

            Repeater {
                model: uiState.statusData.pageTabs || []

                delegate: Rectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.statusX
                    y: (modelData.y || 0) - uiState.statusY - 17
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: modelData.active ? theme.button : theme.rowAlt
                    border.width: 1
                    border.color: modelData.active ? theme.gold : theme.buttonBorder
                    visible: modelData.visible || false

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 10
                        font.bold: modelData.active || false
                    }
                }
            }

            Column {
                x: 28
                y: 5
                spacing: 2
                visible: uiState.statusPage === 0

                Repeater {
                    model: uiState.statusData.stats || []

                    delegate: Item {
                        required property var modelData
                        width: 90
                        height: 14

                        Text {
                            x: 0
                            y: 0
                            text: modelData.label
                            color: theme.ink
                            font.family: theme.fontFamily
                            font.pixelSize: 11
                            font.bold: true
                        }

                        Text {
                            x: 26
                            y: 0
                            width: 34
                            text: modelData.value
                            color: theme.ink
                            font.family: theme.fontFamily
                            font.pixelSize: 11
                        }

                        RoChromeButton {
                            x: 64
                            y: 1
                            width: modelData.increaseWidth || 0
                            height: modelData.increaseHeight || 0
                            visible: modelData.canIncrease
                            label: modelData.increaseLabel || ""
                            labelSize: 8
                            labelBold: true
                        }

                        Text {
                            x: 78
                            y: 0
                            width: 10
                            text: modelData.cost > 0 ? modelData.cost : ""
                            color: theme.muted
                            font.family: theme.fontFamily
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }

            Text {
                x: 132
                y: 10
                visible: uiState.statusPage === 0
                text: "Atk"
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
            }

            Text {
                x: 152
                y: 10
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.attackText || ""
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 132
                y: 26
                visible: uiState.statusPage === 0
                text: "Matk"
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
            }

            Text {
                x: 152
                y: 26
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.matkText || ""
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 132
                y: 42
                visible: uiState.statusPage === 0
                text: "Hit"
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
            }

            Text {
                x: 152
                y: 42
                width: 40
                visible: uiState.statusPage === 0
                text: (uiState.statusData.hit || 0).toString()
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 132
                y: 58
                visible: uiState.statusPage === 0
                text: "Crit"
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
            }

            Text {
                x: 152
                y: 58
                width: 40
                visible: uiState.statusPage === 0
                text: (uiState.statusData.critical || 0).toString()
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 204
                y: 74
                visible: uiState.statusPage === 0
                text: "Pts"
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
                font.bold: true
            }

            Text {
                x: 243
                y: 74
                width: 30
                visible: uiState.statusPage === 0
                text: (uiState.statusData.statusPoint || 0).toString()
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
                font.bold: true
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 204
                y: 10
                visible: uiState.statusPage === 0
                text: "Def"
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
            }

            Text {
                x: 233
                y: 10
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.itemDefText || ""
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 204
                y: 26
                visible: uiState.statusPage === 0
                text: "Mdef"
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
            }

            Text {
                x: 233
                y: 26
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.itemMdefText || ""
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 204
                y: 42
                visible: uiState.statusPage === 0
                text: "Flee"
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
            }

            Text {
                x: 233
                y: 42
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.fleeText || ""
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 204
                y: 58
                visible: uiState.statusPage === 0
                text: "Aspd"
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
            }

            Text {
                x: 233
                y: 58
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.aspdText || ""
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    RoFrame {
        showTitle: false
        showShadow: false
        id: chatWindowChrome
        x: uiState.chatWindowX
        y: uiState.chatWindowY
        width: uiState.chatWindowWidth
        height: uiState.chatWindowHeight
        visible: uiState.chatWindowVisible
        readonly property var chatUi: uiState.chatWindowUi || ({})
        readonly property var scrollBar: uiState.chatWindowScrollBar || ({})
        readonly property bool scrollBarVisible: scrollBar.visible || false
        readonly property int scrollBarWidth: scrollBarVisible ? 8 : 0
        readonly property int scrollBarGap: scrollBarVisible ? 4 : 0
        readonly property int chatFontSize: chatUi.fontPixelSize || 13
        opacity: root.chatChromeOpacity(chatUi)
        Behavior on opacity {
            NumberAnimation {
                duration: 160
                easing.type: Easing.InOutQuad
            }
        }
        Rectangle {
            x: 1
            y: 6
            width: parent.width - 2
            height: 32
            radius: 7
            color: theme.titleTop
            border.width: 0
        }

        Rectangle {
            x: 8
            y: 40
            width: parent.width - 16
            height: 1
            color: theme.gold
        }

        Row {
            x: 10
            y: 12
            height: 22
            width: parent.width - 44
            spacing: 6

            Repeater {
                model: parent.parent.chatUi.tabs || []

                delegate: Rectangle {
                    required property var modelData
                    width: Math.max(68, Math.floor((parent.width - ((parent.spacing || 0) * Math.max(0, ((parent.parent.chatUi.tabs || []).length || 1) - 1))) / Math.max(1, ((parent.parent.chatUi.tabs || []).length || 1))))
                    height: 22
                    radius: theme.buttonRadius
                    color: modelData.active ? theme.button : theme.titleBottom
                    border.width: 1
                    border.color: modelData.active ? theme.gold : theme.buttonBorder

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label
                        color: modelData.active ? theme.ink : theme.titleText
                        font.family: theme.fontFamily
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }
        }

        RoChromeButton {
            x: parent.width - 28
            y: 14
            width: 18
            height: 18
            pressed: parent.chatUi.configVisible || false
            label: "\u2699"
            labelSize: 12
            labelBold: false
            labelFont: "Segoe UI Symbol"
        }

        Rectangle {
            x: 10
            y: 46
            width: parent.width - 20
            height: parent.height - 96
            radius: theme.buttonRadius
            color: "#14110e"
            border.width: 1
            border.color: theme.gold
            clip: true

            readonly property int chatTextWidth: width - 10 - parent.scrollBarWidth - parent.scrollBarGap

            Column {
                id: chatLinesColumn
                x: 5
                y: Math.max(6, parent.height - height - 6)
                width: parent.chatTextWidth
                spacing: 2

                Repeater {
                    model: uiState.chatWindowLines

                    delegate: Text {
                        required property var modelData
                        width: chatLinesColumn.width
                        text: modelData.text
                        textFormat: Text.PlainText
                        color: modelData.color
                        font.family: "Segoe UI"
                        font.pixelSize: parent.parent.parent.chatFontSize
                        wrapMode: Text.WordWrap
                    }
                }
            }

            RoScrollBar {
                readonly property int totalLines: Math.max(1, parent.parent.scrollBar.totalLines || 0)
                readonly property int visibleLineCount: Math.max(1, parent.parent.scrollBar.visibleLineCount || 0)
                readonly property int firstVisibleLine: Math.max(0, parent.parent.scrollBar.firstVisibleLine || 0)
                readonly property int scrollDenominator: Math.max(1, totalLines - visibleLineCount)
                x: parent.width - 5 - width
                y: 5
                width: chatWindowChrome.scrollBarWidth
                height: parent.height - 10
                visible: chatWindowChrome.scrollBarVisible
                thumbWidth: Math.max(4, width - 2)
                thumbHeight: Math.max(18, Math.round(height * visibleLineCount / totalLines))
                thumbX: 1
                thumbY: Math.round(Math.max(0, height - thumbHeight) * firstVisibleLine / scrollDenominator)
            }
        }

        RoField {
            x: 10
            y: parent.height - 38
            width: 126
            height: 28
            emphasize: true
            editing: uiState.chatWindowWhisperInputActive

            Text {
                x: 8
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 16
                text: {
                    const baseText = uiState.chatWindowWhisperTargetText.length > 0 || uiState.chatWindowWhisperInputActive
                        ? uiState.chatWindowWhisperTargetText
                        : "To"
                    return baseText + (uiState.chatWindowWhisperInputActive ? "_" : "")
                }
                color: uiState.chatWindowWhisperInputActive ? theme.ink : theme.muted
                font.family: "Segoe UI"
                font.pixelSize: 13
                elide: Text.ElideLeft
            }
        }

        RoField {
            x: 144
            y: parent.height - 38
            width: parent.width - 154
            height: 28
            emphasize: true
            editing: uiState.chatWindowMessageInputActive

            Text {
                x: 8
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 16
                text: uiState.chatWindowInputText + (uiState.chatWindowMessageInputActive ? "_" : "")
                color: uiState.chatWindowMessageInputActive ? theme.ink : theme.muted
                font.family: "Segoe UI"
                font.pixelSize: 13
                elide: Text.ElideLeft
            }
        }
    }

    RoFrame {
        showTitle: false
        visible: uiState.chatWindowVisible && (uiState.chatWindowUi.configVisible || false)
        x: uiState.chatWindowUi.configX || 0
        y: uiState.chatWindowUi.configY || 0
        width: uiState.chatWindowUi.configWidth || 0
        height: uiState.chatWindowUi.configHeight || 0
        Rectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 42
            radius: 7
            color: theme.titleBottom
        }

        Rectangle {
            x: 8
            y: 42
            width: parent.width - 16
            height: 1
            color: theme.gold
        }

        Text {
            x: 14
            y: 12
            text: "Chat Settings"
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: 17
            font.bold: true
        }

        Text {
            x: 14
            y: 50
            text: "Tabs"
            color: theme.ink
            font.family: "Segoe UI"
            font.pixelSize: 12
            font.bold: true
        }

        Column {
            x: 12
            y: 72
            width: 92
            spacing: 6

            Repeater {
                model: uiState.chatWindowUi.tabs || []

                delegate: Rectangle {
                    required property var modelData
                    width: parent.width
                    height: 28
                    radius: theme.buttonRadius
                    color: modelData.active ? theme.rowSelected : theme.button
                    border.width: 1
                    border.color: theme.buttonBorder

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label
                        color: theme.ink
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        font.bold: modelData.active
                    }
                }
            }
        }

        Text {
            x: 120
            y: 50
            text: "Font"
            color: theme.ink
            font.family: "Segoe UI"
            font.pixelSize: 12
            font.bold: true
        }

        Rectangle {
            x: 120
            y: 72
            width: parent.width - 132
            height: 28
            radius: 8
            color: theme.button
            border.width: 1
            border.color: theme.buttonBorder
        }

        Text {
            x: 132
            y: 79
            text: "Size " + (uiState.chatWindowUi.fontPixelSize || 13)
            color: theme.ink
            font.family: "Segoe UI"
            font.pixelSize: 12
            font.bold: true
        }

        RoTextButton {
            x: parent.width - 76
            y: 72
            width: 28
            height: 28
            label: "-"
            labelSize: 16
            labelBold: true
        }

        RoTextButton {
            x: parent.width - 40
            y: 72
            width: 28
            height: 28
            label: "+"
            labelSize: 16
            labelBold: true
        }

        Text {
            x: 120
            y: 112
            text: "In focus"
            color: theme.ink
            font.family: "Segoe UI"
            font.pixelSize: 12
            font.bold: true
        }

        Rectangle {
            x: 120
            y: 128
            width: parent.width - 132
            height: 28
            radius: 8
            color: theme.button
            border.width: 1
            border.color: theme.buttonBorder
        }

        Text {
            x: 132
            y: 135
            text: (uiState.chatWindowUi.windowOpacityPercent || 84) + "%"
            color: theme.ink
            font.family: "Segoe UI"
            font.pixelSize: 12
            font.bold: true
        }

        Rectangle {
            x: 130
            y: 168
            width: parent.width - 152
            height: 8
            radius: 4
            color: theme.scrollTrack
            border.width: 0

            Rectangle {
                width: Math.max(8, parent.width * (((uiState.chatWindowUi.windowOpacityPercent || 84) - 20) / 80))
                height: parent.height
                radius: 4
                color: theme.gold
            }

            Rectangle {
                x: Math.max(0, Math.min(parent.width - 14, parent.width * (((uiState.chatWindowUi.windowOpacityPercent || 84) - 20) / 80) - 7))
                y: -5
                width: 14
                height: 18
                radius: 7
                color: theme.button
                border.width: 1
                border.color: theme.gold
            }
        }

        Text {
            x: 120
            y: 196
            text: "Not in focus"
            color: theme.ink
            font.family: "Segoe UI"
            font.pixelSize: 12
            font.bold: true
        }

        Rectangle {
            x: 120
            y: 212
            width: parent.width - 132
            height: 28
            radius: 8
            color: theme.button
            border.width: 1
            border.color: theme.buttonBorder
        }

        Text {
            x: 132
            y: 219
            text: (uiState.chatWindowUi.idleOpacityPercent || 45) + "%"
            color: theme.ink
            font.family: "Segoe UI"
            font.pixelSize: 12
            font.bold: true
        }

        Rectangle {
            x: 130
            y: 252
            width: parent.width - 152
            height: 8
            radius: 4
            color: theme.scrollTrack
            border.width: 0

            Rectangle {
                width: Math.max(8, parent.width * (((uiState.chatWindowUi.idleOpacityPercent || 45) - 20) / 80))
                height: parent.height
                radius: 4
                color: theme.gold
            }

            Rectangle {
                x: Math.max(0, Math.min(parent.width - 14, parent.width * (((uiState.chatWindowUi.idleOpacityPercent || 45) - 20) / 80) - 7))
                y: -5
                width: 14
                height: 18
                radius: 7
                color: theme.button
                border.width: 1
                border.color: theme.gold
            }
        }

        Text {
            x: 120
            y: 276
            text: "Message Types"
            color: theme.ink
            font.family: "Segoe UI"
            font.pixelSize: 12
            font.bold: true
        }

        Grid {
            x: 120
            y: 300
            width: parent.width - 132
            columns: 2
            rowSpacing: 8
            columnSpacing: 8

            Repeater {
                model: uiState.chatWindowUi.filters || []

                delegate: RoTextButton {
                    required property var modelData
                    width: Math.floor((parent.width - 8) / 2)
                    height: 28
                    enabled: modelData.enabled || false
                    label: modelData.label
                    labelSize: 12
                    labelBold: true
                }
            }
        }

        RoTextButton {
            x: parent.width - 116
            y: parent.height - 40
            width: 104
            height: 28
            label: "Reset Tab"
            labelSize: 12
            labelBold: true
        }
    }

    RoFrame {
        showTitle: false
        showShadow: false
        x: uiState.rechargeGaugeX
        y: uiState.rechargeGaugeY
        width: uiState.rechargeGaugeWidth
        height: uiState.rechargeGaugeHeight
        visible: uiState.rechargeGaugeVisible
        Rectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: parent.height - 2
            radius: 2
            color: theme.fieldFill
        }

        Rectangle {
            x: 1
            y: 1
            width: Math.max(0, (parent.width - 2) * ((uiState.rechargeGaugeTotal || 0) > 0
                ? (uiState.rechargeGaugeAmount || 0) / uiState.rechargeGaugeTotal : 0))
            height: parent.height - 2
            radius: 2
            color: theme.gold
        }
    }

    RoFrame {
        x: uiState.minimapX
        y: uiState.minimapY
        width: uiState.minimapWidth
        height: uiState.minimapHeight
        visible: uiState.minimapVisible
        Rectangle {
            id: minimapMapFrame
            x: (uiState.minimapData.mapX || 0) - parent.x
            y: (uiState.minimapData.mapY || 0) - parent.y
            width: uiState.minimapData.mapWidth || 0
            height: uiState.minimapData.mapHeight || 0
            color: "#12161d"
            border.width: 1
            border.color: "#485060"
            clip: true

            Image {
                anchors.fill: parent
                fillMode: Image.Stretch
                smooth: false
                cache: false
                source: parent.visible ? ("image://openmidgard/minimap?rev=" + (uiState.minimapData.imageRevision || 0)) : ""
            }

            Repeater {
                model: uiState.minimapData.markers || []

                delegate: Rectangle {
                    required property var modelData
                    x: modelData.x - (uiState.minimapData.mapX || 0) - modelData.radius
                    y: modelData.y - (uiState.minimapData.mapY || 0) - modelData.radius
                    width: modelData.radius * 2 + 1
                    height: modelData.radius * 2 + 1
                    radius: width / 2
                    color: modelData.color
                    border.width: 1
                    border.color: theme.ink
                }
            }

            Item {
                    property real headingDegrees: ((180 - (((uiState.minimapData.playerDirection || 0) % 8) * 45)) + 360) % 360

                    width: 19
                    height: 19
                visible: uiState.minimapData.playerVisible || false
                x: (uiState.minimapData.playerX || 0) - (uiState.minimapData.mapX || 0) - width / 2
                y: (uiState.minimapData.playerY || 0) - (uiState.minimapData.mapY || 0) - height / 2
                transformOrigin: Item.Center
                rotation: headingDegrees

                Canvas {
                    anchors.fill: parent
                    antialiasing: false

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.fillStyle = "#ffffff"
                        ctx.strokeStyle = "#000000"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(width / 2, 0.5)
                        ctx.lineTo(width - 1.5, height - 3.5)
                        ctx.lineTo(width / 2, height - 6.5)
                        ctx.lineTo(1.5, height - 3.5)
                        ctx.closePath()
                        ctx.fill()
                        ctx.stroke()
                    }
                }
            }
        }

        Text {
            x: 18
            y: 2
            width: Math.max(0, parent.width - 40)
            text: uiState.minimapData.title || ""
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
            elide: Text.ElideRight
        }

        Text {
            x: (uiState.minimapData.coordsX || 0) - parent.x
            y: (uiState.minimapData.coordsY || 0) - parent.y
            width: Math.max(0, uiState.minimapData.coordsWidth || 0)
            horizontalAlignment: Text.AlignRight
            text: uiState.minimapData.coordsText || ""
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        RoChromeButton {
            x: (uiState.minimapData.closeX || 0) - parent.x
            y: (uiState.minimapData.closeY || 0) - parent.y
            width: Math.max(10, uiState.minimapData.closeWidth || 0)
            height: Math.max(10, uiState.minimapData.closeHeight || 0)
            pressed: uiState.minimapData.closePressed || false
            label: uiState.minimapData.closeLabel || ""
            labelSize: 10
        }
    }

    Item {
        x: uiState.minimapX + Math.max(0, uiState.minimapWidth - width)
        y: uiState.minimapY + uiState.minimapHeight + 6
        width: 56
        height: {
            const iconCount = (uiState.statusIcons || []).length
            return iconCount > 0 ? (iconCount * 56) + ((iconCount - 1) * 6) : 0
        }
        visible: uiState.minimapVisible && (uiState.statusIcons || []).length > 0
        z: 72

        Column {
            anchors.fill: parent
            spacing: 6

            Repeater {
                model: uiState.statusIcons || []

                delegate: Item {
                    required property var modelData
                    property real tintFraction: statusExpiryTintFraction(modelData)

                    width: 56
                    height: 56

                    Item {
                        anchors.fill: parent
                        clip: true

                        Image {
                            id: statusIconImage
                            anchors.fill: parent
                            source: statusIconSource(modelData.statusType || 0)
                            visible: source !== "" && status === Image.Ready
                            smooth: false
                            cache: false
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: !statusIconImage.visible
                            color: "#556574"
                            radius: 8
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !statusIconImage.visible
                            text: modelData.shortName || "?"
                            color: "#f6f1e0"
                            font.family: theme.fontFamily
                            font.pixelSize: 18
                            font.bold: true
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: Math.round(parent.height * tintFraction)
                            visible: tintFraction > 0
                            color: "#c92f2f"
                            opacity: 0.2 + (0.45 * tintFraction)
                        }
                    }
                }
            }
        }
    }

    RoFrame {
        x: uiState.inventoryX
        y: uiState.inventoryY
        width: uiState.inventoryWidth
        height: uiState.inventoryHeight
        visible: uiState.inventoryVisible
        Text {
            x: 17
            y: 3
            text: uiState.inventoryData.title || "Inventory"
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        Repeater {
            model: uiState.inventoryData.systemButtons || []

            delegate: RoChromeButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.inventoryX
                y: (modelData.y || 0) - uiState.inventoryY
                width: modelData.width || 0
                height: modelData.height || 0
                visible: modelData.visible || false
                label: modelData.label || ""
            }
        }

        Rectangle {
            x: 0
            y: 17
            width: parent.width
            height: parent.height - 17
            color: "transparent"
            visible: !uiState.inventoryMini

            Repeater {
                model: uiState.inventoryData.tabs || []

                delegate: Rectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.inventoryX
                    y: (modelData.y || 0) - uiState.inventoryY - 17
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: modelData.active ? theme.button : theme.rowAlt
                    border.width: 1
                    border.color: modelData.active ? theme.gold : theme.buttonBorder
                    visible: modelData.visible || false

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 9
                        font.bold: modelData.active || false
                    }
                }
            }

            Repeater {
                model: uiState.inventoryData.slots || []

                delegate: Rectangle {
                    required property var modelData
                    x: modelData.x - uiState.inventoryX
                    y: modelData.y - uiState.inventoryY - 17
                    width: modelData.width
                    height: modelData.height
                    color: modelData.hovered ? theme.rowHover : theme.fieldFill
                    border.width: 1
                    border.color: modelData.hovered ? theme.gold : theme.buttonBorder

                    Image {
                        id: inventoryIcon
                        anchors.centerIn: parent
                        width: Math.max(1, parent.width - 4)
                        height: Math.max(1, parent.height - 4)
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        cache: false
                        source: modelData.occupied && (modelData.itemId || 0) > 0 ? root.itemIconSource(modelData.itemId || 0) : ""
                        visible: source !== "" && status === Image.Ready
                    }

                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 4
                        text: modelData.occupied ? modelData.label : ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 9
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        visible: modelData.occupied && !inventoryIcon.visible
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 3
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 2
                        text: (modelData.count || 0) > 1 ? modelData.count : ""
                        color: theme.titleBottom
                        font.family: theme.fontFamily
                        font.pixelSize: 9
                        font.bold: true
                    }
                }
            }

            RoScrollBar {
                x: (uiState.inventoryData.scrollTrackX || 0) - uiState.inventoryX
                y: (uiState.inventoryData.scrollTrackY || 0) - uiState.inventoryY - 17
                width: uiState.inventoryData.scrollTrackWidth || 0
                height: uiState.inventoryData.scrollTrackHeight || 0
                visible: uiState.inventoryData.scrollBarVisible || false
                thumbX: (uiState.inventoryData.scrollThumbX || 0) - (uiState.inventoryData.scrollTrackX || 0)
                thumbY: (uiState.inventoryData.scrollThumbY || 0) - (uiState.inventoryData.scrollTrackY || 0)
                thumbWidth: uiState.inventoryData.scrollThumbWidth || 0
                thumbHeight: uiState.inventoryData.scrollThumbHeight || 0
            }

            Rectangle {
                x: 0
                y: parent.height - height
                width: parent.width
                height: 21
                color: theme.rowAlt
                border.width: 1
                border.color: theme.buttonBorder

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: (uiState.inventoryData.currentItemCount || 0) + " / " + (uiState.inventoryData.maxItemCount || 0)
                    color: theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                }
            }
        }
    }

    RoFrame {
        x: uiState.storageX
        y: uiState.storageY
        width: uiState.storageWidth
        height: uiState.storageHeight
        visible: uiState.storageVisible
        Text {
            x: 17
            y: 3
            text: uiState.storageData.title || "Kafra Storage"
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        Repeater {
            model: uiState.storageData.systemButtons || []

            delegate: RoChromeButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.storageX
                y: (modelData.y || 0) - uiState.storageY
                width: modelData.width || 0
                height: modelData.height || 0
                visible: modelData.visible || false
                label: modelData.label || ""
                labelSize: 8
                labelBold: true
            }
        }

        Rectangle {
            x: 0
            y: 17
            width: parent.width
            height: parent.height - 17
            color: "transparent"
            visible: !uiState.storageMini

            Repeater {
                model: uiState.storageData.tabs || []

                delegate: Rectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.storageX
                    y: (modelData.y || 0) - uiState.storageY - 17
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: modelData.active ? theme.button : theme.rowAlt
                    border.width: 1
                    border.color: modelData.active ? theme.gold : theme.buttonBorder
                    visible: modelData.visible || false

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 9
                        font.bold: modelData.active || false
                    }
                }
            }

            Rectangle {
                x: 36
                y: 0
                width: parent.width - 56
                height: 18
                color: theme.rowAlt
                border.width: 1
                border.color: theme.buttonBorder

                Text {
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Item"
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                    font.bold: true
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Qty"
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            Repeater {
                model: uiState.storageData.slots || []

                delegate: Rectangle {
                    required property var modelData
                    x: modelData.x - uiState.storageX
                    y: modelData.y - uiState.storageY - 17
                    width: modelData.width
                    height: modelData.height
                    color: modelData.hovered ? theme.rowHover : theme.fieldFill
                    border.width: 1
                    border.color: modelData.hovered ? theme.gold : theme.buttonBorder

                    Image {
                        id: storageIcon
                        x: 5
                        y: Math.floor((parent.height - height) / 2)
                        width: 20
                        height: 20
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        cache: false
                        source: modelData.occupied && (modelData.itemId || 0) > 0 ? root.itemIconSource(modelData.itemId || 0) : ""
                        visible: source !== "" && status === Image.Ready
                    }

                    Rectangle {
                        x: 4
                        y: 3
                        width: 22
                        height: 20
                        color: theme.fieldFill
                        border.width: 1
                        border.color: theme.buttonBorder
                        visible: modelData.occupied && !storageIcon.visible
                    }

                    Text {
                        x: 30
                        width: parent.width - 88
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.occupied ? modelData.label : ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 9
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        visible: modelData.occupied && !storageIcon.visible
                    }

                    Text {
                        x: 30
                        width: parent.width - 88
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.occupied ? modelData.label : ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        visible: modelData.occupied && storageIcon.visible
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.occupied ? Math.max(1, modelData.count || 0) : ""
                        color: theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }

            RoScrollBar {
                x: (uiState.storageData.scrollTrackX || 0) - uiState.storageX
                y: (uiState.storageData.scrollTrackY || 0) - uiState.storageY - 17
                width: uiState.storageData.scrollTrackWidth || 0
                height: uiState.storageData.scrollTrackHeight || 0
                visible: uiState.storageData.scrollBarVisible || false
                thumbX: (uiState.storageData.scrollThumbX || 0) - (uiState.storageData.scrollTrackX || 0)
                thumbY: (uiState.storageData.scrollThumbY || 0) - (uiState.storageData.scrollTrackY || 0)
                thumbWidth: uiState.storageData.scrollThumbWidth || 0
                thumbHeight: uiState.storageData.scrollThumbHeight || 0
            }

            Rectangle {
                x: 0
                y: parent.height - height
                width: parent.width
                height: 21
                color: theme.rowAlt
                border.width: 1
                border.color: theme.buttonBorder

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: (uiState.storageData.currentItemCount || 0) + " / " + (uiState.storageData.maxItemCount || 0)
                    color: theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                }
            }
        }
    }

    RoFrame {
        x: uiState.equipX
        y: uiState.equipY
        width: uiState.equipWidth
        height: uiState.equipHeight
        visible: uiState.equipVisible
        Text {
            x: 17
            y: 3
            text: uiState.equipData.title || ""
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        Repeater {
            model: uiState.equipData.systemButtons || []

            delegate: RoChromeButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.equipX
                y: (modelData.y || 0) - uiState.equipY
                width: modelData.width || 0
                height: modelData.height || 0
                visible: modelData.visible || false
                label: modelData.label || ""
            }
        }

        Rectangle {
            x: 0
            y: 17
            width: parent.width
            height: parent.height - 17
            color: "transparent"
            visible: !uiState.equipMini

            Rectangle {
                x: 98
                y: 15
                width: Math.max(1, 182 - 98)
                height: Math.max(1, parent.height - y - 12)
                color: theme.rowAlt
                border.width: 1
                border.color: theme.buttonBorder

                Image {
                    id: equipPreviewImage
                    anchors.centerIn: parent
                    readonly property real availableWidth: Math.max(1, parent.width - 6)
                    readonly property real availableHeight: Math.max(1, parent.height - 6)
                    readonly property real sourceAspectRatio: (status === Image.Ready && sourceSize.height > 0)
                        ? (sourceSize.width / sourceSize.height)
                        : 1.0
                    width: Math.min(availableWidth, availableHeight * sourceAspectRatio)
                    height: Math.min(availableHeight, availableWidth / Math.max(0.001, sourceAspectRatio))
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                    cache: false
                    source: parent.visible ? root.equipPreviewSource() : ""
                }
            }

            Repeater {
                model: uiState.equipData.slots || []

                delegate: Item {
                    required property var modelData
                    x: modelData.x - uiState.equipX
                    y: modelData.y - uiState.equipY - 17
                    width: modelData.leftColumn ? (modelData.width + 90) : (modelData.width + 60)
                    height: modelData.height

                    Rectangle {
                        x: modelData.leftColumn ? 0 : width - modelData.width
                        y: 0
                        width: modelData.width
                        height: modelData.height
                        color: modelData.hovered ? theme.rowHover : (modelData.occupied ? theme.rowSelected : theme.fieldFill)
                        border.width: 1
                        border.color: (modelData.hovered || modelData.occupied) ? theme.gold : theme.buttonBorder

                        Image {
                            id: equipIcon
                            anchors.centerIn: parent
                            width: Math.max(1, parent.width - 4)
                            height: Math.max(1, parent.height - 4)
                            fillMode: Image.PreserveAspectFit
                            smooth: false
                            cache: false
                            source: modelData.occupied && (modelData.itemId || 0) > 0 ? root.itemIconSource(modelData.itemId || 0) : ""
                            visible: source !== "" && status === Image.Ready
                        }
                    }

                    Text {
                        x: modelData.leftColumn ? (modelData.width + 4) : 0
                        y: 0
                        width: width - modelData.width - 4
                        height: parent.height
                        text: modelData.occupied ? modelData.label : ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 10
                        font.bold: modelData.hovered
                        horizontalAlignment: modelData.leftColumn ? Text.AlignLeft : Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        visible: modelData.occupied
                    }
                }
            }
        }
    }

    RoFrame {
        x: uiState.friendPartyX
        y: uiState.friendPartyY
        width: uiState.friendPartyWidth
        height: uiState.friendPartyHeight
        visible: uiState.friendPartyVisible
        Text {
            x: 10
            y: 2
            text: uiState.friendPartyData.title || "Friend / Party"
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        Repeater {
            model: uiState.friendPartyData.systemButtons || []

            delegate: RoChromeButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.friendPartyX
                y: (modelData.y || 0) - uiState.friendPartyY
                width: modelData.width || 0
                height: modelData.height || 0
                visible: modelData.visible || false
                label: modelData.label || ""
                labelSize: 9
                labelBold: true
            }
        }

        Rectangle {
            x: 0
            y: 17
            width: parent.width
            height: parent.height - 17
            color: theme.fieldFill

            Repeater {
                model: uiState.friendPartyData.tabs || []

                delegate: Rectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.friendPartyX
                    y: (modelData.y || 0) - uiState.friendPartyY - 17
                    width: modelData.width || 0
                    height: modelData.height || 0
                    radius: 3
                    color: modelData.active ? theme.button : theme.rowAlt
                    border.width: 1
                    border.color: modelData.active ? theme.gold : theme.buttonBorder

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }

            Rectangle {
                x: 10
                y: 23
                width: parent.width - 26
                height: 18
                color: theme.rowAlt
                border.width: 1
                border.color: theme.buttonBorder

                Text {
                    x: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Name"
                    color: theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                    font.bold: true
                }

                Text {
                    x: 110
                    anchors.verticalCenter: parent.verticalCenter
                    text: (uiState.friendPartyData.currentTab || 0) === 0 ? "State" : "Role"
                    color: theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                    font.bold: true
                }

                Text {
                    x: 171
                    anchors.verticalCenter: parent.verticalCenter
                    text: (uiState.friendPartyData.currentTab || 0) === 0 ? "Memo" : "Map"
                    color: theme.muted
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            Rectangle {
                x: 10
                y: 41
                width: parent.width - 26
                height: parent.height - 125
                color: theme.fieldFill
                border.width: 1
                border.color: theme.buttonBorder

                Repeater {
                    model: uiState.friendPartyData.rows || []

                    delegate: Rectangle {
                        required property var modelData
                        x: (modelData.x || 0) - uiState.friendPartyX - 10
                        y: (modelData.y || 0) - uiState.friendPartyY - 58
                        width: modelData.width || 0
                        height: modelData.height || 0
                        color: modelData.selected ? theme.rowSelected : theme.fieldFill

                        Rectangle {
                            x: 3
                            y: 3
                            width: 6
                            height: parent.height - 6
                            color: Qt.rgba(((modelData.color || 0) >> 16 & 255) / 255,
                                           ((modelData.color || 0) >> 8 & 255) / 255,
                                           ((modelData.color || 0) & 255) / 255,
                                           1.0)
                        }

                        Text {
                            x: 14
                            width: 88
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.name || ""
                            color: theme.ink
                            font.family: theme.fontFamily
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }

                        Text {
                            x: 109
                            width: 50
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.status || ""
                            color: theme.muted
                            font.family: theme.fontFamily
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }

                        Text {
                            x: 171
                            width: parent.width - 175
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.detail || ""
                            color: theme.muted
                            font.family: theme.fontFamily
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }
                }

                RoScrollBar {
                    x: width - 12
                    y: 1
                    width: 11
                    height: parent.height - 2
                    visible: uiState.friendPartyData.scrollBarVisible || false
                    thumbX: 1
                    thumbY: (uiState.friendPartyData.scrollThumbY || 0) - (uiState.friendPartyData.scrollTrackY || 0)
                    thumbWidth: width - 2
                    thumbHeight: uiState.friendPartyData.scrollThumbHeight || 0
                }
            }

            Repeater {
                model: uiState.friendPartyData.actionButtons || []

                delegate: RoChromeButton {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.friendPartyX
                    y: (modelData.y || 0) - uiState.friendPartyY - 17
                    width: modelData.width || 0
                    height: modelData.height || 0
                    visible: modelData.visible || false
                    label: modelData.label || ""
                    labelSize: 10
                    labelBold: true
                }
            }

            Rectangle {
                x: 10
                y: parent.height - 43
                width: parent.width - 20
                height: 34
                color: theme.rowAlt
                border.width: 1
                border.color: theme.buttonBorder

                Text {
                    x: 6
                    y: 4
                    text: uiState.friendPartyData.summaryTitle || ""
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                    font.bold: true
                }

                Text {
                    x: 86
                    width: parent.width - 92
                    y: 4
                    text: uiState.friendPartyData.summaryValue || ""
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }

                Text {
                    x: 6
                    y: 18
                    width: parent.width - 12
                    text: uiState.friendPartyData.summarySecondaryValue || ""
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }
        }
    }

    RoFrame {
        x: uiState.partySetupX
        y: uiState.partySetupY
        width: uiState.partySetupWidth
        height: uiState.partySetupHeight
        visible: uiState.partySetupVisible
        Text {
            x: 10
            y: 2
            text: uiState.partySetupData.title || "Party Setup"
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        Repeater {
            model: uiState.partySetupData.labels || []

            delegate: Text {
                required property var modelData
                x: (modelData.x || 0) - uiState.partySetupX
                y: (modelData.y || 0) - uiState.partySetupY
                width: modelData.width || 0
                height: modelData.height || 0
                text: modelData.text || ""
                color: (modelData.header || false) ? theme.muted : theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 10
                font.bold: modelData.header || false
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }

        Repeater {
            model: uiState.partySetupData.helpIcons || []

            delegate: RoChromeButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.partySetupX
                y: (modelData.y || 0) - uiState.partySetupY
                width: modelData.width || 0
                height: modelData.height || 0
                hovered: modelData.hovered || false
                label: "?"
                labelSize: 10
                labelBold: true
            }
        }

        Rectangle {
            x: (uiState.partySetupData.nameFieldX || 0) - uiState.partySetupX
            y: (uiState.partySetupData.nameFieldY || 0) - uiState.partySetupY
            width: uiState.partySetupData.nameFieldWidth || 0
            height: uiState.partySetupData.nameFieldHeight || 0
            color: (uiState.partySetupData.showNameEdit || false) ? theme.fieldFill : "transparent"
            border.width: (uiState.partySetupData.showNameEdit || false) ? 1 : 0
            border.color: (uiState.partySetupData.nameFocused || false) ? theme.gold : theme.fieldBorder

            Text {
                x: (uiState.partySetupData.showNameEdit || false) ? 6 : 0
                width: parent.width - ((uiState.partySetupData.showNameEdit || false) ? 12 : 0)
                anchors.verticalCenter: parent.verticalCenter
                text: uiState.partySetupData.nameValue || ""
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }

        Repeater {
            model: uiState.partySetupData.choices || []

            delegate: Item {
                required property var modelData
                x: (modelData.x || 0) - uiState.partySetupX
                y: (modelData.y || 0) - uiState.partySetupY
                width: modelData.width || 0
                height: modelData.height || 0

                Rectangle {
                    x: 0
                    y: 2
                    width: 14
                    height: 14
                    color: theme.fieldFill
                    border.width: 1
                    border.color: theme.buttonBorder

                    Rectangle {
                        x: 3
                        y: 3
                        width: 8
                        height: 8
                        color: theme.titleBottom
                        visible: modelData.selected || false
                    }
                }

                Text {
                    x: 20
                    width: parent.width - 20
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.label || ""
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }
        }

        Repeater {
            model: uiState.partySetupData.buttons || []

            delegate: RoChromeButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.partySetupX
                y: (modelData.y || 0) - uiState.partySetupY
                width: modelData.width || 0
                height: modelData.height || 0
                visible: modelData.visible || false
                label: modelData.label || ""
                labelSize: 10
                labelBold: true
            }
        }

        Rectangle {
            visible: !!(uiState.partySetupData.tooltipText || "")
            x: Math.max(4, Math.min(parent.width - width - 4, ((uiState.partySetupData.tooltipX || 0) - uiState.partySetupX) - Math.floor(width / 2)))
            y: Math.max(4, Math.min(parent.height - height - 4, ((uiState.partySetupData.tooltipY || 0) - uiState.partySetupY) - height - 14))
            width: 248
            height: tooltipText.paintedHeight + 12
            color: theme.ink
            border.width: 1
            border.color: theme.buttonBorder
            z: 10

            Text {
                id: tooltipText
                x: 8
                y: 6
                width: parent.width - 16
                text: uiState.partySetupData.tooltipText || ""
                color: "#ffffff"
                font.family: theme.fontFamily
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
        }
    }

    RoFrame {
        x: uiState.partyInviteX
        y: uiState.partyInviteY
        width: uiState.partyInviteWidth
        height: uiState.partyInviteHeight
        visible: uiState.partyInviteVisible
        Rectangle {
            x: 14
            y: 28
            width: parent.width - 28
            height: 52
            radius: 2
            color: theme.fieldFill
            border.width: 1
            border.color: theme.buttonBorder
        }

        Text {
            x: 10
            y: 2
            text: uiState.partyInviteData.title || "Party Invitation"
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        Text {
            x: 20
            y: 34
            width: parent.width - 40
            height: 42
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            text: uiState.partyInviteData.bodyText || ((uiState.partyInviteData.messageLines || []).join("\n"))
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 11
        }

        Repeater {
            model: uiState.partyInviteData.buttons || []

            delegate: RoChromeButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.partyInviteX
                y: (modelData.y || 0) - uiState.partyInviteY
                width: modelData.width || 0
                height: modelData.height || 0
                label: modelData.label || ""
                labelSize: 10
                labelBold: true
            }
        }
    }

    RoFrame {
        x: uiState.skillListX
        y: uiState.skillListY
        width: uiState.skillListWidth
        height: uiState.skillListHeight
        visible: uiState.skillListVisible
        Text {
            x: 17
            y: 3
            text: uiState.skillListData.title || ""
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        Repeater {
            model: uiState.skillListData.systemButtons || []

            delegate: RoChromeButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.skillListX
                y: (modelData.y || 0) - uiState.skillListY
                width: modelData.width || 0
                height: modelData.height || 0
                visible: modelData.visible || false
                label: modelData.label || ""
            }
        }

        Rectangle {
            x: 8
            y: 20
            width: parent.width - 16
            height: parent.height - 74
            radius: theme.buttonRadius
            color: theme.fieldFill
            border.width: 1
            border.color: theme.buttonBorder
        }

        Repeater {
            model: uiState.skillListData.rows || []

            delegate: Item {
                required property var modelData
                x: modelData.x - uiState.skillListX
                y: modelData.y - uiState.skillListY
                width: modelData.width
                height: modelData.height

                Rectangle {
                    anchors.fill: parent
                    color: modelData.selected ? theme.rowSelected : (modelData.hovered ? theme.rowHover : "transparent")
                    border.width: 1
                    border.color: modelData.selected ? theme.gold : (modelData.hovered ? theme.buttonBorder : "transparent")
                }

                Rectangle {
                    width: modelData.selected ? 3 : 0
                    height: parent.height
                    color: theme.gold
                }

                Rectangle {
                    x: 4
                    y: 1
                    width: 32
                    height: 32
                    color: theme.fieldFill
                    border.width: 1
                    border.color: theme.buttonBorder

                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        source: root.skillIconSource(modelData.skillId || 0)
                        visible: (modelData.iconVisible !== false) && source !== ""
                    }
                }

                Text {
                    x: 48
                    y: 3
                    width: parent.width - 88
                    text: modelData.name
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                Text {
                    x: 48
                    y: 18
                    width: 80
                    text: modelData.levelText
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                }

                Text {
                    x: 165
                    y: 18
                    width: parent.width - 198
                    text: modelData.rightText
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }

                RoTextButton {
                    x: modelData.upgradeX - modelData.x
                    y: modelData.upgradeY - modelData.y
                    width: modelData.upgradeWidth
                    height: modelData.upgradeHeight
                    visible: modelData.upgradeVisible
                    label: uiState.skillListData.upgradeLabel || ""
                    labelSize: 12
                    labelBold: true
                }
            }
        }

        RoScrollBar {
            x: (uiState.skillListData.scrollTrackX || 0) - uiState.skillListX
            y: (uiState.skillListData.scrollTrackY || 0) - uiState.skillListY
            width: uiState.skillListData.scrollTrackWidth || 0
            height: uiState.skillListData.scrollTrackHeight || 0
            visible: uiState.skillListData.scrollBarVisible || false
            thumbX: (uiState.skillListData.scrollThumbX || 0) - (uiState.skillListData.scrollTrackX || 0)
            thumbY: (uiState.skillListData.scrollThumbY || 0) - (uiState.skillListData.scrollTrackY || 0)
            thumbWidth: uiState.skillListData.scrollThumbWidth || 0
            thumbHeight: uiState.skillListData.scrollThumbHeight || 0
        }

        Rectangle {
            x: 8
            y: parent.height - 46
            width: parent.width - 16
            height: 38
            radius: theme.buttonRadius
            color: theme.rowAlt
            border.width: 1
            border.color: theme.buttonBorder
        }

        Text {
            x: 13
            y: parent.height - 18
            text: uiState.skillListData.skillPointText || ""
            color: theme.gold
            font.family: theme.fontFamily
            font.pixelSize: 11
            font.bold: true
        }

        Repeater {
            model: uiState.skillListData.bottomButtons || []

            delegate: RoChromeButton {
                required property var modelData
                x: modelData.x - uiState.skillListX
                y: modelData.y - uiState.skillListY
                width: modelData.width
                height: modelData.height
                pressed: modelData.pressed || false
                hovered: modelData.hovered || false
                label: modelData.label
                labelSize: 10
            }
        }
    }

    RoFrame {
        x: uiState.itemInfoX
        y: uiState.itemInfoY
        width: uiState.itemInfoWidth
        height: uiState.itemInfoHeight
        visible: uiState.itemInfoVisible
        z: 40
        Text {
            x: 8
            y: 3
            text: uiState.itemInfoData.title || "Item Information"
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        RoChromeButton {
            x: (uiState.itemInfoData.closeButton.x || 0) - uiState.itemInfoX
            y: (uiState.itemInfoData.closeButton.y || 0) - uiState.itemInfoY
            width: uiState.itemInfoData.closeButton.width || 0
            height: uiState.itemInfoData.closeButton.height || 0
            pressed: uiState.itemInfoData.closeButton.pressed || false
            hovered: uiState.itemInfoData.closeButton.hovered || false
            label: uiState.itemInfoData.closeButton.label || "X"
        }

        Rectangle {
            x: 8
            y: 25
            width: 96
            height: 96
            color: theme.fieldFill
            border.width: 1
            border.color: theme.buttonBorder

            Image {
                anchors.fill: parent
                anchors.margins: 2
                fillMode: Image.PreserveAspectFit
                smooth: false
                cache: false
                source: (uiState.itemInfoData.previewUsesCollection || false)
                    ? root.collectionImageSource(uiState.itemInfoData.itemId || 0, uiState.itemInfoData.identified)
                    : root.itemIconSource(uiState.itemInfoData.itemId || 0, uiState.itemInfoData.identified)
            }

            RoChromeButton {
                x: 4
                y: 4
                width: (uiState.itemInfoData.graphicsButton.width || 0)
                height: (uiState.itemInfoData.graphicsButton.height || 0)
                visible: uiState.itemInfoData.graphicsButton.visible || false
                pressed: uiState.itemInfoData.graphicsButton.pressed || false
                hovered: uiState.itemInfoData.graphicsButton.hovered || false
                label: uiState.itemInfoData.graphicsButton.label || "View"
                labelSize: 9
            }
        }

        Text {
            x: 112
            y: 25
            width: parent.width - 120
            text: uiState.itemInfoData.name || ""
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 13
            font.bold: true
            elide: Text.ElideRight
        }

        Column {
            id: itemInfoDetailsColumn
            x: 112
            y: 45
            width: parent.width - 120
            spacing: 2

            Repeater {
                model: uiState.itemInfoData.detailLines || []

                delegate: Text {
                    required property var modelData
                    width: parent.width
                    text: modelData
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
        }

        Text {
            x: 112
            y: itemInfoDetailsColumn.y + itemInfoDetailsColumn.height + 4
            width: parent.width - 120
            height: parent.height - y - (((uiState.itemInfoData.slots || []).length > 0) ? 56 : 20)
            text: root.roColorTextToRichText(uiState.itemInfoData.description || "")
            textFormat: Text.RichText
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            verticalAlignment: Text.AlignTop
        }

        Repeater {
            model: uiState.itemInfoData.slots || []

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.itemInfoX
                y: (modelData.y || 0) - uiState.itemInfoY
                width: modelData.width || 0
                height: modelData.height || 0
                color: !(modelData.available || false)
                    ? theme.rowAlt
                    : ((modelData.occupied || false) ? theme.rowSelected : theme.fieldFill)
                border.width: 1
                border.color: (modelData.occupied || false) ? theme.gold : theme.buttonBorder

                Image {
                    anchors.fill: parent
                    anchors.margins: 2
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                    cache: false
                    visible: modelData.occupied || false
                    source: root.itemIconSource(modelData.itemId || 0)
                }

                Rectangle {
                    id: cardTooltip
                    visible: (modelData.hovered || false) && (modelData.occupied || false) && !!modelData.tooltip
                    z: 5
                    width: cardTooltipLabel.implicitWidth + 14
                    height: cardTooltipLabel.implicitHeight + 8
                    x: (parent.width - width) / 2
                    y: -height - 4
                    radius: theme.buttonRadius
                    color: theme.ink
                    border.width: 1
                    border.color: theme.gold

                    Text {
                        id: cardTooltipLabel
                        anchors.centerIn: parent
                        text: modelData.tooltip || ""
                        color: theme.titleText
                        font.family: theme.fontFamily
                        font.pixelSize: 11
                    }
                }
            }
        }
    }

    RoFrame {
        x: uiState.skillDescribeX
        y: uiState.skillDescribeY
        width: uiState.skillDescribeWidth
        height: uiState.skillDescribeHeight
        visible: uiState.skillDescribeVisible
        z: 41
        Text {
            x: 8
            y: 3
            text: uiState.skillDescribeData.title || "Skill Information"
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        RoChromeButton {
            x: (uiState.skillDescribeData.closeButton.x || 0) - uiState.skillDescribeX
            y: (uiState.skillDescribeData.closeButton.y || 0) - uiState.skillDescribeY
            width: uiState.skillDescribeData.closeButton.width || 0
            height: uiState.skillDescribeData.closeButton.height || 0
            pressed: uiState.skillDescribeData.closeButton.pressed || false
            hovered: uiState.skillDescribeData.closeButton.hovered || false
            label: uiState.skillDescribeData.closeButton.label || "X"
        }

        Rectangle {
            x: 8
            y: 25
            width: 52
            height: 52
            color: theme.fieldFill
            border.width: 1
            border.color: theme.buttonBorder

            Image {
                anchors.fill: parent
                anchors.margins: 2
                fillMode: Image.PreserveAspectFit
                smooth: false
                cache: false
                source: root.skillIconSource(uiState.skillDescribeData.skillId || 0)
            }
        }

        Text {
            x: 68
            y: 25
            width: parent.width - 76
            text: uiState.skillDescribeData.name || ""
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 13
            font.bold: true
            elide: Text.ElideRight
        }

        Column {
            x: 68
            y: 45
            width: parent.width - 76
            spacing: 2

            Repeater {
                model: uiState.skillDescribeData.detailLines || []

                delegate: Text {
                    required property var modelData
                    width: parent.width
                    height: (modelData || "").length > 0 ? 14 : 0
                    visible: (modelData || "").length > 0
                    text: modelData
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
        }

        Rectangle {
            x: (uiState.skillDescribeData.descriptionX || 0) - uiState.skillDescribeX
            y: (uiState.skillDescribeData.descriptionY || 0) - uiState.skillDescribeY
            width: uiState.skillDescribeData.descriptionWidth || 0
            height: uiState.skillDescribeData.descriptionHeight || 0
            color: "transparent"
            clip: true

            Text {
                x: 0
                y: 8 - (uiState.skillDescribeData.descriptionScrollOffset || 0)
                width: parent.width
                height: Math.max(implicitHeight, uiState.skillDescribeData.descriptionContentHeight || 0)
                text: uiState.skillDescribeData.description || ""
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                verticalAlignment: Text.AlignTop
            }
        }

        RoScrollBar {
            x: (uiState.skillDescribeData.descriptionScrollBar.trackX || 0) - uiState.skillDescribeX
            y: (uiState.skillDescribeData.descriptionScrollBar.trackY || 0) - uiState.skillDescribeY
            width: uiState.skillDescribeData.descriptionScrollBar.trackWidth || 0
            height: uiState.skillDescribeData.descriptionScrollBar.trackHeight || 0
            visible: uiState.skillDescribeData.descriptionScrollBar.visible || false
            thumbX: (uiState.skillDescribeData.descriptionScrollBar.thumbX || 0) - (uiState.skillDescribeData.descriptionScrollBar.trackX || 0)
            thumbY: (uiState.skillDescribeData.descriptionScrollBar.thumbY || 0) - (uiState.skillDescribeData.descriptionScrollBar.trackY || 0)
            thumbWidth: uiState.skillDescribeData.descriptionScrollBar.thumbWidth || 0
            thumbHeight: uiState.skillDescribeData.descriptionScrollBar.thumbHeight || 0
        }
    }

    RoFrame {
        x: uiState.itemCollectionX
        y: uiState.itemCollectionY
        width: uiState.itemCollectionWidth
        height: uiState.itemCollectionHeight
        visible: uiState.itemCollectionVisible
        z: 42
        Text {
            x: 8
            y: 3
            text: uiState.itemCollectionData.title || "Card illustration"
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        RoChromeButton {
            x: (uiState.itemCollectionData.closeButton.x || 0) - uiState.itemCollectionX
            y: (uiState.itemCollectionData.closeButton.y || 0) - uiState.itemCollectionY
            width: uiState.itemCollectionData.closeButton.width || 0
            height: uiState.itemCollectionData.closeButton.height || 0
            pressed: uiState.itemCollectionData.closeButton.pressed || false
            hovered: uiState.itemCollectionData.closeButton.hovered || false
            label: uiState.itemCollectionData.closeButton.label || "X"
        }

        Rectangle {
            x: 8
            y: 25
            width: parent.width - 16
            height: parent.height - 33
            color: theme.fieldFill
            border.width: 1
            border.color: theme.buttonBorder

            Image {
                anchors.fill: parent
                anchors.margins: 0
                fillMode: Image.PreserveAspectFit
                smooth: false
                cache: false
                source: (uiState.itemCollectionData.mainUsesIllust || false)
                    ? root.illustImageSource(uiState.itemCollectionData.itemId || 0)
                    : ((uiState.itemCollectionData.mainUsesCollection || false)
                        ? root.collectionImageSource(uiState.itemCollectionData.itemId || 0)
                        : root.itemIconSource(uiState.itemCollectionData.itemId || 0))
            }
        }
    }

    RoFrame {
        x: uiState.itemIdentifyX
        y: uiState.itemIdentifyY
        width: uiState.itemIdentifyWidth
        height: uiState.itemIdentifyHeight
        visible: uiState.itemIdentifyVisible
        z: 43
        Text {
            x: 8
            y: 3
            text: uiState.itemIdentifyData.title || "Identify Item"
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        RoChromeButton {
            x: (uiState.itemIdentifyData.closeButton.x || 0) - uiState.itemIdentifyX
            y: (uiState.itemIdentifyData.closeButton.y || 0) - uiState.itemIdentifyY
            width: uiState.itemIdentifyData.closeButton.width || 0
            height: uiState.itemIdentifyData.closeButton.height || 0
            pressed: uiState.itemIdentifyData.closeButton.pressed || false
            hovered: uiState.itemIdentifyData.closeButton.hovered || false
            label: uiState.itemIdentifyData.closeButton.label || "X"
        }

        Rectangle {
            id: itemIdentifyList
            x: 8
            y: 27
            width: Math.max(0, parent.width - 30)
            height: 128
            color: theme.fieldFill
            border.width: 1
            border.color: theme.buttonBorder
            clip: true

            Text {
                anchors.centerIn: parent
                visible: (uiState.itemIdentifyData.rows || []).length === 0
                text: uiState.itemIdentifyData.emptyText || "No unidentified items available."
                color: theme.muted
                font.family: theme.fontFamily
                font.pixelSize: 10
            }

            Repeater {
                model: uiState.itemIdentifyData.rows || []

                delegate: Rectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.itemIdentifyX - itemIdentifyList.x
                    y: (modelData.y || 0) - uiState.itemIdentifyY - itemIdentifyList.y
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: (modelData.selected || false) ? theme.rowSelected : ((modelData.hovered || false) ? theme.rowHover : theme.fieldFill)
                    border.width: 1
                    border.color: (modelData.selected || false) ? theme.gold : theme.buttonBorder

                    Rectangle {
                        x: 4
                        y: 4
                        width: 24
                        height: 24
                        color: theme.fieldFill
                        border.width: 1
                        border.color: theme.buttonBorder

                        Image {
                            anchors.fill: parent
                            anchors.margins: 1
                            fillMode: Image.PreserveAspectFit
                            smooth: false
                            cache: false
                            source: root.itemIconSource(modelData.itemId || 0, modelData.identified)
                        }
                    }

                    Text {
                        x: 34
                        y: 5
                        width: parent.width - 70
                        text: modelData.label || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 11
                        font.bold: modelData.selected || false
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        y: 5
                        visible: (modelData.count || 0) > 1
                        text: modelData.count || 0
                        color: theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: 10
                    }
                }
            }
        }

        RoScrollBar {
            x: (uiState.itemIdentifyData.scrollBar.trackX || 0) - uiState.itemIdentifyX
            y: (uiState.itemIdentifyData.scrollBar.trackY || 0) - uiState.itemIdentifyY
            width: uiState.itemIdentifyData.scrollBar.trackWidth || 0
            height: uiState.itemIdentifyData.scrollBar.trackHeight || 0
            visible: uiState.itemIdentifyData.scrollBar.visible || false
            thumbX: (uiState.itemIdentifyData.scrollBar.thumbX || 0) - (uiState.itemIdentifyData.scrollBar.trackX || 0)
            thumbY: (uiState.itemIdentifyData.scrollBar.thumbY || 0) - (uiState.itemIdentifyData.scrollBar.trackY || 0)
            thumbWidth: uiState.itemIdentifyData.scrollBar.thumbWidth || 0
            thumbHeight: uiState.itemIdentifyData.scrollBar.thumbHeight || 0
        }

        RoChromeButton {
            x: (uiState.itemIdentifyData.okButton.x || 0) - uiState.itemIdentifyX
            y: (uiState.itemIdentifyData.okButton.y || 0) - uiState.itemIdentifyY
            width: uiState.itemIdentifyData.okButton.width || 0
            height: uiState.itemIdentifyData.okButton.height || 0
            enabled: uiState.itemIdentifyData.okButton.enabled || false
            pressed: uiState.itemIdentifyData.okButton.pressed || false
            hovered: uiState.itemIdentifyData.okButton.hovered || false
            label: uiState.itemIdentifyData.okButton.label || "OK"
            labelSize: 10
        }

        RoChromeButton {
            x: (uiState.itemIdentifyData.cancelButton.x || 0) - uiState.itemIdentifyX
            y: (uiState.itemIdentifyData.cancelButton.y || 0) - uiState.itemIdentifyY
            width: uiState.itemIdentifyData.cancelButton.width || 0
            height: uiState.itemIdentifyData.cancelButton.height || 0
            pressed: uiState.itemIdentifyData.cancelButton.pressed || false
            hovered: uiState.itemIdentifyData.cancelButton.hovered || false
            label: uiState.itemIdentifyData.cancelButton.label || "Cancel"
            labelSize: 10
        }
    }

    RoFrame {
        x: uiState.skillSelectX
        y: uiState.skillSelectY
        width: uiState.skillSelectWidth
        height: uiState.skillSelectHeight
        visible: uiState.skillSelectVisible
        z: 44
        Text {
            x: 8
            y: 3
            text: uiState.skillSelectData.title || "Select"
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        RoChromeButton {
            x: (uiState.skillSelectData.closeButton.x || 0) - uiState.skillSelectX
            y: (uiState.skillSelectData.closeButton.y || 0) - uiState.skillSelectY
            width: uiState.skillSelectData.closeButton.width || 0
            height: uiState.skillSelectData.closeButton.height || 0
            pressed: uiState.skillSelectData.closeButton.pressed || false
            hovered: uiState.skillSelectData.closeButton.hovered || false
            label: uiState.skillSelectData.closeButton.label || "X"
        }

        Rectangle {
            id: skillSelectList
            x: 8
            y: 27
            width: Math.max(0, parent.width - 30)
            height: 128
            color: theme.fieldFill
            border.width: 1
            border.color: theme.buttonBorder
            clip: true

            Text {
                anchors.centerIn: parent
                visible: (uiState.skillSelectData.rows || []).length === 0
                text: uiState.skillSelectData.emptyText || "Nothing to select."
                color: theme.muted
                font.family: theme.fontFamily
                font.pixelSize: 10
            }

            Repeater {
                model: uiState.skillSelectData.rows || []

                delegate: Rectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.skillSelectX - skillSelectList.x
                    y: (modelData.y || 0) - uiState.skillSelectY - skillSelectList.y
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: (modelData.selected || false) ? theme.rowSelected : ((modelData.hovered || false) ? theme.rowHover : theme.fieldFill)
                    border.width: 1
                    border.color: (modelData.selected || false) ? theme.gold : theme.buttonBorder

                    Rectangle {
                        x: 4
                        y: 4
                        width: 24
                        height: 24
                        visible: modelData.showIcon || false
                        color: theme.fieldFill
                        border.width: 1
                        border.color: theme.buttonBorder

                        Image {
                            anchors.fill: parent
                            anchors.margins: 1
                            fillMode: Image.PreserveAspectFit
                            smooth: false
                            cache: false
                            source: root.itemIconSource(modelData.itemId || 0, modelData.identified)
                        }
                    }

                    Text {
                        x: (modelData.showIcon || false) ? 34 : 8
                        y: 5
                        width: parent.width - ((modelData.showIcon || false) ? 70 : 44)
                        text: modelData.label || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 11
                        font.bold: modelData.selected || false
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        y: 5
                        visible: (modelData.count || 0) > 1
                        text: modelData.count || 0
                        color: theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: 10
                    }
                }
            }
        }

        RoScrollBar {
            x: (uiState.skillSelectData.scrollBar.trackX || 0) - uiState.skillSelectX
            y: (uiState.skillSelectData.scrollBar.trackY || 0) - uiState.skillSelectY
            width: uiState.skillSelectData.scrollBar.trackWidth || 0
            height: uiState.skillSelectData.scrollBar.trackHeight || 0
            visible: uiState.skillSelectData.scrollBar.visible || false
            thumbX: (uiState.skillSelectData.scrollBar.thumbX || 0) - (uiState.skillSelectData.scrollBar.trackX || 0)
            thumbY: (uiState.skillSelectData.scrollBar.thumbY || 0) - (uiState.skillSelectData.scrollBar.trackY || 0)
            thumbWidth: uiState.skillSelectData.scrollBar.thumbWidth || 0
            thumbHeight: uiState.skillSelectData.scrollBar.thumbHeight || 0
        }

        RoChromeButton {
            x: (uiState.skillSelectData.okButton.x || 0) - uiState.skillSelectX
            y: (uiState.skillSelectData.okButton.y || 0) - uiState.skillSelectY
            width: uiState.skillSelectData.okButton.width || 0
            height: uiState.skillSelectData.okButton.height || 0
            enabled: uiState.skillSelectData.okButton.enabled || false
            pressed: uiState.skillSelectData.okButton.pressed || false
            hovered: uiState.skillSelectData.okButton.hovered || false
            label: uiState.skillSelectData.okButton.label || "OK"
            labelSize: 10
        }

        RoChromeButton {
            x: (uiState.skillSelectData.cancelButton.x || 0) - uiState.skillSelectX
            y: (uiState.skillSelectData.cancelButton.y || 0) - uiState.skillSelectY
            width: uiState.skillSelectData.cancelButton.width || 0
            height: uiState.skillSelectData.cancelButton.height || 0
            pressed: uiState.skillSelectData.cancelButton.pressed || false
            hovered: uiState.skillSelectData.cancelButton.hovered || false
            label: uiState.skillSelectData.cancelButton.label || "Cancel"
            labelSize: 10
        }
    }

    RoFrame {
        x: uiState.itemCompositionX
        y: uiState.itemCompositionY
        width: uiState.itemCompositionWidth
        height: uiState.itemCompositionHeight
        visible: uiState.itemCompositionVisible
        z: 43
        Text {
            x: 8
            y: 3
            text: uiState.itemCompositionData.title || "Compound Item"
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        RoChromeButton {
            x: (uiState.itemCompositionData.closeButton.x || 0) - uiState.itemCompositionX
            y: (uiState.itemCompositionData.closeButton.y || 0) - uiState.itemCompositionY
            width: uiState.itemCompositionData.closeButton.width || 0
            height: uiState.itemCompositionData.closeButton.height || 0
            pressed: uiState.itemCompositionData.closeButton.pressed || false
            hovered: uiState.itemCompositionData.closeButton.hovered || false
            label: uiState.itemCompositionData.closeButton.label || "X"
        }

        Rectangle {
            id: itemCompositionList
            x: 8
            y: 27
            width: Math.max(0, parent.width - 30)
            height: 128
            color: theme.fieldFill
            border.width: 1
            border.color: theme.buttonBorder
            clip: true

            Text {
                anchors.centerIn: parent
                visible: (uiState.itemCompositionData.rows || []).length === 0
                text: uiState.itemCompositionData.emptyText || "No equipment can accept this card."
                color: theme.muted
                font.family: theme.fontFamily
                font.pixelSize: 10
            }

            Repeater {
                model: uiState.itemCompositionData.rows || []

                delegate: Rectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.itemCompositionX - itemCompositionList.x
                    y: (modelData.y || 0) - uiState.itemCompositionY - itemCompositionList.y
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: (modelData.selected || false) ? theme.rowSelected : ((modelData.hovered || false) ? theme.rowHover : theme.fieldFill)
                    border.width: 1
                    border.color: (modelData.selected || false) ? theme.gold : theme.buttonBorder

                    Rectangle {
                        x: 4
                        y: 4
                        width: 24
                        height: 24
                        color: theme.fieldFill
                        border.width: 1
                        border.color: theme.buttonBorder

                        Image {
                            anchors.fill: parent
                            anchors.margins: 1
                            fillMode: Image.PreserveAspectFit
                            smooth: false
                            cache: false
                            source: root.itemIconSource(modelData.itemId || 0, modelData.identified)
                        }
                    }

                    Text {
                        x: 34
                        y: 5
                        width: parent.width - 70
                        text: modelData.label || ""
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 11
                        font.bold: modelData.selected || false
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        y: 5
                        visible: (modelData.count || 0) > 1
                        text: modelData.count || 0
                        color: theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: 10
                    }
                }
            }
        }

        RoScrollBar {
            x: (uiState.itemCompositionData.scrollBar.trackX || 0) - uiState.itemCompositionX
            y: (uiState.itemCompositionData.scrollBar.trackY || 0) - uiState.itemCompositionY
            width: uiState.itemCompositionData.scrollBar.trackWidth || 0
            height: uiState.itemCompositionData.scrollBar.trackHeight || 0
            visible: uiState.itemCompositionData.scrollBar.visible || false
            thumbX: (uiState.itemCompositionData.scrollBar.thumbX || 0) - (uiState.itemCompositionData.scrollBar.trackX || 0)
            thumbY: (uiState.itemCompositionData.scrollBar.thumbY || 0) - (uiState.itemCompositionData.scrollBar.trackY || 0)
            thumbWidth: uiState.itemCompositionData.scrollBar.thumbWidth || 0
            thumbHeight: uiState.itemCompositionData.scrollBar.thumbHeight || 0
        }

        RoChromeButton {
            x: (uiState.itemCompositionData.okButton.x || 0) - uiState.itemCompositionX
            y: (uiState.itemCompositionData.okButton.y || 0) - uiState.itemCompositionY
            width: uiState.itemCompositionData.okButton.width || 0
            height: uiState.itemCompositionData.okButton.height || 0
            enabled: uiState.itemCompositionData.okButton.enabled || false
            pressed: uiState.itemCompositionData.okButton.pressed || false
            hovered: uiState.itemCompositionData.okButton.hovered || false
            label: uiState.itemCompositionData.okButton.label || "OK"
            labelSize: 10
        }

        RoChromeButton {
            x: (uiState.itemCompositionData.cancelButton.x || 0) - uiState.itemCompositionX
            y: (uiState.itemCompositionData.cancelButton.y || 0) - uiState.itemCompositionY
            width: uiState.itemCompositionData.cancelButton.width || 0
            height: uiState.itemCompositionData.cancelButton.height || 0
            pressed: uiState.itemCompositionData.cancelButton.pressed || false
            hovered: uiState.itemCompositionData.cancelButton.hovered || false
            label: uiState.itemCompositionData.cancelButton.label || "Cancel"
            labelSize: 10
        }
    }

    RoFrame {
        x: uiState.optionX
        y: uiState.optionY
        width: uiState.optionWidth
        height: uiState.optionHeight
        visible: uiState.optionVisible
        Text {
            x: 17
            y: 2
            text: uiState.optionData.title || ""
            color: theme.titleText
            font.family: theme.fontFamily
            font.pixelSize: theme.titleSize
            font.bold: true
        }

        Repeater {
            model: uiState.optionData.systemButtons || []

            delegate: RoChromeButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.optionX
                y: (modelData.y || 0) - uiState.optionY
                width: modelData.width || 0
                height: modelData.height || 0
                visible: modelData.visible || false
                label: modelData.label || ""
                labelSize: 8
            }
        }

        Rectangle {
            x: (uiState.optionData.contentX || 0) - uiState.optionX
            y: (uiState.optionData.contentY || 0) - uiState.optionY
            width: uiState.optionData.contentWidth || 0
            height: uiState.optionData.contentHeight || 0
            color: theme.fieldFill
            border.width: 1
            border.color: theme.buttonBorder
            visible: !(uiState.optionData.collapsed || false)
        }

        Repeater {
            model: uiState.optionData.tabs || []

            delegate: Rectangle {
                required property var modelData
                x: modelData.x - uiState.optionX
                y: modelData.y - uiState.optionY
                width: modelData.width
                height: modelData.height
                radius: theme.buttonRadius
                visible: !(uiState.optionData.collapsed || false)
                color: modelData.active ? theme.button : theme.rowAlt
                border.width: 1
                border.color: modelData.active ? theme.gold : theme.buttonBorder

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 11
                }
            }
        }

        Repeater {
            model: uiState.optionData.toggles || []

            delegate: Item {
                required property var modelData
                x: modelData.x - uiState.optionX
                y: modelData.y - uiState.optionY
                width: (uiState.optionData.contentWidth || 0) - 24
                height: Math.max(modelData.height, 16)
                visible: !(uiState.optionData.collapsed || false)

                Rectangle {
                    id: optionToggleBox
                    x: 0
                    y: 0
                    width: modelData.width
                    height: modelData.height
                    color: modelData.checked ? theme.rowSelected : theme.fieldFill
                    border.width: 1
                    border.color: theme.buttonBorder
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: modelData.width + 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.label
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 11
                }

                Text {
                    anchors.centerIn: optionToggleBox
                    text: modelData.checked ? "X" : ""
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }

        Repeater {
            model: uiState.optionData.sliders || []

            delegate: Item {
                required property var modelData
                x: modelData.x - uiState.optionX
                y: modelData.y - uiState.optionY
                width: modelData.width
                height: modelData.height
                visible: !(uiState.optionData.collapsed || false)
                readonly property int sliderMin: modelData.minValue !== undefined ? modelData.minValue : 0
                readonly property int sliderMax: modelData.maxValue !== undefined ? Math.max(sliderMin + 1, modelData.maxValue) : 127
                readonly property real sliderRange: Math.max(1, sliderMax - sliderMin)

                Text {
                    x: -60
                    y: -1
                    width: 56
                    text: modelData.label
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight
                }

                Rectangle {
                    x: 0
                    y: 4
                    width: parent.width
                    height: Math.max(1, parent.height - 8)
                    color: theme.scrollTrack
                    border.width: 1
                    border.color: theme.buttonBorder
                }

                Rectangle {
                    x: 4 + ((Math.max(0, parent.width - 8) * ((modelData.value || 0) - parent.sliderMin)) / parent.sliderRange) - 4
                    y: -2
                    width: 8
                    height: parent.height + 4
                    radius: 4
                    color: theme.button
                    border.width: 1
                    border.color: theme.gold
                }

                Text {
                    x: parent.width + 8
                    y: -1
                    width: 42
                    text: modelData.valueText || ""
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 11
                }
            }
        }

        Repeater {
            model: uiState.optionData.graphicsRows || []

            delegate: Rectangle {
                required property var modelData
                x: modelData.x - uiState.optionX
                y: modelData.y - uiState.optionY
                width: modelData.width
                height: modelData.height
                radius: theme.buttonRadius
                visible: !(uiState.optionData.collapsed || false)
                color: theme.fieldFill
                border.width: 1
                border.color: theme.buttonBorder

                Text {
                    x: 8
                    y: 4
                    text: modelData.label
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 11
                }

                Text {
                    x: 110
                    y: 4
                    width: Math.max(0, parent.width - 170)
                    text: modelData.value
                    color: theme.ink
                    font.family: theme.fontFamily
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                RoChromeButton {
                    x: modelData.prevX - modelData.x
                    y: modelData.prevY - modelData.y
                    width: modelData.prevWidth
                    height: modelData.prevHeight
                    label: modelData.prevLabel || ""
                    labelSize: 10
                    labelBold: true
                }

                RoChromeButton {
                    x: modelData.nextX - modelData.x
                    y: modelData.nextY - modelData.y
                    width: modelData.nextWidth
                    height: modelData.nextHeight
                    label: modelData.nextLabel || ""
                    labelSize: 10
                    labelBold: true
                }
            }
        }

        RoChromeButton {
            id: optionRestartButton
            readonly property var restartButtonData: uiState.optionData.restartButton || ({})
            x: (restartButtonData.x || 0) - uiState.optionX
            y: (restartButtonData.y || 0) - uiState.optionY
            width: restartButtonData.width || 0
            height: restartButtonData.height || 0
            visible: restartButtonData.visible || false
            label: optionRestartButton.restartButtonData.label || ""
            labelSize: 10
        }

        RoChromeButton {
            id: optionApplyButton
            readonly property var applyButtonData: uiState.optionData.applyButton || ({})
            x: (applyButtonData.x || 0) - uiState.optionX
            y: (applyButtonData.y || 0) - uiState.optionY
            width: applyButtonData.width || 0
            height: applyButtonData.height || 0
            visible: applyButtonData.visible || false
            label: optionApplyButton.applyButtonData.label || ""
            labelSize: 10
        }
    }

    RoFrame {
        showTitle: false
        x: uiState.shopChoiceX
        y: uiState.shopChoiceY
        width: uiState.shopChoiceWidth
        height: uiState.shopChoiceHeight
        visible: uiState.shopChoiceVisible
        Text {
            x: 12
            y: 10
            text: uiState.shopChoiceTitle
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 15
            font.bold: true
        }

        Text {
            x: 14
            y: 28
            width: parent.width - 28
            text: uiState.shopChoicePrompt
            color: theme.muted
            font.family: theme.fontFamily
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
        }

        Repeater {
            model: uiState.shopChoiceButtons

            delegate: RoTextButton {
                required property var modelData
                x: modelData.x - uiState.shopChoiceX
                y: modelData.y - uiState.shopChoiceY
                width: modelData.width
                height: modelData.height
                pressed: modelData.pressed || false
                hovered: modelData.hot || false
                label: modelData.label
                labelSize: 12
                labelBold: false
            }
        }
    }

    RoFrame {
        showTitle: false
        x: uiState.serverPanelX
        y: uiState.serverPanelY
        width: uiState.serverPanelWidth
        height: uiState.serverPanelHeight
        visible: uiState.serverSelectVisible
        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 4

            Text {
                text: uiState.serverPanelData.title || ""
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 14
                font.bold: true
            }

            Repeater {
                model: uiState.serverEntryLabels.length

                delegate: Rectangle {
                    readonly property bool isSelected: index === uiState.serverSelectedIndex
                    readonly property bool isHovered: index === uiState.serverHoverIndex
                    readonly property int statusWidth: 64
                    readonly property int labelLeft: isSelected ? 14 : 8
                    readonly property string labelText: {
                        var base = String(uiState.serverEntryLabels[index] || "")
                        if (base.length === 0) {
                            base = "Server " + (index + 1)
                        }
                        return "#" + (index + 1) + " " + base
                    }
                    readonly property string detailText: {
                        if (isSelected) {
                            return "Selected"
                        }
                        var value = String(uiState.serverEntryDetails[index] || "")
                        if (value.length > 0) {
                            return value
                        }
                        return ""
                    }

                    width: parent ? parent.width : 0
                    height: 24
                    radius: 2
                    color: isSelected
                        ? theme.rowSelected
                        : (isHovered ? theme.rowHover : theme.fieldFill)
                    border.width: 1
                    border.color: isSelected ? theme.gold : theme.buttonBorder
                    clip: true

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: isSelected ? 3 : 0
                        color: theme.gold
                        visible: isSelected
                    }

                    Text {
                        id: serverLabel
                        x: labelLeft
                        width: Math.max(1, parent.width - labelLeft - statusWidth - 12)
                        anchors.verticalCenter: parent.verticalCenter
                        text: labelText
                        color: theme.ink
                        font.family: theme.fontFamily
                        font.pixelSize: 12
                        font.bold: isSelected
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                        z: 1
                    }

                    Text {
                        id: serverStatus
                        x: parent.width - statusWidth - 8
                        width: statusWidth
                        anchors.verticalCenter: parent.verticalCenter
                        text: detailText
                        color: isSelected ? theme.ink : theme.muted
                        font.family: theme.fontFamily
                        font.pixelSize: 12
                        font.bold: isSelected
                        textFormat: Text.PlainText
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                        z: 1
                    }
                }
            }
        }
    }

    Rectangle {
        x: uiState.loginPanelX
        y: uiState.loginPanelY
        width: uiState.loginPanelWidth
        height: uiState.loginPanelHeight
        color: "transparent"
        border.width: 0
        visible: uiState.loginPanelVisible

        Image {
            anchors.fill: parent
            fillMode: Image.Stretch
            smooth: false
            cache: false
            source: parent.visible ? "image://openmidgard/loginpanel" : ""
        }

        RoField {
            x: 92
            y: 29
            width: 125
            height: 18
            editing: !uiState.loginPasswordFocused
        }

        Text {
            x: 98
            y: 31
            width: 112
            text: uiState.loginUserId + (!uiState.loginPasswordFocused && root.loginCaretVisible ? "|" : "")
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        RoField {
            x: 92
            y: 61
            width: 125
            height: 18
            editing: uiState.loginPasswordFocused
        }

        Text {
            x: 98
            y: 63
            width: 112
            text: uiState.loginPasswordMask + (uiState.loginPasswordFocused && root.loginCaretVisible ? "|" : "")
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        RoField {
            x: 232
            y: 33
            width: 16
            height: 16
            editing: uiState.loginSaveAccountChecked

            Text {
                anchors.centerIn: parent
                text: uiState.loginSaveAccountChecked ? "X" : ""
                color: theme.ink
                font.family: theme.fontFamily
                font.pixelSize: 11
                font.bold: true
            }
        }

        Text {
            x: 250
            y: 31
            text: uiState.loginPanelLabels.saveLabel || ""
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 11
        }

        Repeater {
            model: uiState.loginButtons

            delegate: RoTextButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.loginPanelX
                y: (modelData.y || 0) - uiState.loginPanelY
                width: modelData.width || 0
                height: modelData.height || 0
                label: modelData.label || ""
                labelSize: 11
                labelBold: true
            }
        }
    }

    Rectangle {
        x: uiState.charSelectPanelX
        y: uiState.charSelectPanelY
        width: uiState.charSelectPanelWidth
        height: uiState.charSelectPanelHeight
        color: "transparent"
        border.width: 0
        visible: uiState.charSelectVisible

        Image {
            anchors.fill: parent
            fillMode: Image.Stretch
            smooth: false
            cache: false
            source: parent.visible ? ("image://openmidgard/charselectpanel?rev=" + root.charSelectPanelKey()) : ""
        }

        Repeater {
            model: uiState.charSelectSlots

            delegate: Item {
                required property var modelData
                x: modelData.x - uiState.charSelectPanelX
                y: modelData.y - uiState.charSelectPanelY
                width: modelData.width
                height: modelData.height

                Image {
                    x: 0
                    y: 0
                    width: implicitWidth
                    height: implicitHeight
                    fillMode: Image.Pad
                    smooth: false
                    cache: false
                    visible: modelData.selected
                    source: visible ? "image://openmidgard/charselectslotselected" : ""
                }
            }
        }

        Repeater {
            model: uiState.charSelectSelectedDetails.fields || []

            delegate: Text {
                required property var modelData
                x: (modelData.x || 0) - uiState.charSelectPanelX
                y: (modelData.y || 0) - uiState.charSelectPanelY
                width: modelData.width || 0
                height: modelData.height || 0
                text: modelData.text || ""
                color: "#4b4b4b"
                font.family: theme.fontFamily
                font.pixelSize: 11
            }
        }

        Repeater {
            model: uiState.charSelectPageButtons

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.charSelectPanelX
                y: (modelData.y || 0) - uiState.charSelectPanelY
                width: modelData.width || 0
                height: modelData.height || 0
                visible: modelData.visible || false
                color: "transparent"
                border.width: 0

                Image {
                    anchors.fill: parent
                    fillMode: Image.Pad
                    smooth: false
                    cache: false
                    visible: (modelData.label || "") === ""
                    source: visible
                        ? ("image://openmidgard/charselectpage/" + ((modelData.id || 0) < 0 ? "prev" : "next"))
                        : ""
                }

                Text {
                    anchors.centerIn: parent
                    visible: (modelData.label || "") !== ""
                    text: modelData.label || ""
                    color: "#5a5448"
                    font.family: theme.fontFamily
                    font.pixelSize: 18
                    font.bold: true
                }
            }
        }

        Repeater {
            model: uiState.charSelectActionButtons

            delegate: RoTextButton {
                required property var modelData
                x: (modelData.x || 0) - uiState.charSelectPanelX
                y: (modelData.y || 0) - uiState.charSelectPanelY
                width: modelData.width || 0
                height: modelData.height || 0
                visible: modelData.visible || false
                label: modelData.label || ""
                labelSize: 11
                labelBold: true
            }
        }
    }

    Rectangle {
        x: uiState.makeCharPanelX
        y: uiState.makeCharPanelY
        width: uiState.makeCharPanelWidth
        height: uiState.makeCharPanelHeight
        color: "transparent"
        border.width: 0
        visible: uiState.makeCharVisible

        Image {
            anchors.fill: parent
            fillMode: Image.Stretch
            smooth: false
            cache: false
            source: parent.visible
                ? ("image://openmidgard/makecharpanel?rev="
                    + ((uiState.makeCharPanelData && uiState.makeCharPanelData.imageRevision) || 0)
                    + "&buttons=" + root.makeCharPanelButtonsKey())
                : ""
        }

        RoField {
            x: 62
            y: 244
            width: 100
            height: 18
            editing: uiState.makeCharNameFocused
        }

        Text {
            x: 68
            y: 246
            width: 88
            text: uiState.makeCharName + (uiState.makeCharNameFocused && root.loginCaretVisible ? "|" : "")
            color: theme.ink
            font.family: theme.fontFamily
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        Repeater {
            model: uiState.makeCharStatFields

            delegate: Text {
                required property var modelData
                x: (modelData.x || 0) - uiState.makeCharPanelX
                y: (modelData.y || 0) - uiState.makeCharPanelY
                width: 30
                text: modelData.value !== undefined ? modelData.value : ""
                color: "#3c2414"
                font.family: theme.fontFamily
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Repeater {
            model: uiState.makeCharButtons

            delegate: Item {
                required property var modelData
                x: (modelData.x || 0) - uiState.makeCharPanelX
                y: (modelData.y || 0) - uiState.makeCharPanelY
                width: modelData.width || 0
                height: modelData.height || 0
            }
        }
    }
    }
}
