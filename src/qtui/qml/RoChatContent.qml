import QtQuick

Item {
    id: chat
    property int fontPixelSize: 13
    property color ink: "#ece1cc"

    function escaped(value) {
        return String(value).replace(/&/g, "&amp;").replace(/</g, "&lt;")
            .replace(/>/g, "&gt;").replace(/\n/g, "<br>")
    }
    function updateHistory() {
        // Keep the document stable while the player is selecting/copying it.
        if (history.selectedText.length > 0) return
        const atBottom = transcript.contentY >= transcript.contentHeight - transcript.height - 4
        const lines = uiState.chatWindowLines || []
        let html = ""
        for (const line of lines)
            html += '<p style="margin:0 0 2px 0;color:' + line.color + '">' + escaped(line.text) + '</p>'
        if (history.text !== html) history.text = html
        if (atBottom) Qt.callLater(function() { transcript.contentY = Math.max(0, transcript.contentHeight - transcript.height) })
    }

    Rectangle {
        id: pane
        objectName: "chatHistoryPane"
        width: parent.width
        height: parent.height - 40
        color: "#14110e"
        radius: 4
        border.color: "#806c49"
        clip: true

        Flickable {
            id: transcript
            objectName: "chatTranscript"
            anchors.fill: parent
            anchors.margins: 5
            anchors.rightMargin: scroll.visible ? 12 : 5
            contentWidth: width
            contentHeight: Math.max(height, history.contentHeight)
            boundsBehavior: Flickable.StopAtBounds
            acceptedButtons: Qt.NoButton
            clip: true
            TextEdit {
                id: history
                objectName: "chatHistoryEditor"
                width: transcript.width
                y: Math.max(0, transcript.height - contentHeight)
                height: Math.max(transcript.height, contentHeight)
                readOnly: true
                selectByMouse: true
                selectByKeyboard: true
                persistentSelection: true
                activeFocusOnPress: true
                textFormat: TextEdit.RichText
                wrapMode: TextEdit.Wrap
                color: chat.ink
                selectionColor: "#496b95"
                selectedTextColor: "white"
                font.family: "Noto Sans"
                font.pixelSize: chat.fontPixelSize
                font.contextFontMerging: true
                onActiveFocusChanged: if (activeFocus) uiState.focusChat(0)
                onSelectedTextChanged: if (selectedText.length === 0) Qt.callLater(chat.updateHistory)
                Keys.onReturnPressed: uiState.chatKey(13)
                Keys.onEnterPressed: uiState.chatKey(13)
                Keys.onEscapePressed: { deselect(); focus = false; uiState.focusChat(0) }
                onCursorRectangleChanged: {
                    if (!activeFocus || selectedText.length === 0) return
                    const top = y + cursorRectangle.y
                    if (top < transcript.contentY) transcript.contentY = top
                    else if (top + cursorRectangle.height > transcript.contentY + transcript.height)
                        transcript.contentY = top + cursorRectangle.height - transcript.height
                }
            }
        }
        RoScrollBar {
            id: scroll
            x: parent.width - 9
            y: 5
            width: 6
            height: parent.height - 10
            visible: transcript.contentHeight > transcript.height
            thumbWidth: width
            thumbHeight: Math.max(18, height * transcript.height / transcript.contentHeight)
            thumbY: (height - thumbHeight) * transcript.contentY / Math.max(1, transcript.contentHeight - transcript.height)
            MouseArea {
                anchors.fill: parent
                property real grab: 0
                onPressed: function(mouse) { grab = mouse.y - scroll.thumbY; if (grab < 0 || grab > scroll.thumbHeight) grab = scroll.thumbHeight / 2; moveThumb(mouse.y) }
                onPositionChanged: function(mouse) { if (pressed) moveThumb(mouse.y) }
                function moveThumb(y) {
                    transcript.contentY = Math.max(0, Math.min(1, (y - grab) / Math.max(1, scroll.height - scroll.thumbHeight))) * Math.max(0, transcript.contentHeight - transcript.height)
                }
            }
        }
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
        y: parent.height - height
        width: 126
        fieldId: 1
        sourceText: uiState.chatWindowWhisperTargetText
        inputActive: uiState.chatWindowWhisperInputActive
    }
    ChatInput {
        id: message
        objectName: "chatMessageField"
        x: 134
        y: parent.height - height
        width: parent.width - x
        sourceText: uiState.chatWindowInputText
        inputActive: uiState.chatWindowMessageInputActive
    }
    Connections {
        target: uiState
        function onChatWindowLinesChanged() { chat.updateHistory() }
    }
    Component.onCompleted: updateHistory()
}
