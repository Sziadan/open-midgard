#pragma once

#include "UIChatRoomSettingsWnd.h"
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class UIEditCtrl;
class QInputMethodEvent;
class QInputMethodQueryEvent;

// Text prompt used by the two source ground-skill windows (IDs 84 and 101).
class UIGroundSkillTextWnd : public UIWindow {
public:
    explicit UIGroundSkillTextWnd(int windowId);
    ~UIGroundSkillTextWnd() override = default;

    const std::array<int,4>& GetValues() const { return m_values; }
    UIEditCtrl* GetEditCtrl() const { return m_editCtrl; }
    const std::string& GetCaption() const { return m_caption; }
    const std::string& GetPrompt() const { return m_prompt; }
    const std::string& GetFrameUri(int index) const { return m_frameUris.at(static_cast<std::size_t>(index)); }
    const shopui::BitmapPixels& GetFrameBitmap(int index) const { return m_frame.at(static_cast<std::size_t>(index)); }
    const shopui::BitmapPixels& GetColorChip() const { return m_colorChip; }
    u32 GetColorChipRgb(int x,int y) const;
    bool IsFrameDragging() const { return m_dragging; }
    const RoomSettingsButton* GetButton(int id) const;
    int GetButtonState(int id) const;

    void Move(int x,int y) override;
    void OnDraw() override;
    void OnLBtnDown(int x,int y) override;
    void OnLBtnUp(int x,int y) override;
    void OnMouseMove(int x,int y) override;
    void OnMouseHover(int x,int y) override;
    void OnChar(char c) override;
    void OnUtf16Character(char16_t codeUnit);
    void CommitText(std::u16string_view text);
    void SetComposition(std::u16string_view text, int cursor = -1, bool visible = true);
    void CancelComposition();
    struct ImeCandidatePage {
        std::vector<std::string> entries;
        int selection = -1;
        unsigned page = 0;
        unsigned lastPage = 0;
    };
    void SetImeCandidates(ImeCandidatePage page);
    void DrawImeCandidates(HDC dc, RECT client);
    void RecordImeKeystroke(int key);
    void RetainLastImeKeystroke();
    void ClearImeKeystrokes();
    std::optional<std::array<std::string, 4>> ImeKeystrokeCells() const;
    void DrawImeKeystrokes(HDC dc, RECT client);
#if defined(_WIN32)
    bool HandleNativeImeCandidates(HWND hwnd, WPARAM event, LPARAM lists);
#endif
#if RO_ENABLE_QT6_UI
    void ApplyInputMethod(const QInputMethodEvent& event);
    void QueryInputMethod(QInputMethodQueryEvent& event);
#endif
    void OnKeyDown(int key) override;
    bool HandleEditKey(int key, bool extendSelection);
    bool HandleClipboardKey(int key, bool shift, bool control, HWND owner);
    std::optional<std::string> CopySelectionBytes();
    void PasteSelectionBytes(std::string_view bytes);
    bool CanReceiveKeyInput() const override { return true; }
    bool IsFrameWnd() override { return true; }
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;

private:
    friend class GroundSkillTextEditCtrl;
    friend class UIWindowMgr;
    void SyncSelection();
    void UpdateText(std::string text, std::size_t cursor);
    void ReplaceSelection(std::string_view insertion);
    void ApplyEdit(std::string text, std::size_t cursor);
    void FocusEdit();
    void ActivateButton(int id);
    void CancelPointerCapture();

    std::array<int,4> m_values{};
    UIEditCtrl* m_editCtrl=nullptr; // Owned by UIWindow::m_children.
    RoomSettingsButton m_ok;
    RoomSettingsButton m_cancel;
    std::array<shopui::BitmapPixels,6> m_frame;
    std::array<std::string,6> m_frameUris;
    shopui::BitmapPixels m_colorChip;
    bool m_dragging=false;
    int m_dragStartX=0,m_dragStartY=0,m_dragOriginX=0,m_dragOriginY=0;
    std::string m_caption;
    std::string m_prompt;
    std::string m_selectionText;
    std::string m_preedit;
    ImeCandidatePage m_candidates;
    std::vector<int> m_imeKeystrokes;
    std::optional<std::array<shopui::BitmapPixels, 8>> m_candidateFrame;
#if defined(_WIN32)
    bool m_systemCandidates = false;
#endif
    std::size_t m_preeditCursor=0;
    bool m_preeditCursorVisible=true;
    char16_t m_highSurrogate=0;
    int m_pressed=0;
    int m_hover=0;
};
