import QtQuick 2.15

RoFrame {
    id: panel
    objectName: "guildWindowPanel"
    property bool nativeInteractive: true
    RoTheme { id: theme }
    property var guildData: uiState.guildWindow
    property int selectedCharacter: 0
    property string confirmation: ""
    property string feedback: ""
    visible: !!guildData.visible
    x: guildData.x || 40
    y: guildData.y || 65
    width: guildData.width || 720
    height: guildData.height || 470
    z: 30
    onVisibleChanged: { confirmation = ""; feedback = "" }
    focus: visible
    Keys.onEscapePressed: function(event) { uiState.guildAction("close", {}); event.accepted = true }

    component GuildButton: RoTextButton {
        signal clicked()
        width: 94; height: 26
        labelSize: 11
        MouseArea { anchors.fill: parent; enabled: parent.enabled; hoverEnabled: true
            onEntered: parent.hovered = true
            onExited: { parent.hovered = false; parent.pressed = false }
            onPressed: parent.pressed = true
            onReleased: parent.pressed = false
            onClicked: parent.clicked()
        }
    }
    MouseArea {
        objectName: "guildDragHeader"
        x: 0; y: 0; width: parent.width - 30; height: 26
        property point start
        property point origin
        onPressed: function(mouse) { start = mapToItem(panel.parent, mouse.x, mouse.y); origin = Qt.point(panel.x, panel.y) }
        onPositionChanged: function(mouse) {
            if (pressed) { var p = mapToItem(panel.parent, mouse.x, mouse.y); uiState.guildAction("move", {x: origin.x + p.x - start.x, y: origin.y + p.y - start.y}) }
        }
    }
    Text { objectName: "guildWindowTitle"; x: 12; y: 2; text: "Guild"; color: theme.titleText; font.family: theme.fontFamily; font.pixelSize: 12; font.bold: true }
    GuildButton { objectName: "guildCloseButton"; x: parent.width - 28; y: 0; width: 26; height: 18; label: "×"; onClicked: uiState.guildAction("close", {}) }

    Row {
        x: 12; y: 30; spacing: 4
        Repeater {
            model: ["Overview", "Members", "Positions", "Skills", "Relations", "Expulsions"]
            GuildButton {
                required property int index
                required property string modelData
                objectName: "guildTab" + index
                width: 108; label: modelData; primary: (panel.guildData.tab || 0) === index
                onClicked: { panel.feedback = ""; uiState.guildAction("tab", {tab: index}) }
            }
        }
    }
    Text {
        visible: !panel.guildData.joined
        anchors.centerIn: parent
        width: parent.width - 64
        text: "You are not in a guild.\n\nCreate one with /guild \"Guild Name\", or accept a guild invitation."
        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
        color: theme.muted; font.family: theme.fontFamily; font.pixelSize: 14
    }
    Item {
        x: 14; y: 68; width: parent.width - 28; height: parent.height - 116
        visible: !!panel.guildData.joined
        Flickable {
            id: guildScroll
            objectName: "guildScroll"
            anchors.fill: parent; clip: true; contentHeight: content.height
            Column {
                id: content; width: parent.width; spacing: 10
                Column {
                    visible: (panel.guildData.tab || 0) === 0
                    width: parent.width; spacing: 10
                    Row {
                        spacing: 14
                        Rectangle { width: 64; height: 64; radius: 6; color: theme.rowAlt
                            objectName: "guildEmblemButton"
                            border.width: emblemMouse.containsMouse ? 1 : 0
                            border.color: theme.gold
                            Image { anchors.centerIn: parent; width: 48; height: 48; source: panel.guildData.emblem || ""; smooth: false }
                            Text { anchors.centerIn: parent; visible: !panel.guildData.emblem; text: panel.guildData.isMaster ? "+\nEmblem" : "Emblem"; horizontalAlignment: Text.AlignHCenter; color: theme.muted; font.pixelSize: 11 }
                            MouseArea { id: emblemMouse; anchors.fill: parent; enabled: !!panel.guildData.isMaster; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: uiState.guildAction("emblem", {})
                            }
                            Text { visible: emblemMouse.containsMouse; anchors.top: parent.bottom; anchors.topMargin: 2; text: "Upload emblem"; color: theme.ink; font.pixelSize: 10 }
                        }
                        Column { spacing: 6
                            Text { text: panel.guildData.name || "Guild"; font.family: theme.fontFamily; font.pixelSize: 22; font.bold: true; color: theme.ink }
                            Text { text: "Level " + (panel.guildData.level || 0) + "   ·   " + (panel.guildData.online || 0) + " online / " + (panel.guildData.capacity || 0) + " members"; color: theme.muted; font.pixelSize: 12 }
                            Text { text: "Guild master: " + (panel.guildData.master || "—") + "     Average level: " + (panel.guildData.averageLevel || 0); color: theme.ink; font.pixelSize: 12 }
                        }
                    }
                    RoGauge { width: parent.width; height: 18; value: panel.guildData.exp || 0; maximum: panel.guildData.nextExp || 1; showText: false; fillColor: theme.gold }
                    Text { text: "Guild experience  " + (panel.guildData.exp || 0) + " / " + (panel.guildData.nextExp || 0) + "     ·     Territory: " + (panel.guildData.territory || "None"); color: theme.muted; font.pixelSize: 11 }
                    Text { text: "Guild notice"; color: theme.ink; font.bold: true; font.pixelSize: 13 }
                    RoField { width: parent.width; height: 30
                        TextInput { id: noticeSubject; objectName: "guildNoticeSubject"; anchors.fill: parent; anchors.margins: 7; text: panel.guildData.subject || ""; readOnly: !panel.guildData.isMaster; color: theme.ink; font.family: theme.fontFamily; font.pixelSize: 12; clip: true; selectByMouse: true; maximumLength: 59 }
                    }
                    RoField { width: parent.width; height: 84
                        TextEdit { id: noticeBody; objectName: "guildNoticeBody"; anchors.fill: parent; anchors.margins: 7; text: panel.guildData.notice || ""; readOnly: !panel.guildData.isMaster; color: theme.ink; font.family: theme.fontFamily; font.pixelSize: 12; wrapMode: TextEdit.Wrap; selectByMouse: true; clip: true }
                    }
                    GuildButton { visible: !!panel.guildData.isMaster; label: "Save notice"; onClicked: { uiState.guildAction("notice", {subject: noticeSubject.text, body: noticeBody.text}); panel.feedback = "Notice requested; waiting for the server." } }
                    Text { visible: !panel.guildData.loaded; text: "Loading guild information…"; color: theme.muted; font.pixelSize: 12 }
                }
                Column {
                    visible: panel.guildData.tab === 1
                    width: parent.width; spacing: 2
                    Row { height: 26; x: 6; spacing: 6
                        Repeater { model: [{label:"CHARACTER / JOB",size:216},{label:"RANK",size:166},{label:"LEVEL",size:44},{label:"STATUS",size:72},{label:"CONTRIBUTION",size:100}]
                            Text { required property var modelData; width: modelData.size; text: modelData.label; color: theme.muted; font.pixelSize: 10; font.bold: true }
                        }
                    }
                    Repeater { model: panel.guildData.members || []
                        Rectangle { required property var modelData; required property int index
                            width: content.width; height: 40; radius: 4; color: panel.selectedCharacter === modelData.character ? theme.rowSelected : index % 2 ? theme.rowAlt : theme.bodyTop
                            Row { anchors.fill: parent; anchors.margins: 6; spacing: 6
                                Column { width: 216
                                    Text { text: modelData.name; color: theme.ink; font.pixelSize: 12; font.bold: true }
                                    Text { text: modelData.job; color: theme.muted; font.pixelSize: 10 }
                                }
                                Text { width: 166; text: modelData.rank; elide: Text.ElideRight; color: theme.ink; font.pixelSize: 12 }
                                Text { width: 44; text: modelData.level; color: theme.ink; font.pixelSize: 12 }
                                Text { width: 72; text: modelData.online ? "Online" : "Offline"; color: modelData.online ? "#237a51" : theme.muted; font.pixelSize: 12 }
                                Text { text: modelData.exp; color: theme.ink; font.pixelSize: 12 }
                            }
                            MouseArea { anchors.fill: parent; onClicked: panel.selectedCharacter = modelData.character }
                        }
                    }
                    Row { visible: !!panel.guildData.isMaster; spacing: 8; height: 36
                        Text { text: "Assign selected member to rank #"; color: theme.muted; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: 12 }
                        RoField { width: 44; height: 26
                            TextInput { id: memberRank; anchors.centerIn: parent; width: 32; text: "1"; validator: IntValidator { bottom: 0; top: 19 } color: theme.ink; selectByMouse: true }
                        }
                        GuildButton { label: "Assign rank"; enabled: panel.selectedCharacter !== 0; onClicked: uiState.guildAction("memberPosition", {character: panel.selectedCharacter, position: Number(memberRank.text)}) }
                    }
                    Text { visible: !(panel.guildData.members || []).length; text: "No member list received yet. Use Refresh to request it."; color: theme.muted }
                }
                Column {
                    visible: panel.guildData.tab === 2; width: parent.width; spacing: 4
                    Text { text: "Positions and experience tax"; color: theme.ink; font.bold: true; font.pixelSize: 14; height: 24 }
                    Repeater { model: panel.guildData.positions || []
                        Rectangle { required property var modelData; width: content.width; height: 38; color: theme.rowAlt; radius: 4
                            property bool invite: modelData.invite
                            property bool expel: modelData.expel
                            id: positionRow
                            Row { x: 6; y: 6; spacing: 8
                                Text { width: 26; text: "#" + modelData.id; color: theme.muted; anchors.verticalCenter: parent.verticalCenter }
                                RoField { width: 228; height: 26; TextInput { id: rankName; anchors.fill: parent; anchors.margins: 5; text: modelData.name; readOnly: !panel.guildData.isMaster; maximumLength: 23; color: theme.ink; selectByMouse: true; clip: true } }
                                GuildButton { label: positionRow.invite ? "Invite ✓" : "Invite —"; enabled: !!panel.guildData.isMaster; onClicked: positionRow.invite = !positionRow.invite }
                                GuildButton { label: positionRow.expel ? "Expel ✓" : "Expel —"; enabled: !!panel.guildData.isMaster; onClicked: positionRow.expel = !positionRow.expel }
                                RoField { width: 42; height: 26; TextInput { id: rankTax; anchors.centerIn: parent; width: 32; text: modelData.tax; readOnly: !panel.guildData.isMaster; validator: IntValidator { bottom: 0; top: 50 } color: theme.ink; selectByMouse: true } }
                                Text { text: "%"; color: theme.muted; anchors.verticalCenter: parent.verticalCenter }
                                GuildButton { width: 64; label: "Save"; visible: !!panel.guildData.isMaster; onClicked: uiState.guildAction("position", {id: modelData.id, name: rankName.text, invite: positionRow.invite, expel: positionRow.expel, tax: Number(rankTax.text)}) }
                            }
                        }
                    }
                }
                Column {
                    visible: panel.guildData.tab === 3; width: parent.width; spacing: 5
                    Text { text: "Guild skill points: " + (panel.guildData.skillPoints || 0); color: theme.ink; font.bold: true; font.pixelSize: 14; height: 26 }
                    Repeater { model: panel.guildData.skills || []
                        Rectangle { required property var modelData; width: content.width; height: 42; color: theme.rowAlt; radius: 4
                            Text { x: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.name; color: theme.ink; font.pixelSize: 13 }
                            Text { x: parent.width - 178; anchors.verticalCenter: parent.verticalCenter; text: "Level " + modelData.level; color: theme.muted }
                            GuildButton { x: parent.width - width - 8; y: 8; label: "Level up"; visible: !!panel.guildData.isMaster; enabled: !!modelData.upgrade && panel.guildData.skillPoints > 0; onClicked: uiState.guildAction("skill", {id: modelData.id}) }
                        }
                    }
                    Text { visible: !(panel.guildData.skills || []).length; text: "No guild skills received yet."; color: theme.muted }
                }
                Column {
                    visible: panel.guildData.tab === 4; width: parent.width; spacing: 6
                    Text { text: "Alliances and opposition"; font.bold: true; color: theme.ink; font.pixelSize: 14; height: 26 }
                    Repeater { model: panel.guildData.relations || []
                        Text { required property var modelData; text: (modelData.hostile ? "Opposition   ·   " : "Ally   ·   ") + modelData.name; color: modelData.hostile ? theme.warning : theme.ink; font.pixelSize: 13; height: 28 }
                    }
                    Text { visible: !(panel.guildData.relations || []).length; text: "No alliances or hostile guilds."; color: theme.muted }
                }
                Column {
                    visible: panel.guildData.tab === 5; width: parent.width; spacing: 6
                    Text { text: "Expulsion history"; font.bold: true; color: theme.ink; font.pixelSize: 14; height: 26 }
                    Repeater { model: panel.guildData.bans || []
                        Text { required property var modelData; text: modelData.name + "   ·   " + modelData.reason; color: theme.ink; wrapMode: Text.Wrap; width: content.width; font.pixelSize: 12 }
                    }
                    Text { visible: !(panel.guildData.bans || []).length; text: "No expelled members."; color: theme.muted }
                }
            }
        }
        RoScrollBar {
            visible: guildScroll.contentHeight > guildScroll.height
            anchors.right: parent.right; width: 7; height: parent.height
            thumbWidth: width
            thumbHeight: Math.max(20, height * guildScroll.height / Math.max(1, guildScroll.contentHeight))
            thumbY: Math.max(0, Math.min(height - thumbHeight, guildScroll.contentY / Math.max(1, guildScroll.contentHeight - guildScroll.height) * (height - thumbHeight)))
        }
    }
    Text { x: 14; y: parent.height - 36; width: parent.width - 240; text: panel.feedback; color: theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
    GuildButton { x: parent.width - 214; y: parent.height - 36; label: "Leave guild"; visible: !!panel.guildData.joined && !panel.guildData.isMaster; onClicked: panel.confirmation = "leave" }
    GuildButton { x: parent.width - 108; y: parent.height - 36; label: "Refresh"; onClicked: uiState.guildAction("refresh", {}) }
    Rectangle {
        visible: panel.confirmation !== ""; anchors.fill: parent; color: "#b0f1f0ec"; radius: 7
        MouseArea { anchors.fill: parent }
        RoFrame { width: 360; height: 158; anchors.centerIn: parent; showTitle: false
            Text { x: 18; y: 16; text: "Leave this guild?"; color: theme.ink; font.pixelSize: 16; font.bold: true }
            RoField { x: 18; y: 52; width: 324; height: 28
                TextInput { id: leaveReason; anchors.fill: parent; anchors.margins: 6; text: "Leaving the guild"; maximumLength: 39; color: theme.ink; selectByMouse: true }
            }
            GuildButton { x: 142; y: 110; label: "Cancel"; onClicked: panel.confirmation = "" }
            GuildButton { x: 248; y: 110; label: "Leave"; primary: true; onClicked: { uiState.guildAction("leave", {confirmed: true, reason: leaveReason.text}); panel.confirmation = ""; panel.feedback = "Leave requested; waiting for the server." } }
        }
    }
}
