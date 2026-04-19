#pragma once

#include "UIFrameWnd.h"

#include <string>
#include <vector>

class UIEditCtrl;

class UIPartyOptionWnd : public UIFrameWnd {
public:
    enum ButtonId {
        ButtonClose = 1,
        ButtonConfirm = 2,
        ButtonCancel = 3,
    };

    enum OptionGroup {
        GroupExpShare = 0,
        GroupItemShare = 1,
        GroupItemSharingType = 2,
    };

    struct QtButtonDisplay {
        int id = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::string label;
        bool visible = true;
        bool active = false;
    };

    struct DisplayLabel {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::string text;
        bool header = false;
    };

    struct DisplayChoice {
        int groupId = 0;
        int optionId = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::string label;
        bool selected = false;
    };

    struct DisplayData {
        std::string title;
        bool showNameEdit = false;
        bool nameFocused = false;
        std::string nameLabel;
        std::string nameValue;
        int nameFieldX = 0;
        int nameFieldY = 0;
        int nameFieldWidth = 0;
        int nameFieldHeight = 0;
        std::vector<DisplayLabel> labels;
        std::vector<DisplayChoice> choices;
        std::vector<QtButtonDisplay> buttons;
    };

    UIPartyOptionWnd();
    ~UIPartyOptionWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    bool IsUpdateNeed() override;
    void OnCreate(int x, int y) override;
    void OnDestroy() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnKeyDown(int virtualKey) override;
    void StoreInfo() override;

    void OpenCreateDialog();
    void OpenConfigureDialog();
    bool GetDisplayDataForQt(DisplayData* outData) const;

private:
    enum Mode {
        ModeCreate = 0,
        ModeConfigure = 1,
    };

    void LayoutControls();
    void ResetCreateDefaults();
    void LoadCurrentPartyOptions();
    void ClearEditFocus();
    unsigned int BuildOptionBits() const;
    bool Submit();
    void SetSelectedChoice(int groupId, int optionId);
    int GetSelectedChoice(int groupId) const;
    int HitTestButton(int x, int y) const;
    bool HitTestChoice(int x, int y, int* outGroupId, int* outOptionId) const;
    RECT GetCloseButtonRect() const;
    RECT GetNameFieldRect() const;
    RECT GetButtonRect(int buttonId) const;
    RECT GetChoiceRect(int groupId, int optionId) const;
    unsigned long long BuildVisualStateToken() const;

    Mode m_mode;
    UIEditCtrl* m_nameEditCtrl;
    bool m_controlsCreated;
    int m_pressedButtonId;
    int m_expShareOption;
    int m_itemShareOption;
    int m_itemSharingTypeOption;
    unsigned long long m_lastVisualStateToken;
    bool m_hasVisualStateToken;
};