import QtQuick 2.15

Item {
    id: root
    width: parent ? parent.width : 1280
    height: parent ? parent.height : 720
    property bool loginCaretVisible: true

    function itemIconSource(itemId) {
        return itemId > 0 ? "image://openmidgard/item/" + itemId : ""
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
            ? "image://openmidgard/wallpaper"
            : ""
    }

    Repeater {
        model: uiState.anchors

        delegate: Item {
            required property var modelData
            property int bubblePaddingX: modelData.showBubble === false ? 0 : (modelData.paddingX || 12)
            property int bubblePaddingY: modelData.showBubble === false ? 0 : (modelData.paddingY || 8)
            property int maxTextWidth: modelData.maxTextWidth || 0
            property bool wrapText: modelData.wrap === true
            property int textWidth: {
                const measured = Math.max(1, Math.ceil(label.contentWidth || label.implicitWidth || 1))
                return maxTextWidth > 0 ? Math.min(maxTextWidth, measured) : measured
            }
            x: modelData.centerX !== undefined
                ? Math.round((modelData.centerX || 0) - width / 2)
                : (modelData.x || 0)
            y: modelData.bottomY !== undefined
                ? Math.round((modelData.bottomY || 0) - height)
                : (modelData.y || 0)
            z: modelData.z || 2000
            width: textWidth + bubblePaddingX
            height: Math.max(1, Math.ceil(label.implicitHeight)) + bubblePaddingY

            Rectangle {
                anchors.fill: parent
                radius: modelData.radius || 6
                color: modelData.background
                visible: modelData.showBubble !== false
                border.width: 1
                border.color: modelData.borderColor || "#80ffffff"
            }

            Text {
                id: label
                x: Math.floor(parent.bubblePaddingX / 2)
                y: Math.floor(parent.bubblePaddingY / 2)
                width: parent.textWidth
                text: modelData.text
                color: modelData.foreground
                font.pixelSize: modelData.fontPixelSize || 14
                font.bold: modelData.bold !== false
                wrapMode: parent.wrapText ? Text.Wrap : Text.NoWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 42
        width: Math.min(540, parent.width - 80)
        height: 92
        radius: 8
        color: "#e6191f26"
        border.width: 1
        border.color: "#c2b067"
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
                    color: "#f5efdd"
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
                    color: "#f5efdd"
                    font.pixelSize: 22
                    font.bold: true
                    visible: uiState.loadingProgress > 0
                }
            }

            Text {
                width: parent.width
                text: uiState.loadingMessage
                color: "#f5efdd"
                font.pixelSize: 16
                elide: Text.ElideRight
            }

            Rectangle {
                width: parent.width
                height: 20
                radius: 4
                color: "#323a44"
                border.width: 1
                border.color: "#c2b067"
                visible: uiState.loadingProgress > 0

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(0, (parent.width - 4) * uiState.loadingProgress)
                    height: parent.height - 4
                    radius: 3
                    color: "#e6c658"
                }
            }
        }
    }

    Repeater {
        model: uiState.notifications

        delegate: Rectangle {
            required property var modelData
            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            radius: 6
            color: "#de1f2328"
            border.width: 1
            border.color: modelData.accent

            Rectangle {
                x: 2
                y: 2
                width: parent.width - 4
                height: parent.height - 4
                radius: 5
                color: "#ee2b3138"
                border.width: 1
                border.color: "#70ffffff"
            }

            Text {
                anchors.centerIn: parent
                text: modelData.title
                color: "#f7f2df"
                font.pixelSize: 18
                font.bold: true
            }
        }
    }

    Rectangle {
        x: uiState.npcMenuX
        y: uiState.npcMenuY
        width: uiState.npcMenuWidth
        height: uiState.npcMenuHeight
        radius: 10
        color: "#f8f8f8"
        border.width: 1
        border.color: "#828282"
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
                        ? "#d1e0f4"
                        : (index === uiState.npcMenuHoverIndex ? "#e8eff8" : "transparent")
                    border.width: (index === uiState.npcMenuSelectedIndex || index === uiState.npcMenuHoverIndex) ? 1 : 0
                    border.color: "#a0b4cd"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 8
                        text: modelData
                        textFormat: Text.RichText
                        font.pixelSize: 12
                        clip: true
                    }
                }
            }
        }

        Repeater {
            model: uiState.npcMenuButtons

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.npcMenuX
                y: (modelData.y || 0) - uiState.npcMenuY
                width: modelData.width || 0
                height: modelData.height || 0
                color: modelData.pressed ? "#c4c4c4" : "#f0f0f0"
                border.width: 1
                border.color: "#6e6e6e"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 12
                }
            }
        }
    }

    Rectangle {
        x: uiState.sayDialogX
        y: uiState.sayDialogY
        width: uiState.sayDialogWidth
        height: uiState.sayDialogHeight
        radius: 10
        color: "#f8f8f8"
        border.width: 1
        border.color: "#828282"
        visible: uiState.sayDialogVisible

        Text {
            x: 10
            y: 10
            width: parent.width - 20
            height: uiState.sayDialogHasAction ? (parent.height - 10 - 10 - 22 - 8) : (parent.height - 20)
            text: uiState.sayDialogText
            textFormat: Text.RichText
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            verticalAlignment: Text.AlignTop
        }

        Rectangle {
            x: (uiState.sayDialogActionButton.x || 0) - uiState.sayDialogX
            y: (uiState.sayDialogActionButton.y || 0) - uiState.sayDialogY
            width: uiState.sayDialogActionButton.width || 0
            height: uiState.sayDialogActionButton.height || 0
            visible: uiState.sayDialogActionButton.visible || false
            color: (uiState.sayDialogActionButton.pressed || false) ? "#c4c4c4" : ((uiState.sayDialogActionButton.hovered || false) ? "#e4e4e4" : "#f0f0f0")
            border.width: 1
            border.color: "#6e6e6e"

            Text {
                anchors.centerIn: parent
                text: uiState.sayDialogActionButton.label || ""
                color: "#000000"
                font.pixelSize: 12
            }
        }
    }

    Rectangle {
        x: uiState.npcInputX
        y: uiState.npcInputY
        width: uiState.npcInputWidth
        height: uiState.npcInputHeight
        radius: 10
        color: "#f8f8f8"
        border.width: 1
        border.color: "#828282"
        visible: uiState.npcInputVisible

        Text {
            x: 10
            y: 9
            width: parent.width - 20
            text: uiState.npcInputLabel
            color: "#000000"
            font.pixelSize: 12
        }

        Rectangle {
            x: 10
            y: 26
            width: parent.width - 20
            height: 22
            color: "#fff7c8"
            border.width: 1
            border.color: "#000000"

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 12
                text: uiState.npcInputText
                color: "#000000"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        Repeater {
            model: uiState.npcInputButtons

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.npcInputX
                y: (modelData.y || 0) - uiState.npcInputY
                width: modelData.width || 0
                height: modelData.height || 0
                color: modelData.pressed ? "#c4c4c4" : "#f0f0f0"
                border.width: 1
                border.color: "#6e6e6e"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 12
                }
            }
        }
    }

    Rectangle {
        x: uiState.chooseMenuX
        y: uiState.chooseMenuY
        width: uiState.chooseMenuWidth
        height: uiState.chooseMenuHeight
        radius: 8
        color: "#ffffff"
        border.width: 1
        border.color: "#b8b8b8"
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
                color: index === uiState.chooseMenuPressedIndex
                    ? "#b7c8de"
                    : (index === uiState.chooseMenuSelectedIndex ? "#cad8ea" : "#f1f1f1")
                border.width: 1
                border.color: "#8f8f8f"

                Text {
                    anchors.centerIn: parent
                    text: modelData
                    color: "#1a1a1a"
                    font.pixelSize: 12
                }
            }
        }
    }

    Rectangle {
        x: uiState.itemShopX
        y: uiState.itemShopY
        width: uiState.itemShopWidth
        height: uiState.itemShopHeight
        color: "#ececec"
        border.width: 1
        border.color: "#484848"
        visible: uiState.itemShopVisible

        Rectangle {
            x: 0
            y: 0
            width: parent.width
            height: 17
            color: "#52657b"
        }

        Text {
            x: 6
            y: 1
            text: uiState.itemShopData.title || uiState.itemShopTitle
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Rectangle {
            x: 8
            y: 22
            width: parent.width - 16
            height: parent.height - 34
            color: "#f8f8f8"
            border.width: 1
            border.color: "#787878"

            Column {
                x: 1
                y: 1
                width: parent.width - 2
                spacing: 0

                Rectangle {
                    width: parent.width
                    height: 17
                    color: "#dee5ed"

                    Text {
                        x: 26
                        y: 2
                        width: (uiState.itemShopData.showQuantity || false) ? 160 : (parent.width - 110)
                        text: uiState.itemShopData.nameLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                    }

                    Text {
                        x: 190
                        y: 2
                        width: 38
                        visible: uiState.itemShopData.showQuantity || false
                        text: uiState.itemShopData.quantityLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        x: width - 60
                        y: 2
                        width: 54
                        text: uiState.itemShopData.priceLabel || ""
                        color: "#1e1e1e"
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
                        color: modelData.selected ? "#bccce2" : (modelData.hover ? "#e2eaf4" : "transparent")

                        Text {
                            x: 24
                            y: 2
                            width: (uiState.itemShopData.showQuantity || false) ? 160 : (parent.width - 110)
                            text: modelData.name
                            color: "#1a1a1a"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        Text {
                            x: 188
                            y: 2
                            width: 38
                            visible: uiState.itemShopData.showQuantity || false
                            text: modelData.quantity
                            color: "#303030"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            x: parent.width - 76
                            y: 2
                            width: 70
                            text: modelData.price
                            color: "#1c3c62"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        x: uiState.itemPurchaseX
        y: uiState.itemPurchaseY
        width: uiState.itemPurchaseWidth
        height: uiState.itemPurchaseHeight
        color: "#ececec"
        border.width: 1
        border.color: "#484848"
        visible: uiState.itemPurchaseVisible

        Rectangle {
            x: 0
            y: 0
            width: parent.width
            height: 17
            color: "#52657b"
        }

        Text {
            x: 6
            y: 1
            text: uiState.itemPurchaseData.title || ""
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Rectangle {
            x: 8
            y: 22
            width: parent.width - 16
            height: parent.height - 80
            color: "#f8f8f8"
            border.width: 1
            border.color: "#787878"

            Column {
                x: 1
                y: 1
                width: parent.width - 2
                spacing: 0

                Rectangle {
                    width: parent.width
                    height: 17
                    color: "#dee5ed"

                    Text {
                        x: 4
                        y: 2
                        text: uiState.itemPurchaseData.nameLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                    }

                    Text {
                        x: 126
                        y: 2
                        width: 34
                        text: uiState.itemPurchaseData.quantityLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        x: width - 64
                        y: 2
                        width: 56
                        text: uiState.itemPurchaseData.amountLabel || ""
                        color: "#1e1e1e"
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
                        color: modelData.selected ? "#bccce2" : (modelData.hover ? "#e2eaf4" : "transparent")

                        Text {
                            x: 4
                            y: 2
                            width: 126
                            text: modelData.name
                            color: "#1a1a1a"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        Text {
                            x: 126
                            y: 2
                            width: 34
                            text: modelData.quantity
                            color: "#303030"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            x: parent.width - 64
                            y: 2
                            width: 56
                            text: modelData.cost
                            color: "#1c3c62"
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
            color: "#242424"
            font.pixelSize: 11
        }

        Text {
            x: 90
            y: height - 75
            width: 138
            text: uiState.itemPurchaseTotal
            color: "#1c3c62"
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
        }

        Repeater {
            model: uiState.itemPurchaseButtons

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.itemPurchaseX
                y: (modelData.y || 0) - uiState.itemPurchaseY
                width: modelData.width || 0
                height: modelData.height || 0
                color: modelData.pressed ? "#aab9cd" : (modelData.hot ? "#c4d2e4" : "#dcdcdc")
                border.width: 1
                border.color: "#585858"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#181818"
                    font.pixelSize: 12
                }
            }
        }
    }

    Rectangle {
        x: uiState.itemSellX
        y: uiState.itemSellY
        width: uiState.itemSellWidth
        height: uiState.itemSellHeight
        color: "#ececec"
        border.width: 1
        border.color: "#484848"
        visible: uiState.itemSellVisible

        Rectangle {
            x: 0
            y: 0
            width: parent.width
            height: 17
            color: "#52657b"
        }

        Text {
            x: 6
            y: 1
            text: uiState.itemSellData.title || ""
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Rectangle {
            x: 8
            y: 22
            width: parent.width - 16
            height: parent.height - 80
            color: "#f8f8f8"
            border.width: 1
            border.color: "#787878"

            Column {
                x: 1
                y: 1
                width: parent.width - 2
                spacing: 0

                Rectangle {
                    width: parent.width
                    height: 17
                    color: "#dee5ed"

                    Text {
                        x: 4
                        y: 2
                        text: uiState.itemSellData.nameLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                    }

                    Text {
                        x: 126
                        y: 2
                        width: 34
                        text: uiState.itemSellData.quantityLabel || ""
                        color: "#1e1e1e"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        x: width - 64
                        y: 2
                        width: 56
                        text: uiState.itemSellData.amountLabel || ""
                        color: "#1e1e1e"
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
                        color: modelData.selected ? "#bccce2" : (modelData.hover ? "#e2eaf4" : "transparent")

                        Text {
                            x: 4
                            y: 2
                            width: 126
                            text: modelData.name
                            color: "#1a1a1a"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        Text {
                            x: 126
                            y: 2
                            width: 34
                            text: modelData.quantity
                            color: "#303030"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            x: parent.width - 64
                            y: 2
                            width: 56
                            text: modelData.gain
                            color: "#1c3c62"
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
            color: "#242424"
            font.pixelSize: 11
        }

        Text {
            x: 90
            y: height - 75
            width: 138
            text: uiState.itemSellTotal
            color: "#1c3c62"
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
        }

        Repeater {
            model: uiState.itemSellButtons

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.itemSellX
                y: (modelData.y || 0) - uiState.itemSellY
                width: modelData.width || 0
                height: modelData.height || 0
                color: modelData.pressed ? "#aab9cd" : (modelData.hot ? "#c4d2e4" : "#dcdcdc")
                border.width: 1
                border.color: "#585858"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#181818"
                    font.pixelSize: 12
                }
            }
        }
    }

    Rectangle {
        x: uiState.shortCutX
        y: uiState.shortCutY
        width: uiState.shortCutWidth
        height: uiState.shortCutHeight
        radius: 4
        color: "#d0d0d0"
        border.width: 1
        border.color: "#606060"
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
                color: modelData.hover ? "#d0d8e8" : "#f0f0f0"
                border.width: 1
                border.color: modelData.isSkill ? "#6a4fb0" : "#6a6a6a"

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
                    color: "#202020"
                    font.pixelSize: 10
                    font.bold: true
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 2
                    anchors.bottom: parent.bottom
                    text: modelData.count > 0 ? modelData.count : ""
                    color: "#ffffff"
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
            font.pixelSize: 11
            font.bold: true
            style: Text.Outline
            styleColor: "#000000"
            horizontalAlignment: Text.AlignRight
        }
    }

    Rectangle {
        x: uiState.basicInfoX
        y: uiState.basicInfoY
        width: uiState.basicInfoWidth
        height: uiState.basicInfoHeight
        radius: 4
        color: uiState.basicInfoMini ? "#ece7d8" : "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.basicInfoVisible

        Rectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            radius: 3
            color: "#6e8194"
            border.width: 1
            border.color: "#4e5d6c"
        }

        Text {
            x: uiState.basicInfoMini ? 9 : 17
            y: 3
            text: uiState.basicInfoMini ? (uiState.basicInfoData.name || "") : "Basic Info"
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.basicInfoData.systemButtons || []

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.basicInfoX
                y: (modelData.y || 0) - uiState.basicInfoY
                width: modelData.width || 0
                height: modelData.height || 0
                radius: 2
                color: "#d7d2c5"
                border.width: 1
                border.color: "#7f7a70"
                visible: modelData.visible || false

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }

        Text {
            x: 17
            y: 18
            visible: uiState.basicInfoMini
            text: uiState.basicInfoData.miniHeaderText || ""
            color: "#000000"
            font.pixelSize: 11
        }

        Text {
            x: 17
            y: 33
            visible: uiState.basicInfoMini
            text: uiState.basicInfoData.miniStatusText || ""
            color: "#000000"
            font.pixelSize: 11
        }

        Text {
            x: 9
            y: 24
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.name || ""
            color: "#000000"
            font.pixelSize: 12
        }

        Text {
            x: 9
            y: 38
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.jobName || ""
            color: "#000000"
            font.pixelSize: 12
        }

        Rectangle {
            x: 110
            y: 22
            width: 85
            height: 9
            visible: !uiState.basicInfoMini
            color: "#403c3c"
            border.width: 1
            border.color: "#202020"

            Rectangle {
                x: 1
                y: 1
                width: Math.max(0, (parent.width - 2) * ((uiState.basicInfoData.maxHp || 0) > 0 ? (uiState.basicInfoData.hp || 0) / uiState.basicInfoData.maxHp : 0))
                height: parent.height - 2
                color: ((uiState.basicInfoData.maxHp || 0) > 0 && (uiState.basicInfoData.hp || 0) * 100 < uiState.basicInfoData.maxHp * 25) ? "#d04848" : "#d06060"
            }
        }

        Rectangle {
            x: 110
            y: 43
            width: 85
            height: 9
            visible: !uiState.basicInfoMini
            color: "#403c3c"
            border.width: 1
            border.color: "#202020"

            Rectangle {
                x: 1
                y: 1
                width: Math.max(0, (parent.width - 2) * ((uiState.basicInfoData.maxSp || 0) > 0 ? (uiState.basicInfoData.sp || 0) / uiState.basicInfoData.maxSp : 0))
                height: parent.height - 2
                color: "#4a78d8"
            }
        }

        Text {
            x: 95
            y: 31
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.hpText || ""
            color: "#000000"
            font.pixelSize: 10
        }

        Text {
            x: 95
            y: 52
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.spText || ""
            color: "#000000"
            font.pixelSize: 10
        }

        Text {
            x: 17
            y: 72
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.baseLevelText || ""
            color: "#000000"
            font.pixelSize: 11
        }

        Rectangle {
            x: 84
            y: 77
            width: 102
            height: 6
            visible: !uiState.basicInfoMini
            color: "#ffffff"
            border.width: 1
            border.color: "#808080"

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
            color: "#000000"
            font.pixelSize: 11
        }

        Rectangle {
            x: 84
            y: 87
            width: 102
            height: 6
            visible: !uiState.basicInfoMini
            color: "#ffffff"
            border.width: 1
            border.color: "#808080"

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
            color: ((uiState.basicInfoData.maxWeight || 0) > 0 && (uiState.basicInfoData.weight || 0) * 100 >= uiState.basicInfoData.maxWeight * 50) ? "#ff0000" : "#000000"
            font.pixelSize: 11
        }

        Text {
            x: 107
            y: 103
            visible: !uiState.basicInfoMini
            text: uiState.basicInfoData.moneyText || ""
            color: "#000000"
            font.pixelSize: 11
        }

        Repeater {
            model: uiState.basicInfoData.menuButtons || []

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.basicInfoX
                y: (modelData.y || 0) - uiState.basicInfoY
                width: modelData.width || 0
                height: modelData.height || 0
                radius: 3
                color: "#d7d2c5"
                border.width: 1
                border.color: "#7f7a70"
                visible: modelData.visible || false

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }
    }

    Rectangle {
        x: uiState.statusX
        y: uiState.statusY
        width: uiState.statusWidth
        height: uiState.statusHeight
        radius: 4
        color: uiState.statusMini ? "#ece7d8" : "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.statusVisible

        Rectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            radius: 3
            color: uiState.statusMini ? "#d5d0c2" : "#6e8194"
            border.width: 1
            border.color: uiState.statusMini ? "#8b877b" : "#4e5d6c"
        }

        Text {
            x: 17
            y: 3
            text: uiState.statusData.title || ""
            color: uiState.statusMini ? "#000000" : "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.statusData.systemButtons || []

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.statusX
                y: (modelData.y || 0) - uiState.statusY
                width: modelData.width || 0
                height: modelData.height || 0
                radius: 2
                color: "#d7d2c5"
                border.width: 1
                border.color: "#7f7a70"
                visible: modelData.visible || false

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }

        Text {
            x: 96
            y: 3
            visible: uiState.statusMini
            text: uiState.statusData.miniPointsText || ""
            color: "#000000"
            font.pixelSize: 11
        }

        Rectangle {
            x: 0
            y: 17
            width: parent.width
            height: parent.height - 17
            color: "#e7e2d6"
            border.width: 0
            visible: !uiState.statusMini

            Rectangle {
                x: 0
                y: 0
                width: 20
                height: parent.height
                color: "#d3cdbf"
            }

            Repeater {
                model: uiState.statusData.pageTabs || []

                delegate: Rectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.statusX
                    y: (modelData.y || 0) - uiState.statusY - 17
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: modelData.active ? "#ebe7db" : "#c9c2b2"
                    border.width: 1
                    border.color: "#8c8578"
                    visible: modelData.visible || false

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label || ""
                        color: "#000000"
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
                            color: "#000000"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        Text {
                            x: 26
                            y: 0
                            width: 34
                            text: modelData.value
                            color: "#000000"
                            font.pixelSize: 11
                        }

                        Rectangle {
                            x: 64
                            y: 1
                            width: modelData.increaseWidth || 0
                            height: modelData.increaseHeight || 0
                            radius: 2
                            visible: modelData.canIncrease
                            color: "#d8d8d8"
                            border.width: 1
                            border.color: "#7f7a70"

                            Text {
                                anchors.centerIn: parent
                                text: modelData.increaseLabel || ""
                                color: "#000000"
                                font.pixelSize: 8
                                font.bold: true
                            }
                        }

                        Text {
                            x: 78
                            y: 0
                            width: 10
                            text: modelData.cost > 0 ? modelData.cost : ""
                            color: "#4a4a4a"
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
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 152
                y: 10
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.attackText || ""
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 132
                y: 26
                visible: uiState.statusPage === 0
                text: "Matk"
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 152
                y: 26
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.matkText || ""
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 132
                y: 42
                visible: uiState.statusPage === 0
                text: "Hit"
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 152
                y: 42
                width: 40
                visible: uiState.statusPage === 0
                text: (uiState.statusData.hit || 0).toString()
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 132
                y: 58
                visible: uiState.statusPage === 0
                text: "Crit"
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 152
                y: 58
                width: 40
                visible: uiState.statusPage === 0
                text: (uiState.statusData.critical || 0).toString()
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 204
                y: 74
                visible: uiState.statusPage === 0
                text: "Pts"
                color: "#000000"
                font.pixelSize: 11
                font.bold: true
            }

            Text {
                x: 243
                y: 74
                width: 30
                visible: uiState.statusPage === 0
                text: (uiState.statusData.statusPoint || 0).toString()
                color: "#000000"
                font.pixelSize: 11
                font.bold: true
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 204
                y: 10
                visible: uiState.statusPage === 0
                text: "Def"
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 233
                y: 10
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.itemDefText || ""
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 204
                y: 26
                visible: uiState.statusPage === 0
                text: "Mdef"
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 233
                y: 26
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.itemMdefText || ""
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 204
                y: 42
                visible: uiState.statusPage === 0
                text: "Flee"
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 233
                y: 42
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.fleeText || ""
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Text {
                x: 204
                y: 58
                visible: uiState.statusPage === 0
                text: "Aspd"
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 233
                y: 58
                width: 40
                visible: uiState.statusPage === 0
                text: uiState.statusData.aspdText || ""
                color: "#000000"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    Rectangle {
        x: uiState.chatWindowX
        y: uiState.chatWindowY
        width: uiState.chatWindowWidth
        height: uiState.chatWindowHeight
        color: "#50181818"
        border.width: 1
        border.color: "#ffffff"
        visible: uiState.chatWindowVisible

        Rectangle {
            x: 8
            y: 8
            width: parent.width - 16
            height: parent.height - 38
            color: "#90181818"
            border.width: 1
            border.color: "#60ffffff"
            clip: true

            readonly property var scrollBar: uiState.chatWindowScrollBar || ({})
            readonly property bool scrollBarVisible: scrollBar.visible || false
            readonly property int scrollBarWidth: scrollBarVisible ? 8 : 0
            readonly property int scrollBarGap: scrollBarVisible ? 4 : 0
            readonly property int chatTextWidth: width - 8 - scrollBarWidth - scrollBarGap

            Column {
                id: chatLinesColumn
                x: 4
                y: Math.max(4, parent.height - height - 4)
                width: parent.chatTextWidth
                spacing: 2

                Repeater {
                    model: uiState.chatWindowLines

                    delegate: Text {
                        required property var modelData
                        width: chatLinesColumn.width
                        text: modelData.text
                        color: modelData.color
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                x: parent.width - 4 - width
                y: 4
                width: parent.scrollBarWidth
                height: parent.height - 8
                visible: parent.scrollBarVisible
                color: "#40282828"
                border.width: 1
                border.color: "#80ffffff"

                Rectangle {
                    readonly property int totalLines: Math.max(1, parent.parent.scrollBar.totalLines || 0)
                    readonly property int visibleLineCount: Math.max(1, parent.parent.scrollBar.visibleLineCount || 0)
                    readonly property int firstVisibleLine: Math.max(0, parent.parent.scrollBar.firstVisibleLine || 0)
                    readonly property int thumbHeight: Math.max(18, Math.round(parent.height * visibleLineCount / totalLines))
                    readonly property int maxTravel: Math.max(0, parent.height - thumbHeight)
                    readonly property int scrollDenominator: Math.max(1, totalLines - visibleLineCount)
                    x: 1
                    y: Math.round(maxTravel * firstVisibleLine / scrollDenominator)
                    width: parent.width - 2
                    height: thumbHeight
                    color: "#c0d8d8d8"
                }
            }
        }

        Rectangle {
            x: 8
            y: parent.height - 30
            width: parent.width - 16
            height: 22
            color: uiState.chatWindowInputActive ? "#f5f5dc" : "#d2d2d2"
            border.width: 1
            border.color: "#000000"

            Text {
                x: 4
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 8
                text: uiState.chatWindowInputText + (uiState.chatWindowInputActive ? "_" : "")
                color: "#101010"
                font.pixelSize: 12
                elide: Text.ElideLeft
            }
        }
    }

    Rectangle {
        x: uiState.rechargeGaugeX
        y: uiState.rechargeGaugeY
        width: uiState.rechargeGaugeWidth
        height: uiState.rechargeGaugeHeight
        color: "#523f2d"
        border.width: 1
        border.color: "#523f2d"
        visible: uiState.rechargeGaugeVisible

        Rectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: parent.height - 2
            color: "#f4efe4"
        }

        Rectangle {
            x: 1
            y: 1
            width: Math.max(0, (parent.width - 2) * ((uiState.rechargeGaugeTotal || 0) > 0
                ? (uiState.rechargeGaugeAmount || 0) / uiState.rechargeGaugeTotal : 0))
            height: parent.height - 2
            color: "#307830"
        }
    }

    Rectangle {
        x: uiState.minimapX
        y: uiState.minimapY
        width: uiState.minimapWidth
        height: uiState.minimapHeight
        radius: 9
        color: "#d1d8e4"
        border.width: 1
        border.color: "#394256"
        visible: uiState.minimapVisible

        Rectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            radius: 8
            color: "#62729e"
        }

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
                    border.color: "#181818"
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
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
            elide: Text.ElideRight
        }

        Text {
            x: (uiState.minimapData.coordsX || 0) - parent.x
            y: (uiState.minimapData.coordsY || 0) - parent.y
            width: Math.max(0, uiState.minimapData.coordsWidth || 0)
            horizontalAlignment: Text.AlignRight
            text: uiState.minimapData.coordsText || ""
            color: "#101010"
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        Rectangle {
            x: (uiState.minimapData.closeX || 0) - parent.x
            y: (uiState.minimapData.closeY || 0) - parent.y
            width: Math.max(10, uiState.minimapData.closeWidth || 0)
            height: Math.max(10, uiState.minimapData.closeHeight || 0)
            radius: 2
            color: uiState.minimapData.closePressed ? "#b8c7da" : "#dde4ef"
            border.width: 1
            border.color: "#4d5662"

            Text {
                anchors.centerIn: parent
                text: uiState.minimapData.closeLabel || ""
                color: "#18202a"
                font.pixelSize: 10
                font.bold: true
            }
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

    Rectangle {
        x: uiState.inventoryX
        y: uiState.inventoryY
        width: uiState.inventoryWidth
        height: uiState.inventoryHeight
        radius: 4
        color: "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.inventoryVisible

        Rectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            radius: 3
            color: "#6e8194"
            border.width: 1
            border.color: "#4e5d6c"
        }

        Text {
            x: 17
            y: 3
            text: uiState.inventoryData.title || "Inventory"
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.inventoryData.systemButtons || []

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.inventoryX
                y: (modelData.y || 0) - uiState.inventoryY
                width: modelData.width || 0
                height: modelData.height || 0
                radius: 2
                color: "#d7d2c5"
                border.width: 1
                border.color: "#7f7a70"
                visible: modelData.visible || false

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }

        Rectangle {
            x: 0
            y: 17
            width: parent.width
            height: parent.height - 17
            color: "#e7e2d6"
            visible: !uiState.inventoryMini

            Repeater {
                model: uiState.inventoryData.tabs || []

                delegate: Rectangle {
                    required property var modelData
                    x: (modelData.x || 0) - uiState.inventoryX
                    y: (modelData.y || 0) - uiState.inventoryY - 17
                    width: modelData.width || 0
                    height: modelData.height || 0
                    color: modelData.active ? "#ebe7db" : "#c9c2b2"
                    border.width: 1
                    border.color: "#8c8578"
                    visible: modelData.visible || false

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label || ""
                        color: "#000000"
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
                    color: modelData.hovered ? "#d7dff0" : "#f5f2ea"
                    border.width: 1
                    border.color: modelData.hovered ? "#7e95bf" : "#a69f91"

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
                        color: "#000000"
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
                        color: "#222e50"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }
            }

            Rectangle {
                x: (uiState.inventoryData.scrollTrackX || 0) - uiState.inventoryX
                y: (uiState.inventoryData.scrollTrackY || 0) - uiState.inventoryY - 17
                width: uiState.inventoryData.scrollTrackWidth || 0
                height: uiState.inventoryData.scrollTrackHeight || 0
                visible: uiState.inventoryData.scrollBarVisible || false
                color: "#e3e7ee"
                border.width: 1
                border.color: "#a4adbd"

                Rectangle {
                    x: (uiState.inventoryData.scrollThumbX || 0) - (uiState.inventoryData.scrollTrackX || 0)
                    y: (uiState.inventoryData.scrollThumbY || 0) - (uiState.inventoryData.scrollTrackY || 0)
                    width: uiState.inventoryData.scrollThumbWidth || 0
                    height: uiState.inventoryData.scrollThumbHeight || 0
                    color: "#8192c7"
                    border.width: 1
                    border.color: "#3f5684"
                }
            }

            Rectangle {
                x: 0
                y: parent.height - height
                width: parent.width
                height: 21
                color: "#ddd7ca"
                border.width: 1
                border.color: "#bcb4a7"

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: (uiState.inventoryData.currentItemCount || 0) + " / " + (uiState.inventoryData.maxItemCount || 0)
                    color: "#4a4a4a"
                    font.pixelSize: 10
                }
            }
        }
    }

    Rectangle {
        x: uiState.equipX
        y: uiState.equipY
        width: uiState.equipWidth
        height: uiState.equipHeight
        radius: 4
        color: "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.equipVisible

        Rectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            radius: 3
            color: "#6e8194"
            border.width: 1
            border.color: "#4e5d6c"
        }

        Text {
            x: 17
            y: 3
            text: uiState.equipData.title || ""
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.equipData.systemButtons || []

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.equipX
                y: (modelData.y || 0) - uiState.equipY
                width: modelData.width || 0
                height: modelData.height || 0
                radius: 2
                color: "#d7d2c5"
                border.width: 1
                border.color: "#7f7a70"
                visible: modelData.visible || false

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }

        Rectangle {
            x: 0
            y: 17
            width: parent.width
            height: parent.height - 17
            color: "#e7e2d6"
            visible: !uiState.equipMini

            Rectangle {
                x: 98
                y: 15
                width: Math.max(1, 182 - 98)
                height: Math.max(1, parent.height - y - 12)
                color: "#d8d0c2"
                border.width: 1
                border.color: "#9d9488"

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
                        color: modelData.hovered ? "#d7dff0" : (modelData.occupied ? "#d7dff0" : "#f5f2ea")
                        border.width: 1
                        border.color: modelData.hovered ? "#7e95bf" : (modelData.occupied ? "#7e95bf" : "#a69f91")

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
                        color: "#000000"
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

    Rectangle {
        x: uiState.skillListX
        y: uiState.skillListY
        width: uiState.skillListWidth
        height: uiState.skillListHeight
        radius: 4
        color: "#ede9df"
        border.width: 1
        border.color: "#6b675f"
        visible: uiState.skillListVisible

        Rectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            radius: 3
            color: "#6e8194"
            border.width: 1
            border.color: "#4e5d6c"
        }

        Text {
            x: 17
            y: 3
            text: uiState.skillListData.title || ""
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.skillListData.systemButtons || []

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.skillListX
                y: (modelData.y || 0) - uiState.skillListY
                width: modelData.width || 0
                height: modelData.height || 0
                radius: 2
                color: "#d7d2c5"
                border.width: 1
                border.color: "#7f7a70"
                visible: modelData.visible || false

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#000000"
                    font.pixelSize: 8
                    font.bold: true
                }
            }
        }

        Rectangle {
            x: 41
            y: 17
            width: parent.width - 41
            height: parent.height - 67
            color: "#ffffff"
            border.width: 1
            border.color: "#c4c0b5"
        }

        Rectangle {
            x: 0
            y: 17
            width: 41
            height: parent.height - 67
            color: "#ddd7ca"
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
                    color: modelData.selected ? "#d7dff0" : (modelData.hovered ? "#ece8de" : "transparent")
                    border.width: 1
                    border.color: modelData.selected ? "#7e95bf" : (modelData.hovered ? "#b8b1a3" : "transparent")
                }

                Rectangle {
                    x: 4
                    y: 1
                    width: 32
                    height: 32
                    color: "#f5f2ea"
                    border.width: 1
                    border.color: "#a69f91"

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
                    color: "#000000"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                Text {
                    x: 48
                    y: 18
                    width: 80
                    text: modelData.levelText
                    color: "#000000"
                    font.pixelSize: 10
                }

                Text {
                    x: 165
                    y: 18
                    width: parent.width - 198
                    text: modelData.rightText
                    color: "#000000"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }

                Rectangle {
                    x: modelData.upgradeX - modelData.x
                    y: modelData.upgradeY - modelData.y
                    width: modelData.upgradeWidth
                    height: modelData.upgradeHeight
                    visible: modelData.upgradeVisible
                    color: modelData.upgradePressed ? "#b9c7de" : "#d7dff0"
                    border.width: 1
                    border.color: "#7e95bf"

                    Text {
                        anchors.centerIn: parent
                        text: uiState.skillListData.upgradeLabel || ""
                        color: "#000000"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }
        }

        Rectangle {
            x: (uiState.skillListData.scrollTrackX || 0) - uiState.skillListX
            y: (uiState.skillListData.scrollTrackY || 0) - uiState.skillListY
            width: uiState.skillListData.scrollTrackWidth || 0
            height: uiState.skillListData.scrollTrackHeight || 0
            visible: uiState.skillListData.scrollBarVisible || false
            color: "#e3e7ee"
            border.width: 1
            border.color: "#a4adbd"

            Rectangle {
                x: (uiState.skillListData.scrollThumbX || 0) - (uiState.skillListData.scrollTrackX || 0)
                y: (uiState.skillListData.scrollThumbY || 0) - (uiState.skillListData.scrollTrackY || 0)
                width: uiState.skillListData.scrollThumbWidth || 0
                height: uiState.skillListData.scrollThumbHeight || 0
                color: "#b4bccd"
                border.width: 1
                border.color: "#788296"
            }
        }

        Rectangle {
            x: 0
            y: parent.height - 50
            width: parent.width
            height: 50
            color: "#ddd7ca"
            border.width: 1
            border.color: "#bcb4a7"
        }

        Text {
            x: 13
            y: parent.height - 18
            text: uiState.skillListData.skillPointText || ""
            color: "#b09130"
            font.pixelSize: 11
            font.bold: true
        }

        Repeater {
            model: uiState.skillListData.bottomButtons || []

            delegate: Rectangle {
                required property var modelData
                x: modelData.x - uiState.skillListX
                y: modelData.y - uiState.skillListY
                width: modelData.width
                height: modelData.height
                radius: 3
                color: modelData.pressed ? "#b9c7de" : (modelData.hovered ? "#d7dff0" : "#e9e4d8")
                border.width: 1
                border.color: "#8c8578"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: "#000000"
                    font.pixelSize: 10
                }
            }
        }
    }

    Rectangle {
        x: uiState.optionX
        y: uiState.optionY
        width: uiState.optionWidth
        height: uiState.optionHeight
        radius: 8
        color: "#f4f7fc"
        border.width: 1
        border.color: "#394256"
        visible: uiState.optionVisible

        Rectangle {
            x: 0
            y: 0
            width: parent.width
            height: 17
            radius: 8
            color: "#62729e"
            border.width: 0
        }

        Rectangle {
            x: 0
            y: 8
            width: parent.width
            height: 9
            color: "#62729e"
        }

        Text {
            x: 17
            y: 2
            text: uiState.optionData.title || ""
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Repeater {
            model: uiState.optionData.systemButtons || []

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.optionX
                y: (modelData.y || 0) - uiState.optionY
                width: modelData.width || 0
                height: modelData.height || 0
                radius: 4
                color: "#f8faff"
                border.width: 1
                border.color: "#607096"
                visible: modelData.visible || false

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#28375c"
                    font.pixelSize: 9
                    font.bold: true
                }
            }
        }

        Rectangle {
            x: (uiState.optionData.contentX || 0) - uiState.optionX
            y: (uiState.optionData.contentY || 0) - uiState.optionY
            width: uiState.optionData.contentWidth || 0
            height: uiState.optionData.contentHeight || 0
            color: "#ffffff"
            border.width: 1
            border.color: "#a0abc2"
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
                radius: 6
                visible: !(uiState.optionData.collapsed || false)
                color: modelData.active ? "#ffffff" : "#dce4f1"
                border.width: 1
                border.color: modelData.active ? "#5f7096" : "#7a88a7"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: "#000000"
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
                    color: modelData.checked ? "#cfdaf0" : "#ffffff"
                    border.width: 1
                    border.color: "#7a88a7"
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: modelData.width + 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.label
                    color: "#000000"
                    font.pixelSize: 11
                }

                Text {
                    anchors.centerIn: optionToggleBox
                    text: modelData.checked ? "X" : ""
                    color: "#28375c"
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

                Text {
                    x: -44
                    y: -1
                    width: 40
                    text: modelData.label
                    color: "#000000"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight
                }

                Rectangle {
                    x: 0
                    y: 4
                    width: parent.width
                    height: Math.max(1, parent.height - 8)
                    color: "#b6c2db"
                    border.width: 1
                    border.color: "#6f7da0"
                }

                Rectangle {
                    x: 4 + ((Math.max(0, parent.width - 8) * (modelData.value || 0)) / 127) - 4
                    y: -2
                    width: 8
                    height: parent.height + 4
                    radius: 4
                    color: "#ffffff"
                    border.width: 1
                    border.color: "#576588"
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
                radius: 6
                visible: !(uiState.optionData.collapsed || false)
                color: "#f7faff"
                border.width: 1
                border.color: "#b0bad0"

                Text {
                    x: 8
                    y: 4
                    text: modelData.label
                    color: "#000000"
                    font.pixelSize: 11
                }

                Text {
                    x: 110
                    y: 4
                    width: Math.max(0, parent.width - 170)
                    text: modelData.value
                    color: "#000000"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                Rectangle {
                    x: modelData.prevX - modelData.x
                    y: modelData.prevY - modelData.y
                    width: modelData.prevWidth
                    height: modelData.prevHeight
                    radius: 4
                    color: "#f8faff"
                    border.width: 1
                    border.color: "#607096"

                    Text {
                        anchors.centerIn: parent
                        text: modelData.prevLabel || ""
                        color: "#28375c"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Rectangle {
                    x: modelData.nextX - modelData.x
                    y: modelData.nextY - modelData.y
                    width: modelData.nextWidth
                    height: modelData.nextHeight
                    radius: 4
                    color: "#f8faff"
                    border.width: 1
                    border.color: "#607096"

                    Text {
                        anchors.centerIn: parent
                        text: modelData.nextLabel || ""
                        color: "#28375c"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }
        }

        Rectangle {
            id: optionRestartButton
            readonly property var restartButtonData: uiState.optionData.restartButton || ({})
            x: (restartButtonData.x || 0) - uiState.optionX
            y: (restartButtonData.y || 0) - uiState.optionY
            width: restartButtonData.width || 0
            height: restartButtonData.height || 0
            radius: 4
            visible: restartButtonData.visible || false
            color: "#f8faff"
            border.width: 1
            border.color: "#607096"

            Text {
                anchors.centerIn: parent
                text: optionRestartButton.restartButtonData.label || ""
                color: "#28375c"
                font.pixelSize: 10
            }
        }
    }

    Rectangle {
        x: uiState.shopChoiceX
        y: uiState.shopChoiceY
        width: uiState.shopChoiceWidth
        height: uiState.shopChoiceHeight
        radius: 8
        color: "#f4f1ea"
        border.width: 1
        border.color: "#7f7a70"
        visible: uiState.shopChoiceVisible

        Text {
            x: 12
            y: 10
            text: uiState.shopChoiceTitle
            color: "#2a2a2a"
            font.pixelSize: 15
            font.bold: true
        }

        Text {
            x: 14
            y: 28
            width: parent.width - 28
            text: uiState.shopChoicePrompt
            color: "#343434"
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
        }

        Repeater {
            model: uiState.shopChoiceButtons

            delegate: Rectangle {
                required property var modelData
                x: modelData.x - uiState.shopChoiceX
                y: modelData.y - uiState.shopChoiceY
                width: modelData.width
                height: modelData.height
                radius: 4
                color: modelData.pressed ? "#aab9cd" : (modelData.hot ? "#c4d2e4" : "#dcdcdc")
                border.width: 1
                border.color: "#585858"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: "#181818"
                    font.pixelSize: 12
                }
            }
        }
    }

    Rectangle {
        x: uiState.serverPanelX
        y: uiState.serverPanelY
        width: uiState.serverPanelWidth
        height: uiState.serverPanelHeight
        radius: 8
        color: "#f3f0e7"
        border.width: 1
        border.color: "#787060"
        visible: uiState.serverSelectVisible

        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 4

            Text {
                text: uiState.serverPanelData.title || ""
                color: "#303030"
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
                        ? "#d6e7b8"
                        : (isHovered ? "#ecefe5" : "#faf8f2")
                    border.width: isSelected ? 2 : 1
                    border.color: isSelected ? "#5e7f2b" : "#a0a0a0"
                    clip: true

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: isSelected ? 8 : 0
                        color: "#6f9630"
                        visible: isSelected
                    }

                    Text {
                        id: serverLabel
                        x: labelLeft
                        width: Math.max(1, parent.width - labelLeft - statusWidth - 12)
                        anchors.verticalCenter: parent.verticalCenter
                        text: labelText
                        color: "#181818"
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
                        color: isSelected ? "#3f6111" : "#606060"
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

        Rectangle {
            x: 92
            y: 29
            width: 125
            height: 18
            color: "#f2f2f2"
            border.width: 1
            border.color: uiState.loginPasswordFocused ? "#b0b0b0" : "#707070"
        }

        Text {
            x: 98
            y: 31
            width: 112
            text: uiState.loginUserId + (!uiState.loginPasswordFocused && root.loginCaretVisible ? "|" : "")
            color: "#202020"
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        Rectangle {
            x: 92
            y: 61
            width: 125
            height: 18
            color: "#f2f2f2"
            border.width: 1
            border.color: uiState.loginPasswordFocused ? "#707070" : "#b0b0b0"
        }

        Text {
            x: 98
            y: 63
            width: 112
            text: uiState.loginPasswordMask + (uiState.loginPasswordFocused && root.loginCaretVisible ? "|" : "")
            color: "#202020"
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        Rectangle {
            x: 232
            y: 33
            width: 16
            height: 16
            color: "#ffffff"
            border.width: 1
            border.color: "#404040"

            Text {
                anchors.centerIn: parent
                text: uiState.loginSaveAccountChecked ? "X" : ""
                color: "#202020"
                font.pixelSize: 11
                font.bold: true
            }
        }

        Text {
            x: 250
            y: 31
            text: uiState.loginPanelLabels.saveLabel || ""
            color: "#303030"
            font.pixelSize: 11
        }

        Repeater {
            model: uiState.loginButtons

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.loginPanelX
                y: (modelData.y || 0) - uiState.loginPanelY
                width: modelData.width || 0
                height: modelData.height || 0
                radius: 3
                color: "#d8d0c4"
                border.width: 1
                border.color: "#6f6558"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#202020"
                    font.pixelSize: 11
                    font.bold: true
                }
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

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#5a5448"
                    font.pixelSize: 18
                    font.bold: true
                }
            }
        }

        Repeater {
            model: uiState.charSelectActionButtons

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.charSelectPanelX
                y: (modelData.y || 0) - uiState.charSelectPanelY
                width: modelData.width || 0
                height: modelData.height || 0
                radius: 3
                color: "#d8d0c4"
                border.width: 1
                border.color: "#6f6558"
                visible: modelData.visible || false

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#202020"
                    font.pixelSize: 11
                    font.bold: true
                }
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

        Rectangle {
            x: 62
            y: 244
            width: 100
            height: 18
            color: "#f2f2f2"
            border.width: 1
            border.color: uiState.makeCharNameFocused ? "#6a4c34" : "#909090"
        }

        Text {
            x: 68
            y: 246
            width: 88
            text: uiState.makeCharName + (uiState.makeCharNameFocused && root.loginCaretVisible ? "|" : "")
            color: "#202020"
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
