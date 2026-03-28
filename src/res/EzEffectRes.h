#pragma once

#include "Res.h"
#include "Texture.h"

#include <array>
#include <string>
#include <vector>

struct KAC_XFORMDATA {
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    float us = 0.0f;
    float vs = 0.0f;
    float u2 = 0.0f;
    float v2 = 0.0f;
    float us2 = 0.0f;
    float vs2 = 0.0f;
    float ax[4] = {};
    float ay[4] = {};
    float aniframe = 0.0f;
    u32 anitype = 0;
    float anidelta = 0.0f;
    float rz = 0.0f;
    float crR = 255.0f;
    float crG = 255.0f;
    float crB = 255.0f;
    float crA = 255.0f;
    u32 srcalpha = 5;
    u32 destalpha = 2;
    u32 mtpreset = 0;
};

static_assert(sizeof(KAC_XFORMDATA) == 116, "KAC_XFORMDATA size mismatch");

struct KAC_KEYFRAME {
    int iFrame = 0;
    u32 dwType = 0;
    KAC_XFORMDATA XformData;
};

static_assert(sizeof(KAC_KEYFRAME) == 124, "KAC_KEYFRAME size mismatch");

struct KAC_LAYER {
    int cTex = 0;
    int iCurAniFrame = 0;
    std::array<CTexture*, 110> m_tex{};
    std::array<const char*, 110> m_texName{};
    int cAniKey = 0;
    KAC_KEYFRAME* aAniKey = nullptr;

    std::vector<std::string> m_ownedTexNames;
    std::vector<KAC_KEYFRAME> m_ownedAniKeys;

    void Reset();
    CTexture* GetTexture(int iTex);
};

struct KANICLIP {
    int nFPS = 0;
    int cFrame = 0;
    int cLayer = 0;
    int cEndLayer = 0;
    std::array<KAC_LAYER, 60> aLayer{};

    void Reset();
};

class CEZeffectRes : public CRes {
public:
    CEZeffectRes() = default;
    ~CEZeffectRes() override = default;

    bool LoadFromBuffer(const char* fName, const unsigned char* buffer, int size) override;
    CRes* Clone() override;
    void Reset() override;

    KANICLIP m_aniClips;
    int m_nMaxLayer = 0;
};
