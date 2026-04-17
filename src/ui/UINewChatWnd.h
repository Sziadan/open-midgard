#pragma once

#include "UIWindow.h"

#include <array>
#include <vector>

struct ChatLine {
    std::string text;
    u32 color;
    u8 channel;
    u32 tick;
};

struct ChatScrollBarState {
    int visible = 0;
    int totalLines = 0;
    int firstVisibleLine = 0;
    int visibleLineCount = 0;
};

struct ChatTabDisplay {
    int id = 0;
    const char* label = "";
    int active = 0;
    u32 channelMask = 0;
};

struct ChatFilterOptionDisplay {
    int id = 0;
    const char* label = "";
    int enabled = 0;
};

class UINewChatWnd : public UIWindow {
public:
    enum ActiveInputField {
        InputField_None = 0,
        InputField_WhisperTarget = 1,
        InputField_Message = 2,
    };

    UINewChatWnd();
    ~UINewChatWnd() override;

    bool IsFrameWnd() override { return true; }
    void OnProcess() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnWheel(int delta) override;
    void StoreInfo() override;

    void AddChatLine(const char* text, u32 color, u8 channel, u32 tick);
    bool HandleQtMouseDown(int x, int y);
    bool HandleQtMouseMove(int x, int y);
    bool HandleQtMouseUp(int x, int y);
    bool HandleQtWheel(int delta, int x, int y);
    bool IsQtInteractionPoint(int x, int y) const;
    bool IsQtMainPanelPoint(int x, int y) const;
    bool IsQtPointerCaptured() const;
    void ClearInputFocus();
    const std::vector<ChatLine>& GetLines() const;
    const std::vector<ChatLine>& GetVisibleLines() const;
    const std::string& GetWhisperTargetText() const;
    const std::string& GetInputText() const;
    const std::vector<std::string>& GetInputHistory() const;
    ChatScrollBarState GetScrollBarState() const;
    int GetFontPixelSize() const;
    int GetWindowOpacityPercent() const;
    bool IsConfigVisible() const;
    bool GetConfigRectForQt(int* x, int* y, int* width, int* height) const;
    int GetTabCount() const;
    bool GetTabDisplay(int index, ChatTabDisplay* outData) const;
    int GetFilterOptionCount() const;
    bool GetFilterOptionDisplay(int index, ChatFilterOptionDisplay* outData) const;
    void ClearLines();
    bool HandleKeyDown(int virtualKey);
    bool HandleChar(char c);
    bool IsWhisperTargetActive() const;
    bool IsMessageInputActive() const;
    bool IsInputActive() const;
    void RestorePersistentState(const std::vector<std::string>& inputHistory,
        const std::string& whisperTargetText,
        const std::string& inputText,
        int activeInputField,
        int scrollLineOffset);

private:
    void LoadSettings(int* outWindowWidth, int* outWindowHeight);
    void SaveSettings() const;
    void Layout();
    void RefreshVisibleLines(u32 nowTick);
    void ResetTabDefaults();
    void ResetTabDefault(int index);
    void SetActiveTab(int index);
    void SetConfigVisible(bool visible);
    void SetWindowOpacityPercent(int value);
    void SetActiveInputField(ActiveInputField field);
    std::string* GetActiveInputBuffer();
    bool SubmitInput();
    void AddInputHistory(const std::string& text);
    void AdjustScroll(int lineDelta);
    void ClampScrollOffset();
    void UpdateWindowOpacityFromPointer(int x, const RECT& trackRect);
    bool HandlePointerDown(int x, int y);
    bool HandlePointerMove(int x, int y);
    bool HandlePointerUp(int x, int y);
    bool HandlePointerWheel(int delta, int x, int y);

    std::vector<ChatLine> m_lines;
    std::vector<ChatLine> m_visibleLines;
    std::vector<std::string> m_inputHistory;
    std::string m_whisperTargetText;
    std::string m_inputText;
    std::string m_historyDraft;
    u32 m_lastDrawTick;
    ActiveInputField m_activeInputField;
    int m_historyBrowseIndex;
    int m_scrollLineOffset;
    int m_firstVisibleLineIndex;
    int m_activeTab;
    int m_fontPixelSize;
    int m_windowOpacityPercent;
    int m_configVisible;
    int m_dragStartGlobalX;
    int m_dragStartGlobalY;
    int m_dragStartWindowX;
    int m_dragStartWindowY;
    int m_dragStartWindowW;
    int m_dragStartWindowH;
    int m_dragArmed;
    int m_isDragging;
    int m_resizeEdges;
    int m_transparencyDragActive;
    std::array<u32, 4> m_tabChannelMasks;
};
