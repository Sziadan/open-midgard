#pragma once
#include "UIFriendOptionWnd.h"

class UIPetCommandMenu;
struct PetInfoText {int x=0,y=0;std::string text;u32 color=0;};
struct PetInfoArtwork {int x=0,y=0;shopui::BitmapPixels bitmap;std::string uri;int phase=0;};

class UIPetInfoWnd : public UIFrameWnd {
public:
    UIPetInfoWnd();
    static void LoadStoredInfo(SubChatWindowInfo& info);
    ~UIPetInfoWnd() override;
    UIEditCtrl* GetNameEdit() const {return m_nameEdit;}
    std::string GetEditDisplayText() const;
    const std::array<UIFriendOptionButton*,3>& GetButtons() const {return m_buttons;}
    std::vector<PetInfoText> GetDisplayTexts() const;
    std::vector<PetInfoArtwork> GetFramePieces() const;
    const PetInfoArtwork& GetPortrait();
    UIPetCommandMenu* GetCommandMenu() const {return m_menu;}
    void RefreshReceivedName();
    void ShowCommandMenu();
    void CloseCommandMenu();
    void CommandMenuDeleted(UIPetCommandMenu* menu);
    void SelectCommand(int index);
    void RequestRename();
    void Move(int x,int y) override;
    void StoreInfo() override;
    void CancelPointerCapture();
    void OnLBtnDown(int x,int y) override;
    void OnKeyDown(int key) override;
    bool CanReceiveKeyInput() const override {return true;}
    void OnDraw() override;
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;
private:
    void ShowMessage(int id,const std::string& text,std::function<void(bool)> action={});
    UIEditCtrl* m_nameEdit;
    std::array<UIFriendOptionButton*,3> m_buttons{};
    std::array<shopui::BitmapPixels,5> m_frame;
    std::array<std::string,5> m_frameUris;
    UIPetCommandMenu* m_menu=nullptr;
    PetInfoArtwork m_portrait;
    std::vector<unsigned int> m_nativePortraitPixels;
    int m_portraitJob=-2;
    bool m_portraitIllustrated=false;
    SubChatWindowInfo* m_info=nullptr;
};

class UIPetCommandMenu : public UIWindow {
public:
    explicit UIPetCommandMenu(UIPetInfoWnd* owner);
    ~UIPetCommandMenu() override;
    const std::array<std::string,4>& GetLabels() const {return m_labels;}
    int GetSelection() const {return m_selection;}
    UIPetInfoWnd* GetOwner() const {return m_owner;}
    void OnDraw() override;
    void OnMouseMove(int x,int y) override;
    void OnMouseHover(int x,int y) override {OnMouseMove(x,y);}
    void OnLBtnDown(int,int) override {}
    void OnLBtnUp(int x,int y) override;
    void OnKeyDown(int key) override;
    bool CanReceiveKeyInput() const override {return true;}
private:
    void Activate(int index);
    UIPetInfoWnd* m_owner;
    std::array<std::string,4> m_labels;
    int m_selection=-1;
};
