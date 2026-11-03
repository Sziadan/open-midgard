#include "world/MapLighting.h"
#include "render3d/ModernRenderState.h"
#include "res/ModelRes.h"
#include <cassert>
#include <cmath>
#include <iostream>

// Resource I/O is outside this geometry-only executable. ModelRes.cpp is linked
// intact so the tests call the production mesh normal generator.
CRes::CRes():m_lockCnt(0),m_timeStamp(0),m_extIndex(-1) {}
CRes::~CRes()=default;
bool CRes::Load(const char*) {return false;}
bool CRes::LoadFromBuffer(const char*,const unsigned char*,int) {return false;}
void CRes::Reset() {}
void CRes::OnLoadError(const char*) {}

bool Near(float a,float b) {return std::fabs(a-b)<.0001f;}
int main()
{
    // Ref/Mesh.cpp uses cross(v2-v1, v2-v0). A horizontal model surface
    // with these authored indices must face the sun, not shade its underside.
    C3dMesh mesh;mesh.m_numVert=4;mesh.m_numFace=2;
    mesh.m_vert={{0,0,0},{1,0,0},{0,0,1},{1,0,1}};
    mesh.m_face.resize(2);
    mesh.m_face[0].vertindex[0]=0;mesh.m_face[0].vertindex[1]=1;mesh.m_face[0].vertindex[2]=2;
    mesh.m_face[1].vertindex[0]=1;mesh.m_face[1].vertindex[1]=3;mesh.m_face[1].vertindex[2]=2;
    mesh.UpdateNormal();
    for(const auto& normal:mesh.m_faceNormal)assert(normal.y>.999f);
    for(const auto& normal:mesh.m_vertNormal)assert(normal.y>.999f);
    const auto daylight=ro::lighting::Directional(mesh.m_faceNormal[0],{.5f,.7071068f,.5f},{1,1,1},{.55f,.5f,.5f});
    assert(Near(daylight.x,1) && Near(daylight.y,1) && Near(daylight.z,1));
    const vector3d ambient{.3f,.4f,.8f},diffuse{.8f,.4f,.5f};
    const vector3d sun{.6408564f,.4226183f,.6408564f};
    const auto top=ro::lighting::Directional({0,1,0},sun,diffuse,ambient);
    assert(Near(top.x,.6380946f) && Near(top.y,.5690473f) && Near(top.z,1));
    const auto back=ro::lighting::Directional({0,-1,0},sun,diffuse,ambient);
    assert(Near(back.x,.3f) && Near(back.y,.4f) && Near(back.z,.8f));
    const auto env=ro::lighting::Environment(ambient,diffuse);
    assert(Near(env.x,.86f) && Near(env.y,.64f) && Near(env.z,.9f));
    assert(ro::lighting::Tint(0x7FFFFFFFu,env)==0x7FDBA3E6u);
    const auto day=ro::lighting::Environment({.3f,.3f,.3f},{1,1,1});
    const auto night=ro::lighting::Environment({.3f,.3f,.3f},{.5f,.5f,1});
    assert(ro::lighting::Tint(0xFFFFFFFFu,day)==0xFFFFFFFFu);
    assert(ro::lighting::Tint(0xFFFFFFFFu,night)==0xFFA6A6FFu);
    matrix mirror{};mirror.m[0][0]=-2;mirror.m[1][1]=3;mirror.m[2][2]=4;
    const auto transformed=ro::lighting::TransformNormal({0,1,0},mirror);
    assert(Near(transformed.y,1) && Near(transformed.x,0));
    const auto reflected=ro::lighting::TransformNormal({1,0,0},mirror);
    assert(Near(reflected.x,-1) && Near(reflected.y,0));

    ModernFixedFunctionState state{};ResetModernFixedFunctionState(&state);
    state.textureStages[1].colorOp=D3DTOP_MODULATEALPHA_ADDCOLOR;
    state.textureStages[1].colorArg1=D3DTA_TEXTURE;
    state.textureStages[1].colorArg2=D3DTA_CURRENT;
    auto flags=BuildModernDrawFlags(kModernLightmapFvf,state,true,true);
    assert(flags&ModernDrawFlag_Stage1LightmapAddColor);
    assert(!(flags&ModernDrawFlag_Stage1LightmapAlpha));
    assert(!(BuildModernDrawFlags(kModernLightmapFvf,state,true,false)&ModernDrawFlag_Stage1LightmapAddColor));
    assert(!(BuildModernDrawFlags(D3DFVF_TLVERTEX,state,true,true)&ModernDrawFlag_Stage1LightmapAddColor));
    state.textureStages[1].colorOp=D3DTOP_MODULATE;
    state.textureStages[1].colorArg1=D3DTA_TEXTURE|D3DTA_ALPHAREPLICATE;
    flags=BuildModernDrawFlags(kModernLightmapFvf,state,true,true);
    assert(flags&ModernDrawFlag_Stage1LightmapAlpha);
    assert(!(flags&ModernDrawFlag_Stage1LightmapAddColor));
    std::cout<<"PASS: Comodo directional/ambient light, environment tint, night transitions and colored-lightmap draw state\n";
}
