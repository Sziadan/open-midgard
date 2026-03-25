#pragma once

#include "Res.h"

#include <list>
#include <string>

struct SceneGraphNode;

class C3dWorldRes : public CRes {
public:
    struct actorInfo {
        char name[40];
        char modelName[80];
        char nodeName[80];
        vector3d pos;
        vector3d rot;
        vector3d scale;
        int animType;
        int blockType;
        float animSpeed;
        float posx;
        float posy;
        float posz;
    };

    struct effectSrcInfo {
        char name[80];
        vector3d pos;
        int type;
        float emitSpeed;
        float param[4];
    };

    struct soundSrcInfo {
        char name[80];
        char waveName[80];
        vector3d pos;
        float vol;
        int width;
        int height;
        float range;
        float cycle;
    };

    C3dWorldRes();
    ~C3dWorldRes() override;

    bool LoadFromBuffer(const char* fName, const unsigned char* buffer, int size) override;
    CRes* Clone() override;
    void Reset() override;

    std::list<actorInfo*> m_3dActors;
    std::list<effectSrcInfo*> m_particles;
    std::list<soundSrcInfo*> m_sounds;
    SceneGraphNode* m_CalculatedNode;
    std::string m_gndFile;
    std::string m_attrFile;
    std::string m_scrFile;
    float m_waterLevel;
    int m_waterType;
    float m_waveHeight;
    float m_waveSpeed;
    float m_wavePitch;
    int m_waterAnimSpeed;
    int m_lightLongitude;
    int m_lightLatitude;
    vector3d m_lightDir;
    vector3d m_diffuseCol;
    vector3d m_ambientCol;
    u8 m_verMajor;
    u8 m_verMinor;
    int m_groundTop;
    int m_groundBottom;
    int m_groundLeft;
    int m_groundRight;
};

static_assert(sizeof(C3dWorldRes::actorInfo) == 260, "actorInfo size mismatch");
static_assert(sizeof(C3dWorldRes::effectSrcInfo) == 116, "effectSrcInfo size mismatch");
static_assert(sizeof(C3dWorldRes::soundSrcInfo) == 192, "soundSrcInfo size mismatch");