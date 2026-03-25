#pragma once

#include "UIFrameWnd.h"

#include <array>
#include <string>
#include <windows.h>

class UIBitmapButton;

class UISelectCharWnd : public UIFrameWnd {
public:
    UISelectCharWnd();
    ~UISelectCharWnd() override;

    struct PreviewState {
        bool valid = false;
        int x = 0;
        int y = 0;
        int baseAction = 0;
        int curAction = 0;
        int curMotion = 0;
        int bodyPalette = 0;
        int headPalette = 0;
        std::string actName[2];
        std::string sprName[2];
        std::string imfName;
        std::string bodyPaletteName;
        std::string headPaletteName;
    };

    void OnCreate(int cx, int cy) override;
    void OnDraw() override;
    void OnProcess() override;
    void OnLBtnUp(int x, int y) override;
    int SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra) override;
    void OnKeyDown(int virtualKey);

private:
    int GetCharacterCount() const;
    CHARACTER_INFO* GetCharacters() const;
    int GetPageCount() const;
    int GetSlotCount() const;
    int FindCharacterIndexForSlot(int slotNumber) const;
    int GetVisibleSlotStart() const;
    void ClampSelection();
    void SaveSelectionToRegistry() const;
    void LoadSelectionFromRegistry();
    void MoveSelection(int delta);
    void ChangePage(int delta);
    void SetSelectedSlot(int slotNumber);
    void ActivateSelectedSlot();
    void ActivateCreate();
    void ActivateDelete();
    void EnsureResourceCache();
    void EnsureButtons();
    void UpdateActionButtons();
    void ClearAssets();
    void ReleaseComposeSurface();
    bool EnsureComposeSurface(HDC referenceDC, int width, int height);
    void RebuildVisiblePreviews();
    void BuildPreviewForSlot(int visibleIndex, const CHARACTER_INFO& info);
    void DrawPreview(HDC hdc, const PreviewState& preview) const;
    int HitSlot(int x, int y) const;
    bool HitPrevPageButton(int x, int y) const;
    bool HitNextPageButton(int x, int y) const;

    bool m_controlsCreated;
    bool m_assetsProbed;
    HBITMAP m_backgroundBmp;
    std::string m_backgroundPath;
    HBITMAP m_slotBmp;
    HBITMAP m_slotSelectedBmp;
    HDC m_composeDC;
    HBITMAP m_composeBitmap;
    int m_composeWidth;
    int m_composeHeight;
    UIBitmapButton* m_okButton;
    UIBitmapButton* m_cancelButton;
    UIBitmapButton* m_makeButton;
    UIBitmapButton* m_deleteButton;
    UIBitmapButton* m_chargeButton;
    int m_selectedSlot;
    int m_page;
    std::array<PreviewState, 3> m_visiblePreviews;
    DWORD m_lastAnimTick;
    int m_animAction;
};