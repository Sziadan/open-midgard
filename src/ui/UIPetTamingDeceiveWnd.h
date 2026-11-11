#pragma once
#include "UIFrameWnd.h"
#include "UIShopCommon.h"
#include <cstdint>
#include <string>

// Source factory91. Gameplay prepares one draw frame for both renderers;
// drawing and state projection only consume the prepared image.
class UIPetTamingDeceiveWnd : public UIFrameWnd {
public:
    UIPetTamingDeceiveWnd();
    void PrepareDrawFrame();
    void ProcessAt(u32 now);
    void ApplyCaptureResult(bool captured);
    void OnProcess() override;
    void OnDraw() override;
    void OnLBtnDown(int,int) override;
    msgresult_t SendMsg(UIWindow*,int,msgparam_t,msgparam_t,msgparam_t) override;
    const shopui::BitmapPixels& GetArtwork() const {return m_artwork;}
    const std::string& GetArtworkUri() const {return m_uri;}
    u32 GetTargetGid() const {return m_monGid;}
    bool IsSelected() const {return m_selected;}
    int GetAction() const {return m_action;}
    std::int64_t GetMotionTick() const {return m_motionTick;}
    int GetDelay() const {return m_delay;}
    int GetMaxDelay() const {return m_maxDelay;}
    int GetDestination() const {return m_destination;}
    u32 GetStartTick() const {return m_startTick;}
private:
    void PrepareArtwork(int frame);
    u32 m_monGid=0;
    int m_action=0;
    std::int64_t m_motionTick=0;
    bool m_selected=false;
    bool m_awaitingResult=false;
    int m_delay=1,m_destination=-1,m_maxDelay=10000;
    u32 m_startTick=0xffffffffu;
    int m_artAction=-1,m_artFrame=-1;
    const void* m_artActResource=nullptr;
    const void* m_artSprResource=nullptr;
    shopui::BitmapPixels m_artwork;
    std::string m_uri;
};
