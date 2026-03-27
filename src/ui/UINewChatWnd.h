#pragma once

#include "UIWindow.h"

#include <vector>

struct ChatLine {
    std::string text;
    u32 color;
    u8 channel;
    u32 tick;
};

class UINewChatWnd : public UIWindow {
public:
    UINewChatWnd();
    ~UINewChatWnd() override;

    void OnProcess() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;

    void AddChatLine(const char* text, u32 color, u8 channel, u32 tick);
    const std::vector<ChatLine>& GetLines() const;
    const std::vector<ChatLine>& GetVisibleLines() const;
    void ClearLines();
    bool HandleKeyDown(int virtualKey);
    bool HandleChar(char c);
    bool IsInputActive() const;

private:
    void Layout();
    void RefreshVisibleLines(u32 nowTick);
    void SetInputActive(bool active);
    bool SubmitInput();
    void AddInputHistory(const std::string& text);

    std::vector<ChatLine> m_lines;
    std::vector<ChatLine> m_visibleLines;
    std::vector<std::string> m_inputHistory;
    std::string m_inputText;
    std::string m_historyDraft;
    u32 m_lastDrawTick;
    int m_inputActive;
    int m_historyBrowseIndex;
};
