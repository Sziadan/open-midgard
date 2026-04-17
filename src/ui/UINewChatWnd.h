#pragma once

#include "UIWindow.h"

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

class UINewChatWnd : public UIWindow {
public:
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
    const std::vector<ChatLine>& GetLines() const;
    const std::vector<ChatLine>& GetVisibleLines() const;
    const std::string& GetInputText() const;
    const std::vector<std::string>& GetInputHistory() const;
    ChatScrollBarState GetScrollBarState() const;
    void ClearLines();
    bool HandleKeyDown(int virtualKey);
    bool HandleChar(char c);
    bool IsInputActive() const;
    void RestorePersistentState(const std::vector<std::string>& inputHistory,
        const std::string& inputText,
        bool inputActive,
        int scrollLineOffset);

private:
    void Layout();
    void RefreshVisibleLines(u32 nowTick);
    void SetInputActive(bool active);
    bool SubmitInput();
    void AddInputHistory(const std::string& text);
    void AdjustScroll(int lineDelta);
    void ClampScrollOffset();

    std::vector<ChatLine> m_lines;
    std::vector<ChatLine> m_visibleLines;
    std::vector<std::string> m_inputHistory;
    std::string m_inputText;
    std::string m_historyDraft;
    u32 m_lastDrawTick;
    int m_inputActive;
    int m_historyBrowseIndex;
    int m_scrollLineOffset;
    int m_firstVisibleLineIndex;
    int m_dragStartGlobalX;
    int m_dragStartGlobalY;
    int m_dragStartWindowX;
    int m_dragStartWindowY;
    int m_dragArmed;
    int m_isDragging;
};
