import QtQuick 2.15

Item {
    id: root
    width: 1280
    height: 720

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 16
        width: 440
        radius: 8
        color: "#b01a1f2b"
        border.width: 1
        border.color: "#50d7dde8"

        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6

            Text {
                text: "Qt 6 GPU UI"
                color: "#ffffff"
                font.pixelSize: 22
                font.bold: true
            }

            Text {
                text: "Backend: " + uiState.backendName
                color: "#d7dde8"
                font.pixelSize: 14
            }

            Text {
                text: "Mode: " + uiState.modeName
                color: "#d7dde8"
                font.pixelSize: 14
            }

            Text {
                text: "Render path: " + uiState.renderPath
                color: "#9ee7a0"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Text {
                text: uiState.architectureNote
                color: "#ffcc66"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Rectangle {
                width: parent.width
                height: 1
                color: "#304050"
            }

            Text {
                text: "Login status: " + uiState.loginStatus
                color: "#d7dde8"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Text {
                text: "Recent chat:\n" + uiState.chatPreview
                color: "#b5d9ff"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Text {
                text: "Input: " + uiState.lastInput
                color: "#9ee7a0"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                width: parent.width
            }
        }
    }

    Repeater {
        model: uiState.anchors

        delegate: Item {
            required property var modelData
            x: modelData.x
            y: modelData.y
            width: label.implicitWidth + (modelData.showBubble === false ? 0 : 12)
            height: label.implicitHeight + (modelData.showBubble === false ? 0 : 8)

            Rectangle {
                anchors.fill: parent
                radius: 6
                color: modelData.background
                visible: modelData.showBubble !== false
                border.width: 1
                border.color: "#80ffffff"
            }

            Text {
                id: label
                anchors.centerIn: parent
                text: modelData.text
                color: modelData.foreground
                font.pixelSize: modelData.fontPixelSize || 14
                font.bold: modelData.bold !== false
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

        Rectangle {
            x: width - 10 - 68 - 8 - 68
            y: height - 10 - 22
            width: 68
            height: 22
            color: uiState.npcMenuOkPressed ? "#c4c4c4" : "#f0f0f0"
            border.width: 1
            border.color: "#6e6e6e"

            Text {
                anchors.centerIn: parent
                text: "OK"
                color: "#000000"
                font.pixelSize: 12
            }
        }

        Rectangle {
            x: width - 10 - 68
            y: height - 10 - 22
            width: 68
            height: 22
            color: uiState.npcMenuCancelPressed ? "#c4c4c4" : "#f0f0f0"
            border.width: 1
            border.color: "#6e6e6e"

            Text {
                anchors.centerIn: parent
                text: "Cancel"
                color: "#000000"
                font.pixelSize: 12
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
            x: width - 10 - 68
            y: height - 10 - 22
            width: 68
            height: 22
            visible: uiState.sayDialogHasAction
            color: uiState.sayDialogActionPressed ? "#c4c4c4" : (uiState.sayDialogActionHovered ? "#e4e4e4" : "#f0f0f0")
            border.width: 1
            border.color: "#6e6e6e"

            Text {
                anchors.centerIn: parent
                text: uiState.sayDialogActionLabel
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

        Rectangle {
            x: width - 10 - 68 - 8 - 68
            y: height - 10 - 22
            width: 68
            height: 22
            color: uiState.npcInputOkPressed ? "#c4c4c4" : "#f0f0f0"
            border.width: 1
            border.color: "#6e6e6e"

            Text {
                anchors.centerIn: parent
                text: "OK"
                color: "#000000"
                font.pixelSize: 12
            }
        }

        Rectangle {
            x: width - 10 - 68
            y: height - 10 - 22
            width: 68
            height: 22
            color: uiState.npcInputCancelPressed ? "#c4c4c4" : "#f0f0f0"
            border.width: 1
            border.color: "#6e6e6e"

            Text {
                anchors.centerIn: parent
                text: "Cancel"
                color: "#000000"
                font.pixelSize: 12
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
            text: uiState.itemShopTitle
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
                        width: uiState.itemShopTitle === "Sellable Items" ? 160 : (parent.width - 110)
                        text: "Item"
                        color: "#1e1e1e"
                        font.pixelSize: 11
                    }

                    Text {
                        x: 190
                        y: 2
                        width: 38
                        visible: uiState.itemShopTitle === "Sellable Items"
                        text: "Qty"
                        color: "#1e1e1e"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        x: width - 60
                        y: 2
                        width: 54
                        text: "Price"
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
                            width: uiState.itemShopTitle === "Sellable Items" ? 160 : (parent.width - 110)
                            text: modelData.name
                            color: "#1a1a1a"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        Text {
                            x: 188
                            y: 2
                            width: 38
                            visible: uiState.itemShopTitle === "Sellable Items"
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
            text: "Purchase"
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
                        text: "Item"
                        color: "#1e1e1e"
                        font.pixelSize: 11
                    }

                    Text {
                        x: 126
                        y: 2
                        width: 34
                        text: "Qty"
                        color: "#1e1e1e"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        x: width - 64
                        y: 2
                        width: 56
                        text: "Cost"
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
            text: "Total"
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
            text: "Sell"
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
                        text: "Item"
                        color: "#1e1e1e"
                        font.pixelSize: 11
                    }

                    Text {
                        x: 126
                        y: 2
                        width: 34
                        text: "Qty"
                        color: "#1e1e1e"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        x: width - 64
                        y: 2
                        width: 56
                        text: "Gain"
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
            text: "Total"
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
                required property var modelData
                x: 5 + index * 29
                y: 4
                width: 24
                height: 24
                color: modelData.hover ? "#d0d8e8" : "#f0f0f0"
                border.width: 1
                border.color: modelData.isSkill ? "#6a4fb0" : "#6a6a6a"

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 4
                    text: modelData.occupied ? (modelData.isSkill ? "S" : "I") : ""
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

        Text {
            x: 9
            y: uiState.basicInfoMini ? 3 : 5
            text: uiState.basicInfoMini ? (uiState.basicInfoData.name || "") : "Basic Info"
            color: uiState.basicInfoMini ? "#000000" : "#ffffff"
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
            text: "Lv. " + (uiState.basicInfoData.level || 0)
                + " / " + (uiState.basicInfoData.jobName || "")
                + " / Exp. " + (uiState.basicInfoData.expPercent || 0) + " %"
            color: "#000000"
            font.pixelSize: 11
        }

        Text {
            x: 17
            y: 33
            visible: uiState.basicInfoMini
            text: "HP " + (uiState.basicInfoData.hp || 0) + " / " + (uiState.basicInfoData.maxHp || 0)
                + "  |  SP " + (uiState.basicInfoData.sp || 0) + " / " + (uiState.basicInfoData.maxSp || 0)
                + "  |  " + (uiState.basicInfoData.money || 0) + " Z"
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
            text: "HP      " + (uiState.basicInfoData.hp || 0) + "  /  " + (uiState.basicInfoData.maxHp || 0)
            color: "#000000"
            font.pixelSize: 10
        }

        Text {
            x: 95
            y: 52
            visible: !uiState.basicInfoMini
            text: "SP      " + (uiState.basicInfoData.sp || 0) + "  /  " + (uiState.basicInfoData.maxSp || 0)
            color: "#000000"
            font.pixelSize: 10
        }

        Text {
            x: 17
            y: 72
            visible: !uiState.basicInfoMini
            text: "Base Lv. " + (uiState.basicInfoData.level || 0)
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
            text: "Job Lv. " + (uiState.basicInfoData.jobLevel || 0)
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
            text: "Weight : " + (uiState.basicInfoData.weight || 0) + " / " + (uiState.basicInfoData.maxWeight || 0)
            color: ((uiState.basicInfoData.maxWeight || 0) > 0 && (uiState.basicInfoData.weight || 0) * 100 >= uiState.basicInfoData.maxWeight * 50) ? "#ff0000" : "#000000"
            font.pixelSize: 11
        }

        Text {
            x: 107
            y: 103
            visible: !uiState.basicInfoMini
            text: "Zeny : " + (uiState.basicInfoData.money || 0)
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
            text: "Status"
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
            text: "Points " + (uiState.statusData.statusPoint || 0)
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

            Repeater {
                model: uiState.statusData.stats || []

                delegate: Item {
                    required property var modelData
                    x: 28
                    y: 6 + (index * 16)
                    width: 78
                    height: 14
                    visible: uiState.statusPage === 0

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
                        width: 36
                        text: modelData.value
                        color: "#000000"
                        font.pixelSize: 11
                    }

                    Text {
                        x: 52
                        y: 0
                        width: 20
                        text: modelData.cost > 0 ? modelData.cost : ""
                        color: "#4a4a4a"
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignRight
                    }

                    Rectangle {
                        x: (modelData.increaseX || 0) - modelData.x
                        y: (modelData.increaseY || 0) - modelData.y
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
                }
            }

            Text {
                x: 118
                y: 10
                visible: uiState.statusPage === 0
                text: "Atk  " + (uiState.statusData.attackText || "")
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 118
                y: 26
                visible: uiState.statusPage === 0
                text: "Matk " + (uiState.statusData.matkText || "")
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 118
                y: 42
                visible: uiState.statusPage === 0
                text: "Hit   " + (uiState.statusData.hit || 0)
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 118
                y: 58
                visible: uiState.statusPage === 0
                text: "Crit  " + (uiState.statusData.critical || 0)
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 204
                y: 74
                visible: uiState.statusPage === 0
                text: "Pts " + (uiState.statusData.statusPoint || 0)
                color: "#000000"
                font.pixelSize: 11
                font.bold: true
            }

            Text {
                x: 204
                y: 10
                visible: uiState.statusPage === 0
                text: "Def   " + (uiState.statusData.itemDefText || "")
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 204
                y: 26
                visible: uiState.statusPage === 0
                text: "Mdef " + (uiState.statusData.itemMdefText || "")
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 204
                y: 42
                visible: uiState.statusPage === 0
                text: "Flee  " + (uiState.statusData.fleeText || "")
                color: "#000000"
                font.pixelSize: 11
            }

            Text {
                x: 204
                y: 58
                visible: uiState.statusPage === 0
                text: "Aspd " + (uiState.statusData.aspdText || "")
                color: "#000000"
                font.pixelSize: 11
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

            Column {
                id: chatLinesColumn
                x: 4
                y: Math.max(4, parent.height - height - 4)
                width: parent.width - 8
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

        readonly property var minimapData: uiState.minimapData || ({})

        Rectangle {
            x: 1
            y: 1
            width: parent.width - 2
            height: 16
            radius: 8
            color: "#62729e"
        }

        Rectangle {
            x: minimapData.mapX - parent.x
            y: minimapData.mapY - parent.y
            width: minimapData.mapWidth || 0
            height: minimapData.mapHeight || 0
            color: "#12161d"
            border.width: 1
            border.color: "#485060"
            clip: true

            Image {
                anchors.fill: parent
                fillMode: Image.Stretch
                smooth: false
                cache: false
                source: parent.visible ? ("image://openmidgard/minimap?rev=" + (minimapData.imageRevision || 0)) : ""
            }
        }

        Text {
            x: 18
            y: 2
            text: "Mini Map"
            color: "#ffffff"
            font.pixelSize: 12
            font.bold: true
        }

        Text {
            x: (minimapData.coordsX || 0) - parent.x
            y: (minimapData.coordsY || 0) - parent.y
            width: 76
            text: minimapData.mapName || ""
            color: "#101010"
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        Text {
            x: (minimapData.coordsX || 0) - parent.x + 78
            y: (minimapData.coordsY || 0) - parent.y
            width: Math.max(0, (minimapData.coordsWidth || 0) - 78)
            horizontalAlignment: Text.AlignRight
            text: minimapData.coordsText || ""
            color: "#101010"
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        Repeater {
            model: minimapData.markers || []

            delegate: Rectangle {
                required property var modelData
                x: modelData.x - parent.x - modelData.radius
                y: modelData.y - parent.y - modelData.radius
                width: modelData.radius * 2 + 1
                height: modelData.radius * 2 + 1
                radius: width / 2
                color: modelData.color
                border.width: 1
                border.color: "#181818"
            }
        }

        Rectangle {
            x: (minimapData.closeX || 0) - parent.x
            y: (minimapData.closeY || 0) - parent.y
            width: Math.max(10, minimapData.closeWidth || 0)
            height: Math.max(10, minimapData.closeHeight || 0)
            radius: 2
            color: minimapData.closePressed ? "#b8c7da" : "#dde4ef"
            border.width: 1
            border.color: "#4d5662"

            Text {
                anchors.centerIn: parent
                text: "x"
                color: "#18202a"
                font.pixelSize: 10
                font.bold: true
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

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 8
                        width: parent.width - 4
                        text: modelData.occupied ? modelData.label : ""
                        color: "#000000"
                        font.pixelSize: 9
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
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

            Text {
                x: width - 56
                y: height - 18
                text: (uiState.inventoryData.viewOffset || 0) + " / " + (uiState.inventoryData.maxViewOffset || 0)
                color: "#4a4a4a"
                font.pixelSize: 10
            }

            Rectangle {
                x: 24
                y: height - 22
                width: parent.width - 48
                height: 18
                radius: 3
                color: "#d9d3c6"
                border.width: 1
                border.color: "#8c8578"
                visible: (uiState.inventoryData.hoveredTooltip || "").length > 0

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 12
                    text: uiState.inventoryData.hoveredTooltip || ""
                    color: "#000000"
                    font.pixelSize: 10
                    elide: Text.ElideRight
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
            text: "Equipment"
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
                height: Math.max(1, 188 - 32)
                color: "#d8d0c2"
                border.width: 1
                border.color: "#9d9488"
            }

            Repeater {
                model: uiState.equipData.slots || []

                delegate: Item {
                    required property var modelData
                    x: modelData.x - uiState.equipX
                    y: modelData.y - uiState.equipY - 17
                    width: modelData.leftColumn ? 114 : 84
                    height: modelData.height

                    Rectangle {
                        x: modelData.leftColumn ? 0 : width - modelData.width
                        y: 0
                        width: modelData.width
                        height: modelData.height
                        color: modelData.occupied ? "#d7dff0" : "#f5f2ea"
                        border.width: 1
                        border.color: modelData.occupied ? "#7e95bf" : "#a69f91"
                    }

                    Text {
                        x: modelData.leftColumn ? (modelData.width + 4) : 0
                        y: 4
                        width: modelData.leftColumn ? (width - modelData.width - 4) : (width - modelData.width - 4)
                        text: modelData.occupied ? modelData.label : ""
                        color: "#000000"
                        font.pixelSize: 10
                        horizontalAlignment: modelData.leftColumn ? Text.AlignLeft : Text.AlignRight
                        elide: Text.ElideRight
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
            x: 10
            y: 3
            text: "Skill Tree"
            color: "#000000"
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
                        text: "+"
                        color: "#000000"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }
        }

        Rectangle {
            x: uiState.skillListData.scrollTrackX - uiState.skillListX
            y: uiState.skillListData.scrollTrackY - uiState.skillListY
            width: uiState.skillListData.scrollTrackWidth || 0
            height: uiState.skillListData.scrollTrackHeight || 0
            visible: uiState.skillListData.scrollBarVisible
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
            text: "Skill Point : " + (uiState.skillListData.skillPointCount || 0)
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
            text: "Options"
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
            x: (uiState.optionData.restartButton.x || 0) - uiState.optionX
            y: (uiState.optionData.restartButton.y || 0) - uiState.optionY
            width: uiState.optionData.restartButton.width || 0
            height: uiState.optionData.restartButton.height || 0
            radius: 4
            visible: uiState.optionData.restartButton.visible || false
            color: "#f8faff"
            border.width: 1
            border.color: "#607096"

            Text {
                anchors.centerIn: parent
                text: uiState.optionData.restartButton.label || ""
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
                text: "Select Server"
                color: "#303030"
                font.pixelSize: 14
                font.bold: true
            }

            Repeater {
                model: uiState.serverEntries

                delegate: Rectangle {
                    required property var modelData

                    width: parent ? parent.width : 0
                    height: 22
                    radius: 2
                    color: index === uiState.serverSelectedIndex
                        ? "#d6e0c6"
                        : (index === uiState.serverHoverIndex ? "#e5e9e0" : "#faf8f2")
                    border.width: 1
                    border.color: "#a0a0a0"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: "#181818"
                        font.pixelSize: 12
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.detail
                        color: "#606060"
                        font.pixelSize: 12
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
        color: "#ebe5dc"
        border.width: 1
        border.color: "#303030"
        visible: uiState.loginPanelVisible

        Text {
            x: 14
            y: 10
            text: "Login"
            color: "#303030"
            font.pixelSize: 16
            font.bold: true
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
            text: uiState.loginUserId
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
            text: uiState.loginPasswordMask
            color: "#202020"
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        Rectangle {
            x: 232
            y: 33
            width: 16
            height: 16
            color: uiState.loginSaveAccountChecked ? "#6e8b3d" : "#ffffff"
            border.width: 1
            border.color: "#404040"
        }

        Text {
            x: 250
            y: 31
            text: "Save"
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
        color: "#f5efe6"
        border.width: 1
        border.color: "#6a4c34"
        visible: uiState.charSelectVisible

        Repeater {
            model: uiState.charSelectSlots

            delegate: Rectangle {
                required property var modelData
                x: modelData.x - uiState.charSelectPanelX
                y: modelData.y - uiState.charSelectPanelY
                width: modelData.width
                height: modelData.height
                color: modelData.selected ? "#e8d9b9" : "#fffaf2"
                border.width: 2
                border.color: modelData.selected ? "#9a6d38" : "#b89d79"

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Text {
                        text: modelData.occupied ? modelData.name : "Empty Slot"
                        color: "#50321e"
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Text {
                        text: modelData.occupied ? modelData.job : ""
                        color: "#704c30"
                        font.pixelSize: 13
                    }

                    Text {
                        text: modelData.occupied ? ("Lv. " + modelData.level) : ""
                        color: "#704c30"
                        font.pixelSize: 13
                    }
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
                color: "#50321e"
                font.pixelSize: 13
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
                border.width: 1
                border.color: "#786044"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label || ""
                    color: "#50321e"
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
        color: "#f5efe6"
        border.width: 1
        border.color: "#6a4c34"
        visible: uiState.makeCharVisible

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
            text: uiState.makeCharName
            color: "#202020"
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        Rectangle {
            x: 30
            y: 72
            width: 130
            height: 170
            radius: 8
            color: "#fffaf2"
            border.width: 1
            border.color: "#b89d79"

            Column {
                anchors.centerIn: parent
                spacing: 8

                Text {
                    text: "Preview"
                    color: "#50321e"
                    font.pixelSize: 18
                    font.bold: true
                }

                Text {
                    text: "Hair " + uiState.makeCharHairIndex
                    color: "#704c30"
                    font.pixelSize: 13
                }

                Text {
                    text: "Color " + uiState.makeCharHairColor
                    color: "#704c30"
                    font.pixelSize: 13
                }
            }
        }

        Rectangle {
            x: 190
            y: 40
            width: 190
            height: 240
            radius: 12
            color: "#fffaf2"
            border.width: 1
            border.color: "#b89d79"
        }

        Repeater {
            model: uiState.makeCharStatFields

            delegate: Row {
                required property var modelData
                x: (modelData.x || 0) - uiState.makeCharPanelX
                y: (modelData.y || 0) - uiState.makeCharPanelY
                spacing: 8

                Text {
                    width: 24
                    text: modelData.label || ""
                    color: "#50321e"
                    font.pixelSize: 12
                    font.bold: true
                }

                Text {
                    text: modelData.value !== undefined ? modelData.value : ""
                    color: "#3c2414"
                    font.pixelSize: 12
                }
            }
        }

        Repeater {
            model: uiState.makeCharButtons

            delegate: Rectangle {
                required property var modelData
                x: (modelData.x || 0) - uiState.makeCharPanelX
                y: (modelData.y || 0) - uiState.makeCharPanelY
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
}
