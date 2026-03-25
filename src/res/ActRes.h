#pragma once
#include "Types.h"
#include "res/Res.h"
#include <vector>
#include <string>

struct CSprClip {
    int x;
    int y;
    int sprIndex;
    int flags;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    float zoomX;
    float zoomY;
    int angle;
    int clipType;
};

struct CAttachPointInfo {
    int x;
    int y;
    int attr;
};

struct CMotion {
    RECT range1;
    RECT range2;
    std::vector<CSprClip> sprClips;
    int eventId;
    int attachCount;
    std::vector<CAttachPointInfo> attachInfo;
};

struct CAction {
    std::vector<CMotion> motions;
};

class CActRes : public CRes {
public:
    CActRes();
    virtual ~CActRes();

    virtual bool LoadFromBuffer(const char* fName, const unsigned char* buffer, int size) override;
    virtual bool Load(const char* fName) override;
    virtual CRes* Clone() override;
    virtual void Reset() override;

    float GetDelay(int actionIndex) const;
    int GetMotionCount(int actionIndex) const;
    const CMotion* GetMotion(int actionIndex, int motionIndex) const;
    const char* GetEventName(int eventIndex) const;

    std::vector<CAction> actions;
    int numMaxClipPerMotion;
    std::vector<std::string> m_events;
    std::vector<float> m_delay;
};
