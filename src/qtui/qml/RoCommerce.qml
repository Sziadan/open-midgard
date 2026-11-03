import QtQuick 2.15

Item {
    id: hub
    anchors.fill: parent
    z: pending!==null || !!view.promptKind || !!(view.playerMenu&&view.playerMenu.visible) ? 3000 : 30
    RoTheme { id: theme }
    property var view: uiState.commerceData || ({})
    property var dragItem: ({})
    property string dragFrom: ""
    property bool dragging: false
    property var pending: null
    property string hoverText: ""
    function send(action, args) { uiState.commerceAction(action, args || {}) }
    Repeater {
        model:hub.view.shopBoards || []
        Rectangle {
            required property var modelData
            property bool nativeInteractive:true
            objectName:"commerceShopBoard_"+modelData.target
            x:Math.max(0,Math.min(hub.width-width,modelData.x-width/2)); y:modelData.y-height-12
            width:Math.min(240,Math.max(100,shopLabel.implicitWidth+30)); height:32; radius:7
            color:boardMouse.containsMouse?"#fff4d8":"#f5f7f6f2"; border.color:theme.gold; border.width:1
            Text { id:shopLabel; anchors.centerIn:parent; width:Math.min(210,implicitWidth); text:modelData.title; color:theme.ink; font.pixelSize:12; font.bold:true; elide:Text.ElideRight }
            MouseArea { id:boardMouse; anchors.fill:parent; hoverEnabled:true; cursorShape:Qt.PointingHandCursor; onClicked:hub.send("browseShop",{target:modelData.target}) }
        }
    }
    function icon(item) { return "image://openmidgard/item/" + item.id + "?identified=" + (item.identified ? "1" : "0") }
    function transfer(from, to, item) {
        if (item.count > 1) { pending = {from:from, to:to, item:item}; quantity.text = "1"; Qt.callLater(function(){quantity.forceActiveFocus();quantity.selectAll()}) }
        else send(to === "trade" ? "offerTrade" : "transfer", {from:from, to:to, index:item.index, amount:1})
    }
    function drop(to) {
        if (dragFrom === "cart" && to === "vending") send("offerVend", {index:dragItem.index})
        else if (dragFrom === "vending" && to !== "vending") send("removeVend", {index:dragItem.index})
        else if ((dragFrom === "inventory" && (to === "cart" || to === "trade")) || (dragFrom === "cart" && to === "inventory")) transfer(dragFrom,to,dragItem)
    }
    function dropAt(x,y) {
        // The scene is rendered offscreen, so resolve internal drops directly
        // against its panels rather than relying on a native drag service.
        var target=null
        for(var i=0;i<children.length;++i){var panel=children[i];if(panel.kind && panel.visible && panel.body){var p=panel.mapFromItem(hub,x,y);if(panel.contains(p) && (!target || panel.z>=target.z))target=panel}}
        if(target){var local=target.body.mapFromItem(hub,x,y);if(target.body.contains(local) && (target.kind!=="trade" || local.x<target.body.width/2))drop(target.kind);return true}
        if(dragFrom==="vending"){send("removeVend",{index:dragItem.index});return true}
        return false
    }
    component Button: RoTextButton {
        signal clicked()
        width: 100; height: 28; labelSize: 11
        MouseArea { anchors.fill: parent; enabled: parent.enabled; hoverEnabled: true
            onEntered: parent.hovered = true
            onExited: { parent.hovered = false; parent.pressed = false }
            onPressed: parent.pressed = true
            onReleased: parent.pressed = false
            onClicked: parent.clicked()
        }
    }
    component Field: RoField {
        id: field
        signal editingFinished()
        property alias text: editor.text
        property alias input: editor
        width: 120; height: 28
        TextInput { id: editor; anchors.fill: parent; anchors.margins: 6; color: theme.ink; font.family: theme.fontFamily; font.pixelSize: 12; selectByMouse: true; clip: true; onEditingFinished:field.editingFinished() }
    }
    component Tile: Rectangle {
        id: tile
        objectName: "commerceTile_" + sourceKind + "_" + item.index
        required property var item
        property string sourceKind: "inventory"
        width: sourceKind==="inventory" || sourceKind==="cart" ? 35 : 46; height: width; radius: 5
        color: mouse.containsMouse ? theme.rowHover : theme.rowAlt
        border.width: 1; border.color: item.equipped ? theme.gold : theme.bodyBorder
        Image { anchors.centerIn: parent; width: 30; height: 30; source: tile.item.id ? hub.icon(tile.item) : ""; smooth: false }
        Rectangle { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 1; visible:tile.item.count>1; width:stackLabel.implicitWidth+6; height:16; radius:3; color:"#e025313c"
            Text { id:stackLabel; anchors.centerIn:parent; text:tile.item.count; color:"#ffffff"; font.bold:true; font.pixelSize:12 }
        }
        MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.LeftButton | Qt.RightButton
            preventStealing: true
            property point origin
            onEntered: hub.hoverText = tile.item.name
            onExited: hub.hoverText = ""
            onPressed: function(event) { origin = Qt.point(event.x,event.y); hub.dragItem = tile.item; hub.dragFrom = tile.sourceKind }
            onPositionChanged: function(event) {
                if (pressedButtons & Qt.LeftButton) {
                    if (Math.abs(event.x-origin.x)+Math.abs(event.y-origin.y)>6) hub.dragging = true
                    var p=mapToItem(hub,event.x,event.y);ghost.x=p.x-20;ghost.y=p.y-20
                }
            }
            onReleased: function(event) {
                if (hub.dragging) { var p=mapToItem(hub,event.x,event.y);if(!hub.dropAt(p.x,p.y) && tile.sourceKind==="inventory")hub.send("externalDrop",{index:tile.item.index,x:p.x,y:p.y});hub.dragging=false }
                else if (event.button===Qt.RightButton) { var p=mapToItem(hub,event.x,event.y);hub.send("describe",{from:tile.sourceKind,index:tile.item.index,x:p.x+12,y:p.y+12}) }
            }
            onCanceled: hub.dragging=false
            onDoubleClicked: if(tile.sourceKind==="inventory")hub.send("use",{index:tile.item.index})
        }
    }
    component Status: Text { text: hub.hoverText || hub.view.status || ""; color: theme.muted; font.pixelSize: 10; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }

    Repeater {
        model: ["inventory","cart"]
        RoCommercePanel {
            id: bag
            required property string modelData
            objectName: "commerce_" + modelData
            kind: modelData; info: hub.view[modelData] || ({})
            caption: modelData==="inventory" ? "Inventory" : "Cart inventory"
            z: modelData==="inventory" ? 1 : 2
            Item { parent: bag.body; anchors.fill: parent
                Row { id: tabs; spacing: 4; height: 30
                    Repeater { model: bag.kind==="inventory" ? ["Usable","Equipment","Other"] : []
                        Button { required property string modelData; required property int index; width: 72; label: modelData; primary: hub.view.inventoryTab===index; onClicked: hub.send("inventoryTab",{tab:index}) }
                    }
                    Button { objectName:"commerceCartButton"; visible: bag.kind==="inventory" && !!hub.view.hasCart; label: "Cart"; width: 68; onClicked: hub.send("cart") }
                }
                Flickable { anchors.top: tabs.bottom; anchors.topMargin: 6; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: footer.top; anchors.bottomMargin: 6; clip: true; contentHeight: Math.max(height,grid.height)
                    Grid { id: grid; width: parent.width; columns: Math.max(1,Math.floor((width+4)/39)); spacing: 4
                        property var items: bag.kind==="inventory" ? hub.view.inventoryItems || [] : hub.view.cartItems || []
                        Repeater { model: Math.max(grid.items.length,grid.columns*Math.ceil(parent.parent.height/39))
                            Rectangle { required property int index; width:35; height:35; radius:5; color:theme.rowAlt; border.width:1; border.color:theme.bodyBorder
                                Tile { visible:parent.index<grid.items.length; item:grid.items[parent.index] || ({index:0,id:0,count:0}); sourceKind:bag.kind }
                            }
                        }
                    }
                    DropArea { anchors.fill: parent; keys:["commerce-item"]; onDropped: function(drop) { hub.drop(bag.kind);drop.acceptProposedAction() } }
                }
                Column { id: footer; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; spacing: 3
                    Text { text: bag.kind==="inventory" ? (hub.view.inventoryCount||0)+" / 100 slots · Double-click to use / equip" : (hub.view.cartCount||0)+" / "+(hub.view.cartCapacity||100)+" slots · Weight "+(hub.view.cartWeight||0)+" / "+(hub.view.cartMaxWeight||0); color: theme.muted; font.pixelSize: 10 }
                    Status { width: parent.width; height: 28 }
                }
            }
        }
    }

    RoCommercePanel {
        id: vend; objectName: "commerce_vending"; kind: "vending"; info: hub.view.vending || ({}); caption: hub.view.vendingActive ? "Vending · Open" : "Create vending shop"; z:3
        Item { parent: vend.body; anchors.fill: parent
            Column { id: vendHeader; width: parent.width; spacing: 6
                Text { text: hub.view.vendingActive ? "Your shop is open. Close it to edit stock." : "Drag cart items below. Drag a listing out to remove it."; width: parent.width; wrapMode: Text.WordWrap; color: theme.muted; font.pixelSize: 11 }
                Field { id: shopTitle; width: parent.width; text: "My shop"; input.maximumLength: 79; enabled: !hub.view.vendingActive && !hub.view.vendingPending }
                Text { text: "ITEM                                   QUANTITY    ZENY EACH"; font.pixelSize: 10; color: theme.muted }
            }
            Flickable { anchors.top: vendHeader.bottom; anchors.topMargin: 8; anchors.bottom: vendFooter.top; anchors.bottomMargin: 8; width: parent.width; clip: true; contentHeight: Math.max(height,vendRows.height)
                Column { id: vendRows; width: parent.width; spacing: 5
                    Repeater { model: hub.view.draft || []
                        Rectangle { required property var modelData; width: vendRows.width; height: 56; color: theme.rowAlt; radius: 4
                            Tile { x:4; y:5; item: modelData; sourceKind:"vending" }
                            Text { x:56; y:7; width: parent.width-62; text: modelData.name; color:theme.ink; font.pixelSize:11; elide:Text.ElideRight }
                            Field { id: vendAmount; x:56; y:26; width:62; height:25; text: modelData.count; input.validator: IntValidator { bottom:1; top:32767 } enabled: !hub.view.vendingActive && !hub.view.vendingPending
                                onEditingFinished: if(input.acceptableInput && vendPrice.input.acceptableInput)hub.send("editVend",{index:modelData.index,amount:Number(text),price:Number(vendPrice.text)})
                            }
                            Field { id: vendPrice; x:124; y:26; width:110; height:25; text:modelData.price; input.validator: IntValidator { bottom:1; top:1000000000 } enabled: !hub.view.vendingActive && !hub.view.vendingPending
                                onEditingFinished: if(input.acceptableInput && vendAmount.input.acceptableInput)hub.send("editVend",{index:modelData.index,amount:Number(vendAmount.text),price:Number(text)})
                            }
                            Button { x:parent.width-66; y:27; width:60; height:24; label:"Remove"; enabled: !hub.view.vendingActive && !hub.view.vendingPending; onClicked:hub.send("removeVend",{index:modelData.index}) }
                        }
                    }
                }
                DropArea { anchors.fill: parent; keys:["commerce-item"]; onDropped: function(drop){hub.drop("vending");drop.acceptProposedAction()} }
            }
            Column { id:vendFooter; width:parent.width; anchors.bottom:parent.bottom; spacing:6
                Text { text:(hub.view.draft||[]).length+" / "+(hub.view.vendingSlots||0)+" listings"; color:theme.muted; font.pixelSize:11 }
                Button { label:hub.view.vendingActive ? "Close shop" : hub.view.vendingPending ? "Opening…" : "Open shop"; enabled:!hub.view.vendingPending; primary:true
                    onClicked: if(hub.view.vendingActive)hub.send("close",{to:"vending"});else{vend.forceActiveFocus();Qt.callLater(function(){hub.send("openVend",{text:shopTitle.text})})}
                }
                Status { width:parent.width; height:28 }
            }
        }
    }

    RoCommercePanel {
        id:trade; objectName:"commerce_trade"; kind:"trade"; info:hub.view.trade||({}); caption:"Trade · "+(hub.view.partner||"Player"); z:4
        Item { parent:trade.body; anchors.fill:parent
            Text { id:tradeHeading; width:parent.width; text:hub.view.tradeIncoming ? "Trade invitation from "+hub.view.partner : hub.view.tradeWaiting ? "Waiting for the other player…" : "Review both offers. Lock your offer, then confirm the exchange."; color:theme.ink; font.pixelSize:12; wrapMode:Text.WordWrap }
            Row { visible:!!hub.view.tradeIncoming; anchors.top:tradeHeading.bottom; anchors.topMargin:16; spacing:8
                Button { label:"Accept"; primary:true; onClicked:hub.send("tradeReply",{amount:1}) }
                Button { label:"Decline"; onClicked:hub.send("tradeReply",{amount:0}) }
            }
            Row { id:tradeSides; visible:!!hub.view.tradeActive; anchors.top:tradeHeading.bottom; anchors.topMargin:10; width:parent.width; height:parent.height-126; spacing:12
                Repeater { model:["local","remote"]
                    Rectangle { id:side; required property string modelData; width:(tradeSides.width-12)/2; height:tradeSides.height; color:theme.rowAlt; radius:6
                        property bool mine:modelData==="local"
                        Text { x:10; y:8; text:side.mine ? "Your offer"+(hub.view.localLocked?" · Locked":"") : hub.view.partner+(hub.view.remoteLocked?" · Locked":""); color:theme.ink; font.bold:true; font.pixelSize:12 }
                        Flickable { x:8; y:32; width:parent.width-16; height:parent.height-66; contentHeight:Math.max(height,offerRows.height); clip:true
                            Column { id:offerRows; width:parent.width; spacing:5
                                Repeater { model:side.mine ? hub.view.localOffer||[] : hub.view.remoteOffer||[]
                                    Row { required property var modelData; width:offerRows.width; spacing:6
                                        Image { width:30; height:30; source:hub.icon(modelData); smooth:false }
                                        Text { width:parent.width-38; anchors.verticalCenter:parent.verticalCenter; text:modelData.count+" × "+modelData.name; wrapMode:Text.WordWrap; color:theme.ink; font.pixelSize:11 }
                                    }
                                }
                            }
                            DropArea { anchors.fill:parent; keys:["commerce-item"]; enabled:side.mine&&!hub.view.localLocked&&!hub.view.lockPending&&!hub.view.addPending; onDropped:function(drop){hub.drop("trade");drop.acceptProposedAction()} }
                        }
                        Text { x:10; anchors.bottom:parent.bottom; anchors.bottomMargin:10; text:(side.mine?hub.view.localZeny||0:hub.view.remoteZeny||0)+" Zeny"; color:theme.ink; font.pixelSize:12 }
                    }
                }
            }
            Column { anchors.bottom:parent.bottom; width:parent.width; spacing:6
                Row { spacing:7; visible:!!hub.view.tradeActive
                    Field { id:tradeZeny; width:100; text:"0"; input.validator:IntValidator { bottom:0; top:2147483647 } enabled:!hub.view.localLocked&&!hub.view.lockPending }
                    Button { label:"Offer Zeny"; width:90; enabled:!hub.view.localLocked&&!hub.view.lockPending&&!hub.view.addPending; onClicked:hub.send("tradeZeny",{amount:Number(tradeZeny.text)}) }
                    Button { objectName:"commerceLockTrade"; label:hub.view.localLocked?"Locked":"Lock offer"; width:90; enabled:!hub.view.localLocked&&!hub.view.lockPending&&!hub.view.addPending; onClicked:hub.send("lockTrade") }
                    Button { objectName:"commerceCommitTrade"; label:hub.view.commitPending?"Confirming…":"Confirm trade"; width:110; primary:true; enabled:!!hub.view.localLocked&&!!hub.view.remoteLocked&&!hub.view.commitPending; onClicked:hub.send("commitTrade") }
                    Button { label:"Cancel"; width:70; onClicked:hub.send("close",{to:"trade"}) }
                }
                Status { width:parent.width; height:30 }
            }
        }
    }

    RoCommercePanel {
        id:shop; objectName:"commerce_shop"; kind:"shop"; info:hub.view.shop||({}); caption:"Player shop"; z:5
        Flickable { parent:shop.body; anchors.fill:parent; contentHeight:shopRows.height; clip:true
            Column { id:shopRows; width:parent.width; spacing:8
                Repeater { model:hub.view.shopItems||[]
                    Rectangle { required property var modelData; width:shopRows.width; height:70; color:theme.rowAlt; radius:5
                        Image { x:6; y:8; width:32; height:32; source:hub.icon(modelData) }
                        Text { x:45; y:7; width:parent.width-50; text:modelData.name; elide:Text.ElideRight; color:theme.ink; font.pixelSize:12 }
                        Text { x:45; y:26; text:modelData.price+" Zeny each · "+modelData.count+" available"; color:theme.muted; font.pixelSize:10 }
                        Field { id:buyAmount; x:45; y:41; width:70; height:25; text:"1"; input.validator:IntValidator { bottom:1; top:32767 } }
                        Button { objectName:"commerceBuy_"+modelData.index; x:125; y:41; height:25; label:"Buy"; onClicked:hub.send("buy",{index:modelData.index,amount:Number(buyAmount.text)}) }
                    }
                }
                Status { width:parent.width; height:35 }
            }
        }
    }
    RoCommercePanel {
        id:mail; objectName:"commerce_mail"; kind:"mail"; info:hub.view.mail||({}); caption:"Write mail"; z:6
        Column { parent:mail.body; anchors.fill:parent; spacing:8
            Text { text:"Recipient"; color:theme.muted; font.pixelSize:11 }
            Field { id:recipient; width:parent.width; text:hub.view.mailTo||""; input.maximumLength:23 }
            Text { text:"Subject"; color:theme.muted; font.pixelSize:11 }
            Field { id:mailSubject; width:parent.width; input.maximumLength:39 }
            RoField { width:parent.width; height:150
                TextEdit { id:mailBody; anchors.fill:parent; anchors.margins:8; color:theme.ink; font.pixelSize:12; wrapMode:TextEdit.Wrap; selectByMouse:true; clip:true }
            }
            Button { label:hub.view.mailPending?"Sending…":"Send mail"; primary:true; enabled:!hub.view.mailPending; onClicked:hub.send("mail",{text:recipient.text,subject:mailSubject.text,body:mailBody.text}) }
            Status { width:parent.width; height:32 }
        }
    }
    RoCommercePanel {
        id:equipment; objectName:"commerce_equipment"; kind:"equipment"; info:hub.view.equipment||({}); caption:"Equipment · "+(hub.view.partner||"Player"); z:7
        Flickable { parent:equipment.body; anchors.fill:parent; clip:true; contentHeight:equipmentRows.height
            Column { id:equipmentRows; width:parent.width; spacing:6
                Repeater { model:hub.view.equipmentItems||[]
                    Row { required property var modelData; spacing:8
                        Image { width:32; height:32; source:hub.icon(modelData) }
                        Text { text:modelData.name; color:theme.ink; font.pixelSize:12; anchors.verticalCenter:parent.verticalCenter }
                    }
                }
            }
        }
    }
    RoCommercePanel {
        id:context; objectName:"commerce_playerMenu"; info:hub.view.playerMenu||({}); caption:info.name||"Player"; kind:"playerMenu"; closeAction:"contextClose"; z:100
        property int selected:0
        onVisibleChanged:selected=0
        Keys.onPressed:function(event){var rows=info.actions||[];if(!rows.length)return;if(event.key===Qt.Key_Down){selected=(selected+1)%rows.length;event.accepted=true}else if(event.key===Qt.Key_Up){selected=(selected+rows.length-1)%rows.length;event.accepted=true}else if(event.key===Qt.Key_Return && rows[selected].enabled){hub.send("context",{id:rows[selected].id});event.accepted=true}}
        Column { parent:context.body; anchors.fill:parent; spacing:2
            Repeater { model:context.info.actions||[]
                Button { required property var modelData; required property int index; objectName:"commerceContext"+modelData.id; width:parent.width; height:28; label:modelData.label; enabled:modelData.enabled; primary:context.selected===index; onClicked:hub.send("context",{id:modelData.id}) }
            }
        }
    }

    Rectangle { id:ghost; width:40; height:40; z:2000; visible:hub.dragging; color:theme.rowSelected; radius:5; opacity:0.85
        Image { anchors.centerIn:parent; width:32; height:32; source:hub.dragItem.id?hub.icon(hub.dragItem):"" }
    }
    Rectangle { anchors.fill:parent; visible:hub.pending!==null || !!hub.view.promptKind; z:2001; color:"#50000000"; property bool nativeInteractive:true; MouseArea { anchors.fill:parent } }
    RoFrame { id:quantityDialog; property bool nativeInteractive:true; visible:hub.pending!==null; z:2002; width:320; height:160; anchors.centerIn:parent
        showTitle:false
        RoHeader { width:parent.width; height:32 }
        Keys.onEscapePressed:hub.pending=null
        Text { x:16; y:12; text:"Move items"; font.bold:true; font.pixelSize:14; color:theme.titleText }
        Text { x:16; y:42; width:288; text:hub.pending?hub.pending.item.name+" · up to "+hub.pending.item.count:""; color:theme.ink; font.pixelSize:12; elide:Text.ElideRight }
        TextInput { id:quantity; objectName:"commerceQuantity"; x:20; y:72; width:100; height:26; color:theme.ink; font.pixelSize:16; selectByMouse:true; validator:IntValidator { bottom:1; top:hub.pending?Math.min(32767,hub.pending.item.count):1 } }
        Button { objectName:"commerceQuantityConfirm"; x:16; y:116; label:"Move"; primary:true; enabled:quantity.acceptableInput; onClicked:{var request=hub.pending;hub.pending=null;hub.send(request.to==="trade"?"offerTrade":"transfer",{from:request.from,to:request.to,index:request.item.index,amount:Number(quantity.text)})} }
        Button { x:126; y:116; label:"Cancel"; onClicked:hub.pending=null }
    }
    RoFrame { property bool nativeInteractive:true; visible:!!hub.view.promptKind; z:2003; width:280; height:132; anchors.centerIn:parent
        focus:visible
        Keys.onEscapePressed:hub.send("prompt",{amount:0})
        Rectangle { x:14; y:28; width:parent.width-28; height:52; radius:2; color:theme.fieldFill; border.width:1; border.color:theme.buttonBorder }
        Text { x:12; y:2; text:hub.view.promptKind==="guild"?"Guild Invitation":hub.view.promptKind==="alliance"?"Guild Alliance":"Friend Request"; color:theme.titleText; font.family:theme.fontFamily; font.pixelSize:theme.titleSize; font.weight:Font.DemiBold }
        Text { x:20; y:34; width:parent.width-40; height:42; text:hub.view.promptName||""; color:theme.ink; font.family:theme.fontFamily; font.pixelSize:11; wrapMode:Text.WordWrap; horizontalAlignment:Text.AlignHCenter; verticalAlignment:Text.AlignVCenter }
        RoChromeButton { x:parent.width-18; y:3; width:14; height:12; label:"X"; titleControl:true
            MouseArea { anchors.fill:parent; onClicked:hub.send("prompt",{amount:0}) }
        }
        Repeater { model:["Accept","Decline"]
            RoChromeButton { required property string modelData; required property int index; objectName:index===0?"commercePromptAccept":"commercePromptDecline"; x:index===0?26:162; y:96; width:92; height:24; label:modelData; labelSize:10; labelBold:true
                MouseArea { anchors.fill:parent; hoverEnabled:true; onEntered:parent.hovered=true; onExited:{parent.hovered=false;parent.pressed=false} onPressed:parent.pressed=true; onReleased:parent.pressed=false; onClicked:hub.send("prompt",{amount:parent.index===0?1:0}) }
            }
        }
    }
}
