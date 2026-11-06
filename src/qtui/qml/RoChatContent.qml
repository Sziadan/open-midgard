import QtQuick

Item {
    id: chat
    property int fontPixelSize: 13
    property color ink: "#ece1cc"
    property var sourceRects: ({})
    property bool expanded: true

    Rectangle {
        id: pane
        objectName: "chatHistoryPane"
        readonly property var bounds: chat.sourceRects.history || ({})
        x: bounds.x || 0; y: bounds.y || 0
        width: bounds.width || 0; height: bounds.height || 0
        visible: chat.expanded
        color: "#14110e"; border.color: "#806c49"; clip: true
        Item {
            id: transcript
            objectName: "chatTranscript"
            anchors.fill: parent
            anchors.rightMargin: (uiState.chatWindowScrollBar.visible || false) ? 13 : 0
            clip: true
            Repeater {
                model: uiState.chatWindowLines || []
                delegate: Text {
                    required property var modelData
                    required property int index
                    objectName: "sourceChatRow" + index
                    x: 2; y: index * 14 + 3
                    width: transcript.width - 2; height: 18
                    text: modelData.text; textFormat: Text.PlainText
                    color: modelData.color
                    font.family: "Noto Sans"; font.pixelSize: chat.fontPixelSize
                    font.contextFontMerging: true
                    wrapMode: Text.NoWrap; clip: true
                }
            }
        }
    }
    Rectangle {
        readonly property var bounds: chat.sourceRects.scrollTrack || ({})
        x: bounds.x || 0; y: bounds.y || 0
        width: bounds.width || 0; height: bounds.height || 0
        visible: chat.expanded && (uiState.chatWindowScrollBar.visible || false)
        color: "#404040"
    }
    Rectangle {
        readonly property var bounds: chat.sourceRects.scrollThumb || ({})
        x: bounds.x || 0; y: bounds.y || 0
        width: bounds.width || 0; height: bounds.height || 0
        visible: chat.expanded && (uiState.chatWindowScrollBar.visible || false)
        color: "#b8b8b8"
    }

    component ChatInput: RoField {
        id: field
        property int fieldId: 2
        property string sourceText: ""
        property bool inputActive: false
        property alias editor: edit
        height: 28
        emphasize: true
        editing: inputActive
        TextInput {
            id: edit
            objectName: field.fieldId === 1 ? "chatWhisperEditor" : "chatMessageEditor"
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            verticalAlignment: TextInput.AlignVCenter
            color: "#241e15"
            font.family: "Noto Sans"
            font.pixelSize: chat.fontPixelSize
            font.contextFontMerging: true
            selectionColor: "#496b95"
            selectedTextColor: "white"
            selectByMouse: true
            persistentSelection: true
            clip: true
            maximumLength: field.fieldId === 1 ? 24 : 180
            onActiveFocusChanged: if (activeFocus) uiState.focusChat(field.fieldId)
            onTextEdited: {
                const accepted = uiState.editChat(field.fieldId, text)
                if (accepted !== text) text = accepted
            }
            Keys.onReturnPressed: { if (!inputMethodComposing) uiState.chatKey(13) }
            Keys.onEnterPressed: { if (!inputMethodComposing) uiState.chatKey(13) }
            Keys.onEscapePressed: uiState.chatKey(27)
            Keys.onTabPressed: uiState.chatKey(9)
            Keys.onBacktabPressed: uiState.chatKey(9)
            Keys.onUpPressed: { if (!inputMethodComposing) uiState.chatKey(38) }
            Keys.onDownPressed: { if (!inputMethodComposing) uiState.chatKey(40) }
        }
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: "To"
            color: "#746b5c"
            visible: field.fieldId === 1 && edit.text.length === 0 && !edit.activeFocus
            font.family: "Noto Sans"
            font.pixelSize: chat.fontPixelSize
        }
        onSourceTextChanged: if (edit.text !== sourceText) edit.text = sourceText
        onInputActiveChanged: {
            if (inputActive) edit.forceActiveFocus()
            else edit.focus = false
        }
        Component.onCompleted: { edit.text = sourceText; if (inputActive) edit.forceActiveFocus() }
    }
    ChatInput {
        id: whisper
        objectName: "chatWhisperField"
        x: chat.sourceRects.whisper ? chat.sourceRects.whisper.x : 0
        y: chat.sourceRects.whisper ? chat.sourceRects.whisper.y : parent.height - height
        width: chat.sourceRects.whisper ? chat.sourceRects.whisper.width : 126
        height: chat.sourceRects.whisper ? chat.sourceRects.whisper.height : 28
        fieldId: 1
        sourceText: uiState.chatWindowWhisperTargetText
        inputActive: uiState.chatWindowWhisperInputActive
    }
    ChatInput {
        id: message
        objectName: "chatMessageField"
        x: chat.sourceRects.message ? chat.sourceRects.message.x : 134
        y: chat.sourceRects.message ? chat.sourceRects.message.y : parent.height - height
        width: chat.sourceRects.message ? chat.sourceRects.message.width : parent.width - x
        height: chat.sourceRects.message ? chat.sourceRects.message.height : 28
        sourceText: uiState.chatWindowInputText
        inputActive: uiState.chatWindowMessageInputActive
    }
}
