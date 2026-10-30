#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>
#include <cstdio>
#include <vector>
#include <string>
#include <list>

using u32 = std::uint32_t;
struct RECT { long left, top, right, bottom; };
#include "ParticleActTypes.inc"
struct vector2d { float x=0, y=0; };
struct vector3d { float x=0, y=0, z=0; };
struct matrix { float m[4][4]{}; };
struct EffectGi {
    short alphaB=0, alphaT=0, fullDisplayAngle=0, rotStart=0, riseAngle=0;
    float distance=0.0f,maxHeight=0.0f;
    std::array<float,21> height{};
    std::array<unsigned char,21> flag{};
    float sparkX=0.0f,sparkY=0.0f,sparkZ=0.0f;
    float vecBPreX=0.0f,arcBaselineY=0.0f,vecBPreZ=0.0f;
    float vecTPreX=0.0f,vecTPreY=0.0f,vecTPreZ=0.0f;
    float vecTNowX=0.0f,vecTNowY=0.0f,vecTNowZ=0.0f;
    int process=0;
    unsigned char life=0;
};
void MatrixIdentity(matrix& m){m={};for(int i=0;i<4;++i)m.m[i][i]=1;}
#include "ParticleMatrixBodies.inc"
#include "EffectSegmentTypes.inc"
struct CTexture {
    unsigned int m_w=4, m_h=4, m_updateWidth=2, m_updateHeight=2;
    unsigned int m_surfaceUpdateWidth=2,m_surfaceUpdateHeight=2;
};
struct GroundTileCall { int x=0,y=0;u32 color=0;CTexture* texture=nullptr;vector2d uv[4]{}; };
struct C3dGround {
    std::vector<GroundTileCall> calls;
    void RenderGroundTile(const matrix&,int x,int y,u32 color,CTexture* texture,const vector2d uv[4]) {
        GroundTileCall call;call.x=x;call.y=y;call.color=color;call.texture=texture;
        for(int i=0;i<4;++i)call.uv[i]=uv[i];
        calls.push_back(call);
    }
};
using msgparam_t = std::uintptr_t;
struct CRagEffect {
    int sendCount=0,lastMessage=0;
    msgparam_t lastParameter2=0;
    vector3d lastPosition{},lastPosition2{};
    void SendMsg(void*,int message,msgparam_t parameter,msgparam_t parameter2,msgparam_t) {
        ++sendCount;lastMessage=message;
        if(parameter)lastPosition=*reinterpret_cast<vector3d*>(parameter);
        lastParameter2=parameter2;
        if(parameter2)lastPosition2=*reinterpret_cast<vector3d*>(parameter2);
    }
};
struct CAbleToMakeEffect {
    CRagEffect effect;
    bool effectAvailable=true;
    int launchCount=0,lastEffectId=0;
    vector3d lastDelta{};float lastRotation=0;
    CRagEffect* LaunchEffect(int effectId,vector3d delta,float rotation) {
        ++launchCount;lastEffectId=effectId;lastDelta=delta;lastRotation=rotation;return effectAvailable?&effect:nullptr;
    }
};
struct CWorld {
    C3dGround* m_ground=nullptr;
    CAbleToMakeEffect* m_player=nullptr;
    CAbleToMakeEffect* m_actor=nullptr;
    u32 actorAid=0;
    CAbleToMakeEffect* GetGameActorByAID(u32 aid) const {return aid==actorAid?m_actor:nullptr;}
};
struct CGameMode {
    CWorld* m_world=nullptr;int serverX=0,serverY=0,coordinateCalls=0;
    void GetServerCoor(float,float,int& x,int& y) {++coordinateCalls;x=serverX;y=serverY;}
};
struct CModeMgr {
    CGameMode* current=nullptr;int lookups=0;
    CGameMode* GetCurrentGameMode() {++lookups;return current;}
} g_modeMgr;
struct TestSession { u32 m_aid=0; } g_session;
struct SprImg {
    int width=2, height=2, isHalfW=0, isHalfH=0;
    CTexture* tex=nullptr;
    std::vector<unsigned char> indices;
    std::vector<unsigned int> rgba;
};
struct CSprRes {
    std::vector<SprImg*> m_sprites[2];
    unsigned int m_pal[256]{};
    const SprImg* GetSprite(int type,int index) const {
        if(type<0 || type>1 || index<0 || index>=static_cast<int>(m_sprites[type].size())) return nullptr;
        return m_sprites[type][index];
    }
};
struct CActRes {
    std::vector<CAction> actions;
    const CMotion* GetMotion(int action,int motion) const {
        if(action<0 || action>=static_cast<int>(actions.size())) return nullptr;
        const auto& frames=actions[action].motions;
        return motion<0 || motion>=static_cast<int>(frames.size())?nullptr:&frames[motion];
    }
};
struct CEffectPrim {
    int m_type=0,m_numSegments=1,m_spriteAction=0,m_curMotion=0,m_pattern=0;
    u32 m_renderFlag=5;
    float m_roll=0,m_size=1,m_arcAngle=36,m_longitude=0;
    matrix m_matrix;
    matrix m_rotMat;
    vector3d m_pos,m_deltaPos;
    float m_innerSize=0,m_outerSize=0,m_heightSize=0,m_widthSize=0,m_radius=0;
    float m_v1=0,m_v2=1;
    u32 m_argb=0xFFFFFFFF;
    std::vector<CTexture*> m_texture;
    std::array<EffectSegmentState,50> m_segments;
    std::vector<std::array<vector3d,4>> m_segmentQuadPositions = std::vector<std::array<vector3d,4>>(50);
    std::array<EffectGi,4> m_gi;
};
constexpr int PP_2DTEXTURE=0;
constexpr int PP_2DTEXTUREORBIT=1;
using D3DBLEND=int;
constexpr int PF_A8R8G8B8=4, D3DPT_TRIANGLESTRIP=5, D3DPT_TRIANGLELIST=4, D3DCULL_NONE=1;
constexpr int D3DRENDERSTATE_ZENABLE=0,D3DRENDERSTATE_ZWRITEENABLE=1,D3DRENDERSTATE_SRCBLEND=2,D3DRENDERSTATE_DESTBLEND=3;
constexpr int TRUE=1,FALSE=0,D3DZB_FALSE=0,D3DZB_TRUE=1;
struct SilentRenderDevice;
constexpr int D3DBLEND_SRCALPHA=5,D3DBLEND_ONE=2,D3DBLEND_INVSRCALPHA=6;
struct CTexMgr {
    static CTexture s_dummy_texture;
    CTexture texture;
    int calls=0,lastWidth=0,lastHeight=0;
    bool fail=false;
    std::vector<unsigned int> pixels;
    CTexture* CreateTexture(int w,int h,unsigned int* data,int,bool) {
        ++calls;lastWidth=w;lastHeight=h;pixels.assign(data,data+w*h);
        return fail?&s_dummy_texture:&texture;
    }
    CTexture* GetTexture(const char*,bool) { return &texture; }
} g_texMgr;
CTexture CTexMgr::s_dummy_texture;
struct tlvertex3d { float x=0,y=0,z=0,oow=0; u32 color=0,specular=0; float tu=0,tv=0; };
struct RPFace {
    int primType=0,numVerts=0,numIndices=0,mtPreset=0,cullMode=0;
    tlvertex3d* verts=nullptr;
    tlvertex3d m_verts[6];
    unsigned short* indices=nullptr;
    CTexture* tex=nullptr;
    int srcAlphaMode=0,destAlphaMode=0;
    float alphaSortKey=0;
    static void DrawPri(RPFace*,SilentRenderDevice&);
};
struct SilentDraw { RPFace face; int depthTest,depthWrite,srcBlend,destBlend; CTexture* bound; };
struct SilentRenderDevice {
    int states[4]{1,1,5,6};
    CTexture* bound=nullptr;
    std::vector<SilentDraw> draws;
    void SetRenderState(int name,int value){states[name]=value;}
} silentDevice;
void RPFace::DrawPri(RPFace* face,SilentRenderDevice& device){
    device.draws.push_back({*face,device.states[0],device.states[1],device.states[2],device.states[3],device.bound});
}
int fogCalls=0;
void ApplyFogToVertices(tlvertex3d*,int,bool,int){++fogCalls;}
struct SilentRenderer {
    int m_width=640,m_height=480,m_xoffset=320,m_yoffset=240,m_halfWidth=320,m_halfHeight=240;
    float m_hpc=700,m_vpc=700;
    std::list<RPFace> pool;
    bool m_isFoggy=false;int m_fogPara=0;
    SilentRenderDevice* m_renderDevice=&silentDevice;
    std::vector<RPFace*> m_rpAlphaOPList,m_rpLMGroundList,m_rpLMLightList,m_rpFaceList;
    std::vector<std::pair<float,RPFace*>> m_rpAlphaDepthList,m_rpAlphaList,m_rpAlphaNoDepthList;
    std::vector<std::pair<float,RPFace*>> m_rpEmissiveDepthList,m_rpEmissiveList,m_rpEmissiveNoDepthList;
    void AddReferenceEffectTriangle(RPFace*,int);
    void FlushEffectDepthLists();
    void FlushEffectNoDepthLists();
    void SetTexture(CTexture* tex){m_renderDevice->bound=tex;}
    std::vector<RPFace> submissions;
    std::vector<int> flags;
    RPFace* BorrowNullRP() { pool.emplace_back();return &pool.back(); }
    void AddRP(RPFace* value,int flag) { submissions.push_back(*value);flags.push_back(flag); }
} g_renderer;
float GetRadian(int angle){return angle*3.14159265f/180.0f;}
float GetCos(int angle) { return std::cos(angle*3.14159265f/180.0f); }
float GetSin(int angle) { return std::sin(angle*3.14159265f/180.0f); }
void ProjectReferenceEffectVertex(const vector3d&,const matrix&,tlvertex3d&);
extern float g_cosTable[361],g_sinTable[361];
void IntimidateTowardScreen(vector3d corners[4],int rotationX,int rotationYZ);
int TableAngle(int angle);
int GetReferenceAngle(float moveX,float moveZ);
void RotateCornerTowardScreen(vector3d* corner,int rot0,int rot1);
struct AirTextureQuadCall { vector3d a,b,c,d; CTexture* texture=nullptr; u32 color=0; D3DBLEND blend=0; int flags=0; float u0=0,u1=0,vBottom=0,vTop=0; };
std::vector<AirTextureQuadCall> airTextureQuadCalls;
void SubmitWorldTeiRect(const vector3d&,const vector3d&,const vector3d&,const vector3d&,
    const matrix&,CTexture*,unsigned int,float,float,float,float,D3DBLEND,float,int);
#include "ParticleRenderBodies.inc"
float g_cosTable[361],g_sinTable[361];
int TableAngle(int angle) { angle %= 360; return angle < 0 ? angle + 360 : angle; }
int GetReferenceAngle(float moveX,float moveZ) {
    const float length=std::sqrt(moveX*moveX+moveZ*moveZ);
    const float normalizedX=moveX/length,normalizedZ=moveZ/length;
    int angle=0;while(normalizedX<g_cosTable[angle]){if(++angle>180){angle=180;break;}}
    return normalizedZ<0.0f?360-angle:angle;
}
void RotateCornerTowardScreen(vector3d* corner,int rot0,int rot1) {
    const float oldZ=corner->z,oldY=corner->y;
    corner->z=oldZ*g_cosTable[rot1]-oldY*g_sinTable[rot1];
    corner->y=oldZ*g_sinTable[rot1]+oldY*g_cosTable[rot1];
    const float oldX=corner->x,rotatedZ=corner->z;
    corner->x=oldX*g_cosTable[rot0]-rotatedZ*g_sinTable[rot0];
    corner->z=oldX*g_sinTable[rot0]+rotatedZ*g_cosTable[rot0];
}
void SubmitWorldTeiRect(const vector3d& a,const vector3d& b,const vector3d& c,const vector3d& d,
    const matrix&,CTexture* texture,unsigned int color,float u0,float u1,float vBottom,float vTop,D3DBLEND blend,float,int flags) {
    airTextureQuadCalls.push_back({a,b,c,d,texture,color,blend,flags,u0,u1,vBottom,vTop});
}
#include "AirTextureRendererBody.inc"
#include "HitLine3RendererBody.inc"
#include "SoulLinkRendererBody.inc"
#include "SoulParticleRendererBody.inc"
#include "HermodeRendererBody.inc"
vector3d AddVec3(const vector3d& a,const vector3d& b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
constexpr float kSubmitNearPlane=80.0f,kNearPlane=10.0f;
#include "ProjectPointBody.inc"
#include "CircleRenderBodies.inc"
#include "CastingColorBodies.inc"
#include "CastingLowRiseBody.inc"
#include "ElectricPathBody.inc"
#include "Electric2PathBody.inc"
#include "WindSampleBody.inc"
#include "Aura2RendererBody.inc"
#include "AuraRendererBody.inc"
#include "GrandCross2CornersBody.inc"
#include "LineLinkGeometryBody.inc"
#include "IntimidateRenderGeometryBody.inc"
#include "GumGangRenderGeometryBody.inc"
#include "TeiHit1GeometryBody.inc"
#include "CloudRenderGeometryBody.inc"
#include "ReferenceTriangleQueueBodies.inc"

int main() {

    for(int i=0;i<=360;++i){g_cosTable[i]=GetCos(i);g_sinTable[i]=GetSin(i);}
    int checks=0,failures=0;
    auto check=[&](bool ok,const char* message){++checks;if(!ok){++failures;std::printf("FAIL: %s\n",message);}};
    auto near=[](float a,float b){return std::fabs(a-b)<0.0001f;};
    {
        CTexture texture; CEffectPrim aura; aura.m_texture={&texture}; aura.m_pos={10,20,30}; aura.m_size=0.0f;
        aura.m_renderFlag=7; EffectGi& gi=aura.m_gi[0]; gi.life=1; gi.alphaB=180; gi.height[0]=2;
        gi.distance=10; gi.riseAngle=0; gi.rotStart=0;
        matrix identity{}; MatrixIdentity(identity);
        airTextureQuadCalls.clear();
        Render3DAura(aura,identity);
        check(airTextureQuadCalls.size()==1,"3DAura submits one source quad for one live GI lane");
        if(airTextureQuadCalls.size()==1) {
            const auto& call=airTextureQuadCalls[0];
            check(near(call.a.x,19.0f)&&near(call.a.y,22.0f)&&near(call.a.z,30.0f)
                &&call.texture==&texture&&call.color==0xB4FFFFFFu&&call.blend==D3DBLEND_ONE
                &&call.u0==0.0f&&call.u1==1.0f&&call.vBottom==1.0f&&call.vTop==0.0f,
                "3DAura maps source position, texture slot, alpha/RGB, blend, and reversed UV arguments");
            check(near(call.b.x,10.0f)&&near(call.b.y,22.0f)&&near(call.b.z,39.0f)
                &&near(call.c.x,1.0f)&&near(call.c.y,22.0f)&&near(call.c.z,30.0f)
                &&near(call.d.x,10.0f)&&near(call.d.y,22.0f)&&near(call.d.z,21.0f),
                "3DAura submits PE vec1..vec4 as B-pre, spark, T-now, T-pre");
            check(near(gi.sparkX,0.0f)&&near(gi.sparkY,2.0f)&&near(gi.sparkZ,9.0f)
                &&near(gi.vecTNowX,-9.0f)&&near(gi.vecTNowZ,0.0f),
                "3DAura preserves the source scratch writes for consecutive radius corners");
        }
        aura.m_size=100.0f;
        airTextureQuadCalls.clear();
        Render3DAura(aura,identity);
        check(airTextureQuadCalls.size()==2,
            "3DAura size 100 emits both crossed white faces in source order");
        if(airTextureQuadCalls.size()==2) {
            check(airTextureQuadCalls[0].color==0xB4FFFFFFu
                &&airTextureQuadCalls[1].color==0xB4FFFFFFu
                &&near(airTextureQuadCalls[0].a.x,airTextureQuadCalls[1].a.x),
                "3DAura size 100 duplicates the same white source face");
        }
        aura.m_size=101.0f;
        airTextureQuadCalls.clear(); Render3DAura(aura,identity);
        check(airTextureQuadCalls.size()==2&&airTextureQuadCalls[0].color==0xB4FF0000u
            &&airTextureQuadCalls[1].color==0xB4FF0000u,
            "3DAura size 101 emits two red faces");
        aura.m_size=110.0f;
        airTextureQuadCalls.clear(); Render3DAura(aura,identity);
        check(airTextureQuadCalls.size()==1&&airTextureQuadCalls[0].blend==D3DBLEND_INVSRCALPHA,
            "3DAura size 110 uses inverse-alpha blending");
    }
    {
        CTexture texture; CEffectPrim aura; aura.m_texture={&texture}; aura.m_pos={10,20,30}; aura.m_renderFlag=9;
        aura.m_gi[0].life=1; aura.m_gi[0].distance=10; aura.m_gi[0].alphaT=0; aura.m_gi[0].flag[0]=0;
        aura.m_gi[1].alphaT=90; aura.m_gi[0].vecBPreX=1; aura.m_gi[0].arcBaselineY=-10; aura.m_gi[0].vecBPreZ=3;
        aura.m_gi[0].sparkY=1; aura.m_gi[0].vecTPreY=1; aura.m_gi[0].vecTNowY=1;
        matrix identity{}; MatrixIdentity(identity);
        airTextureQuadCalls.clear();
        Render3DAura2(aura,identity);
        check(airTextureQuadCalls.size()==2,"Aura2 mode zero submits two white faces for the B-pre group");
        if(airTextureQuadCalls.size()==2) {
            const auto& call=airTextureQuadCalls[0];
            const float edge = 10.0f * GetCos(45);
            check(near(call.a.x,10.0f+1.0f+edge)&&near(call.a.y,20.0f-10.0f+edge)
                &&near(call.a.z,33.0f)&&call.color==0xFAFFFFFFu
                &&call.blend==D3DBLEND_ONE&&call.texture==&texture&&call.flags==9,
                "Aura2 billboards the 45-degree radius square and adds the B-pre and world origins");
            check(near(call.b.x,10.0f+1.0f-edge)&&near(call.c.x,10.0f+1.0f-edge)
                &&near(call.d.x,10.0f+1.0f+edge)&&near(call.a.y-call.d.y,edge*2.0f),
                "Aura2 keeps the camera-facing square at the source radius");
            check(call.u0==0.0f&&call.u1==1.0f&&call.vBottom==1.0f&&call.vTop==0.0f,
                "Aura2 maps source Ty1/Ty2 to the reversed local V interval");
        }
        aura.m_gi[0].flag[0]=1;
        airTextureQuadCalls.clear(); Render3DAura2(aura,identity);
        check(airTextureQuadCalls.size()==2&&airTextureQuadCalls[0].color==0xFA5050FFu,
            "Aura2 mode one duplicates the blue B-pre face");
        aura.m_gi[0].flag[0]=3;
        aura.m_gi[0].alphaT=0; aura.m_gi[1].alphaT=0;
        aura.m_gi[0].arcBaselineY=aura.m_gi[0].sparkY=aura.m_gi[0].vecTPreY=aura.m_gi[0].vecTNowY=-1.0f;
        aura.m_gi[0].vecBPreX=1; aura.m_gi[0].sparkX=2; aura.m_gi[0].vecTPreX=3; aura.m_gi[0].vecTNowX=4;
        airTextureQuadCalls.clear(); Render3DAura2(aura,identity);
        const float modeEdge = 10.0f * GetCos(45);
        check(airTextureQuadCalls.size()==4&&airTextureQuadCalls[0].color==0x32FFFF37u
            &&near(airTextureQuadCalls[0].a.x,10.0f+1.0f+modeEdge)
            &&near(airTextureQuadCalls[1].a.x,10.0f+2.0f+modeEdge)
            &&near(airTextureQuadCalls[2].a.x,10.0f+3.0f+modeEdge)
            &&near(airTextureQuadCalls[3].a.x,10.0f+4.0f+modeEdge),
            "Aura2 mode three emits four visible groups in B-pre/spark/T-pre/T-now order");
        aura.m_gi[0].flag[0]=10;
        airTextureQuadCalls.clear(); Render3DAura2(aura,identity);
        check(airTextureQuadCalls.size()==4&&airTextureQuadCalls[0].color==0x32FFFFFFu
            &&airTextureQuadCalls[1].color==0x32467864u,
            "Aura2 mode ten uses the first-group white palette and remaining teal palettes");
        aura.m_gi[0].flag[0]=11;
        aura.m_gi[1].alphaT=0; aura.m_gi[0].arcBaselineY=-45.0f;
        aura.m_gi[0].sparkY=aura.m_gi[0].vecTPreY=aura.m_gi[0].vecTNowY=-1.0f;
        airTextureQuadCalls.clear(); Render3DAura2(aura,identity);
        check(airTextureQuadCalls.size()==1&&airTextureQuadCalls[0].color==0x7D9B9B9Bu,
            "Aura2 mode eleven uses its B-pre-specific -40 fade and gray palette");
        aura.m_gi[0].flag[0]=0; aura.m_gi[0].arcBaselineY=aura.m_gi[0].sparkY
            =aura.m_gi[0].vecTPreY=aura.m_gi[0].vecTNowY=1.0f;
        airTextureQuadCalls.clear(); Render3DAura2(aura,identity);
        check(airTextureQuadCalls.empty(),"Aura2 skips every group whose first transformed Y is positive");
    }
    {
        CAbleToMakeEffect player;CWorld world;world.m_player=&player;CGameMode mode;mode.m_world=&world;
        CGameMode* previousMode=g_modeMgr.current;g_modeMgr.current=&mode;g_modeMgr.lookups=0;
        CEffectPrim sma;EffectGi& gi=sma.m_gi[0];gi.life=1;gi.alphaB=180;gi.flag[0]=0;gi.process=16;
        gi.sparkX=7;gi.sparkY=8;gi.sparkZ=9;
        RenderSma(sma);
        check(player.launchCount==1&&player.lastEffectId==552&&player.lastDelta.x==0&&player.lastDelta.y==0
            &&player.lastDelta.z==0&&player.lastRotation==0,
            "SMA branch zero launches effect 552 with zero offset and rotation at process multiples of eight");
        check(player.effect.sendCount==1&&player.effect.lastMessage==14&&player.effect.lastPosition.x==7
            &&player.effect.lastPosition.y==8&&player.effect.lastPosition.z==9
            &&player.effect.lastPosition2.x==16&&player.effect.lastPosition2.y==0&&player.effect.lastPosition2.z==0,
            "SMA process sixteen sends current anchor and zeroed secondary position with message fourteen");
        player=CAbleToMakeEffect{};gi.process=8;RenderSma(sma);
        check(player.lastEffectId==552&&player.effect.sendCount==1&&player.effect.lastPosition2.x==8
            &&player.effect.lastPosition2.y==1&&player.effect.lastPosition2.z==0,
            "SMA non-sixteen multiple of eight sets secondary Y to one");
        player=CAbleToMakeEffect{};player.effectAvailable=false;RenderSma(sma);
        check(player.launchCount==1&&player.effect.sendCount==0,"SMA sends no position when effect 552 launch fails");
        player=CAbleToMakeEffect{};gi.flag[0]=1;gi.process=7;RenderSma(sma);
        check(player.launchCount==1&&player.lastEffectId==556&&player.effect.sendCount==1
            &&player.effect.lastPosition.x==7&&player.effect.lastPosition2.x==0,
            "SMA selector one launches effect 556 on every live render and sends only current position");
        player=CAbleToMakeEffect{};gi.flag[0]=2;RenderSma(sma);
        check(player.launchCount==0&&player.effect.sendCount==0,"SMA selectors other than zero and one do nothing");
        gi.flag[0]=1;gi.life=0;RenderSma(sma);
        check(player.launchCount==0&&g_modeMgr.lookups==4,"SMA exact-life gate avoids mode lookup and launch");
        g_modeMgr.current=previousMode;
    }
    {
        CEffectPrim stin;stin.m_pos={100,200,300};stin.m_texture={&g_texMgr.texture};stin.m_renderFlag=0x205;
        EffectGi& blue=stin.m_gi[0];blue.life=1;blue.alphaB=64;blue.flag[3]=0;blue.riseAngle=270;
        blue.distance=5;blue.sparkX=10;blue.sparkY=20;blue.sparkZ=30;
        EffectGi& green=stin.m_gi[1];green.life=1;green.alphaB=127;green.flag[3]=1;green.riseAngle=359;
        green.distance=2;green.sparkX=-1;green.sparkY=3;green.sparkZ=4;
        stin.m_gi[2].life=1;stin.m_gi[2].alphaB=88;stin.m_gi[2].flag[3]=2;
        stin.m_gi[3].life=2;stin.m_gi[3].alphaB=200;stin.m_gi[3].flag[3]=0;
        g_renderer.submissions.clear();g_renderer.flags.clear();
        matrix identity{};MatrixIdentity(identity);RenderStin(stin,identity);
        check(g_renderer.submissions.size()==4,"STIN emits two source triangles for each valid color lane");
        if(g_renderer.submissions.size()==4) {
            auto projectedX=[&](vector3d point){tlvertex3d value{};ProjectReferenceEffectVertex(point,identity,value);return value.x;};
            const RPFace& first=g_renderer.submissions[0];const RPFace& second=g_renderer.submissions[1];
            tlvertex3d expected{};ProjectReferenceEffectVertex({15,20,30},identity,expected);
            check(near(first.m_verts[0].x,expected.x)&&near(first.m_verts[0].y,expected.y)
                &&near(first.m_verts[0].oow,expected.oow),"STIN starts the blue band at its first one-step-wrapped radius corner");
            check(near(first.m_verts[1].x,projectedX({10,20,35}))
                &&near(first.m_verts[2].x,projectedX({5,20,30})),"STIN first triangle preserves the next two source corners");
            check(near(second.m_verts[0].x,projectedX({10,20,25}))
                &&near(second.m_verts[1].x,first.m_verts[2].x)&&near(second.m_verts[2].x,first.m_verts[0].x),
                "STIN second triangle preserves source vec3/vec1 winding");
            check(first.m_verts[0].color==0x405050FFu&&second.m_verts[2].color==0x405050FFu
                &&g_renderer.submissions[2].m_verts[0].color==0x7F50FF50u,
                "STIN packs source alpha with blue and green selector tints");
            check(first.srcAlphaMode==D3DBLEND_SRCALPHA&&first.destAlphaMode==D3DBLEND_ONE
                &&g_renderer.flags.size()==4&&g_renderer.flags[0]==0x205&&first.tex==&g_texMgr.texture,
                "STIN keeps texture zero, source-alpha/one blending and the primitive render flag");
            check(first.m_verts[0].tu==0&&first.m_verts[0].tv==1&&first.m_verts[1].tu==1&&first.m_verts[1].tv==1
                &&first.m_verts[2].tu==1&&first.m_verts[2].tv==0&&second.m_verts[0].tu==0&&second.m_verts[0].tv==0,
                "STIN preserves both RenderTeiRect triangle UV orders");
            check(near(g_renderer.submissions[2].m_verts[0].x,projectedX({-1+2*GetCos(89),3,4+2*GetSin(89)})),
                "STIN subtracts 360 once from the 359-degree initial angle step");
        }
        check(stin.m_pos.x==100&&blue.riseAngle==270&&green.riseAngle==359,
            "STIN renderer does not mutate primitive position or GI angles");
        stin.m_texture.clear();g_renderer.submissions.clear();RenderStin(stin,identity);
        check(g_renderer.submissions.size()==4&&g_renderer.submissions[0].tex==nullptr,
            "STIN follows canonical executable behavior and does not invent a missing-texture guard");
        stin.m_texture={&CTexMgr::s_dummy_texture};g_renderer.submissions.clear();RenderStin(stin,identity);
        check(g_renderer.submissions.size()==4&&g_renderer.submissions[0].tex==&CTexMgr::s_dummy_texture,
            "STIN submits the dummy texture as canonical executable RenderTeiRect does");
    }
    {
        CEffectPrim stin5;stin5.m_pos={100,200,300};stin5.m_texture={&g_texMgr.texture};stin5.m_renderFlag=0x205;
        EffectGi& active=stin5.m_gi[0];active.life=1;active.alphaB=150;active.riseAngle=359;active.rotStart=90;
        active.distance=4;active.sparkX=5;active.sparkY=6;active.sparkZ=7;
        stin5.m_gi[1].life=1;stin5.m_gi[1].alphaB=0;
        stin5.m_gi[2].life=0;stin5.m_gi[2].alphaB=80;
        EffectGi& other=stin5.m_gi[3];other.life=1;other.alphaB=20;other.riseAngle=270;other.rotStart=0;other.distance=2;
        g_renderer.submissions.clear();g_renderer.flags.clear();
        matrix identity{};MatrixIdentity(identity);RenderStin5(stin5,identity);
        check(g_renderer.submissions.size()==4,"STIN5 emits two fixed-green source triangles per live positive-alpha lane");
        if(g_renderer.submissions.size()==4) {
            const int order[6]={0,1,2,3,2,0};int angle=active.riseAngle;
            vector3d expectedCorners[4]{};
            for(int i=0;i<4;++i) {
                angle+=90;if(angle>=360)angle-=360;
                const double radial=static_cast<double>(GetCos(angle))*active.distance;
                expectedCorners[i]={static_cast<float>(GetCos(active.rotStart)*radial+active.sparkX),
                    static_cast<float>(static_cast<double>(GetSin(angle))*active.distance+active.sparkY),
                    static_cast<float>(static_cast<double>(GetSin(active.rotStart))*radial+active.sparkZ)};
            }
            for(int i=0;i<6;++i) {
                tlvertex3d expected{};ProjectReferenceEffectVertex(expectedCorners[order[i]],identity,expected);
                const RPFace& face=g_renderer.submissions[static_cast<size_t>(i/3)];
                const tlvertex3d& actual=face.m_verts[i%3];
                check(near(actual.x,expected.x)&&near(actual.y,expected.y)&&near(actual.oow,expected.oow),
                    "STIN5 rotates each cosine radius around RotStart while sine controls vertical height");
            }
            check(g_renderer.submissions[0].m_verts[0].color==0x9650FF50u
                &&g_renderer.submissions[2].m_verts[0].color==0x1450FF50u,
                "STIN5 uses one fixed green tint with each lane alpha");
            check(g_renderer.submissions[0].srcAlphaMode==D3DBLEND_SRCALPHA
                &&g_renderer.submissions[0].destAlphaMode==D3DBLEND_ONE
                &&g_renderer.submissions[0].tex==&g_texMgr.texture
                &&g_renderer.flags.size()==4&&g_renderer.flags[0]==0x205,
                "STIN5 reuses canonical RenderTeiRect texture blend and primitive render flag");
            check(g_renderer.submissions[0].m_verts[0].tu==0&&g_renderer.submissions[0].m_verts[0].tv==1
                &&g_renderer.submissions[0].m_verts[1].tu==1&&g_renderer.submissions[0].m_verts[2].tv==0,
                "STIN5 retains the shared source triangle UV order");
        }
        check(stin5.m_pos.x==100&&active.riseAngle==359&&active.rotStart==90,
            "STIN5 rendering leaves primitive position and lane angles unchanged");
    }
    {
        CEffectPrim sma2;sma2.m_pos={100,200,300};sma2.m_deltaPos={3,4,5};
        sma2.m_texture={&g_texMgr.texture};sma2.m_renderFlag=0x205;
        EffectGi& gi=sma2.m_gi[0];gi.life=1;gi.alphaB=0;gi.fullDisplayAngle=360;gi.rotStart=37;gi.riseAngle=55;gi.distance=2.4f;
        for(int i=0;i<21;++i)gi.height[static_cast<size_t>(i)]=static_cast<float>(i+1);
        sma2.m_gi[1].life=0;sma2.m_gi[1].alphaB=180;sma2.m_gi[1].fullDisplayAngle=360;
        g_renderer.submissions.clear();g_renderer.flags.clear();
        matrix identity{};MatrixIdentity(identity);RenderSma2(sma2,identity);
        check(g_renderer.submissions.size()==40,"SMA2 draws twenty quads for the active 360-degree lane without an alpha gate");
        auto sample=[&](int count,int order) {
            int angle=(count+gi.rotStart)%360;if(gi.fullDisplayAngle==360&&order==20)angle=gi.rotStart;
            const float cosine=GetCos(angle),sine=GetSin(angle);
            const vector3d base{static_cast<float>(cosine*gi.distance),-0.0f,static_cast<float>(sine*gi.distance)};
            const double radial=static_cast<double>(GetCos(gi.riseAngle))*gi.height[static_cast<size_t>(order)];
            const double vertical=static_cast<double>(GetSin(gi.riseAngle))*gi.height[static_cast<size_t>(order)];
            const float radialForZ=static_cast<float>(radial);
            const vector3d top{static_cast<float>(static_cast<double>(base.x)+cosine*radial),static_cast<float>(-vertical),
                static_cast<float>(static_cast<double>(base.z)+sine*radialForZ)};
            return std::pair<vector3d,vector3d>{{base.x+sma2.m_deltaPos.x,base.y+sma2.m_deltaPos.y,base.z+sma2.m_deltaPos.z},
                {top.x+sma2.m_deltaPos.x,top.y+sma2.m_deltaPos.y,top.z+sma2.m_deltaPos.z}};
        };
        if(g_renderer.submissions.size()==40) {
            const auto start=sample(0,0),end=sample(18,1);
            const vector3d expected[6]={start.first,end.first,end.second,start.second,end.second,start.first};
            const int faceOrder[6]={0,1,2,3,4,5};
            for(int i=0;i<6;++i) {
                tlvertex3d projected{};ProjectReferenceEffectVertex(expected[faceOrder[i]],identity,projected);
                const RPFace& face=g_renderer.submissions[static_cast<size_t>(i/3)];
                const tlvertex3d& actual=face.m_verts[i%3];
                check(near(actual.x,projected.x)&&near(actual.y,projected.y)&&near(actual.oow,projected.oow),
                    "SMA2 first ribbon quad follows base/radial-height/vertical-height geometry and delta position");
            }
            const RPFace& first=g_renderer.submissions[0];const RPFace& second=g_renderer.submissions[1];
            check(first.m_verts[0].color==0x005A5AFFu&&first.srcAlphaMode==D3DBLEND_SRCALPHA
                &&first.m_verts[0].specular==0xFF000000u&&first.destAlphaMode==D3DBLEND_ONE
                &&first.tex==&g_texMgr.texture&&g_renderer.flags.size()==40&&g_renderer.flags[0]==0x205,
                "SMA2 retains fixed blue tint zero alpha source-alpha/one blend texture and render flag");
            check(near(first.m_verts[0].tu,0)&&first.m_verts[0].tv==1
                &&near(first.m_verts[1].tu,1.0f/21.0f)&&first.m_verts[1].tv==1
                &&near(first.m_verts[2].tu,1.0f/21.0f)&&first.m_verts[2].tv==0
                &&second.m_verts[0].tu==0&&second.m_verts[0].tv==0,
                "SMA2 advances source U fractions by order and preserves both ribbon triangle UV layouts");
            const auto seam=sample(360,20);
            tlvertex3d projectedSeam{};ProjectReferenceEffectVertex(seam.first,identity,projectedSeam);
            const RPFace& finalFace=g_renderer.submissions[38];
            check(near(finalFace.m_verts[1].x,projectedSeam.x)&&near(finalFace.m_verts[1].y,projectedSeam.y)
                &&near(finalFace.m_verts[1].oow,projectedSeam.oow),"SMA2 full-circle final segment closes at RotStart");
        }
        check(gi.fullDisplayAngle==360&&gi.rotStart==37&&sma2.m_pos.x==100,
            "SMA2 renderer does not mutate GI angles or use primitive world position");
        gi.fullDisplayAngle=315;g_renderer.submissions.clear();RenderSma2(sma2,identity);
        check(g_renderer.submissions.size()==42
            &&near(g_renderer.submissions[40].m_verts[0].tu,19.0f/21.0f)
            &&near(g_renderer.submissions[40].m_verts[1].tu,20.0f/21.0f),
            "SMA2 partial 315-degree range retains its truncated 15-degree steps and capped final U band");
        gi.fullDisplayAngle=-1;g_renderer.submissions.clear();RenderSma2(sma2,identity);
        check(g_renderer.submissions.empty(),"SMA2 negative full angle emits no samples");
    }
    {
        CEffectPrim bash;bash.m_pos={11,20,30};bash.m_renderFlag=0x205;bash.m_texture={&g_texMgr.texture};
        const int modes[4]={2,3,4,0};
        for(int lane=0;lane<4;++lane) {
            EffectGi& gi=bash.m_gi[static_cast<size_t>(lane)];
            gi.life=1;gi.alphaB=static_cast<short>(100-lane*10);gi.riseAngle=359;gi.rotStart=0;gi.distance=10;
            gi.height[0]=2;gi.height[4]=static_cast<float>(modes[lane]);
        }
        g_renderer.submissions.clear();g_renderer.flags.clear();
        matrix identity{};MatrixIdentity(identity);RenderBash3D(bash,identity);
        check(g_renderer.submissions.size()==16,"BASH3D submits narrow and wide two-triangle passes for four active lanes");
        if(g_renderer.submissions.size()==16) {
            const unsigned int narrowColors[4]={0x640000FAu,0x5AFAFAFAu,0x500000FAu,0x4600FAFAu};
            const unsigned int wideColors[4]={0x64FAFA00u,0x5AFAFAFAu,0x50FAFA00u,0x46FA0000u};
            for(int lane=0;lane<4;++lane) {
                const RPFace& narrow=g_renderer.submissions[static_cast<size_t>(lane*4)];
                const RPFace& wide=g_renderer.submissions[static_cast<size_t>(lane*4+2)];
                check(narrow.m_verts[0].color==narrowColors[lane]&&wide.m_verts[0].color==wideColors[lane],
                    "BASH3D mode selects exact narrow and wide pass tint with lane alpha");
                check(narrow.destAlphaMode==D3DBLEND_ONE
                    &&wide.destAlphaMode==(lane==1||lane==2?D3DBLEND_INVSRCALPHA:D3DBLEND_ONE)
                    &&g_renderer.flags[static_cast<size_t>(lane*4)]==0x205,
                    "BASH3D preserves pass blend selector and source render flags");
            }
            const float lowRadius=GetCos(358)*10.0f;
            const vector3d low{bash.m_pos.x+GetCos(0)*lowRadius,bash.m_pos.y+GetSin(0)*lowRadius+2,bash.m_pos.z+GetSin(358)*10.0f};
            const float highRadius=GetCos(0)*10.0f;
            const vector3d high{bash.m_pos.x+GetCos(0)*highRadius,bash.m_pos.y+GetSin(0)*highRadius+2,bash.m_pos.z+GetSin(0)*10.0f};
            tlvertex3d projectedLow{},projectedHigh{};
            ProjectReferenceEffectVertex(low,identity,projectedLow);ProjectReferenceEffectVertex(high,identity,projectedHigh);
            check(near(g_renderer.submissions[0].m_verts[0].x,projectedLow.x)
                &&near(g_renderer.submissions[0].m_verts[0].y,projectedLow.y)
                &&near(g_renderer.submissions[0].m_verts[0].oow,projectedLow.oow)
                &&near(g_renderer.submissions[0].m_verts[1].x,projectedHigh.x)
                &&near(g_renderer.submissions[0].m_verts[1].y,projectedHigh.y)
                &&near(g_renderer.submissions[0].m_verts[1].oow,projectedHigh.oow),
                "BASH3D endpoints preserve wrapped rise angles, RotStart radius rotation and primitive position");
            check(g_renderer.submissions[0].tex==&g_texMgr.texture
                &&g_renderer.submissions[0].m_verts[0].tu==0
                &&g_renderer.submissions[0].m_verts[0].tv==1,
                "BASH3D uses texture0 and full source UV range");
        }
        bash.m_gi[3].height[4]=5;g_renderer.submissions.clear();RenderBash3D(bash,identity);
        check(g_renderer.submissions.size()==16&&g_renderer.submissions[12].m_verts[0].color==0x46323232u
            &&g_renderer.submissions[14].m_verts[0].color==0x46FAFAFAu
            &&g_renderer.submissions[14].destAlphaMode==D3DBLEND_INVSRCALPHA,
            "BASH3D mode five keeps gray narrow and white inverse-alpha wide passes");
        bash.m_gi[0].height[4]=2.5f;g_renderer.submissions.clear();RenderBash3D(bash,identity);
        check(g_renderer.submissions.size()==16&&g_renderer.submissions[0].m_verts[0].color==0x6400FAFAu
            &&g_renderer.submissions[2].m_verts[0].color==0x64FA0000u,
            "BASH3D fractional selectors take the source default tint branches");
        bash.m_gi[3].alphaB=0;g_renderer.submissions.clear();RenderBash3D(bash,identity);
        check(g_renderer.submissions.size()==12,"BASH3D skips lanes with nonpositive alpha");
        bash.m_gi[3].alphaB=40;bash.m_gi[3].life=0;g_renderer.submissions.clear();RenderBash3D(bash,identity);
        check(g_renderer.submissions.size()==12,"BASH3D skips inactive lanes");
        bash.m_gi[3].life=1;bash.m_texture.clear();g_renderer.submissions.clear();RenderBash3D(bash,identity);
        check(g_renderer.submissions.size()==16&&g_renderer.submissions[0].tex==nullptr,
            "BASH3D follows canonical submission when texture0 is null");
        bash.m_texture={&CTexMgr::s_dummy_texture};g_renderer.submissions.clear();RenderBash3D(bash,identity);
        check(g_renderer.submissions.size()==16&&g_renderer.submissions[0].tex==&CTexMgr::s_dummy_texture,
            "BASH3D follows canonical submission when texture0 is dummy");
    }
    {
        CEffectPrim air;
        air.m_texture={&g_texMgr.texture};
        air.m_gi[0].life=1; air.m_gi[0].alphaB=100; air.m_gi[0].distance=2.0f;
        air.m_gi[0].riseAngle=90; air.m_gi[0].sparkX=10.0f; air.m_gi[0].sparkY=20.0f; air.m_gi[0].sparkZ=30.0f;
        air.m_gi[1].life=1; air.m_gi[1].alphaB=0; air.m_gi[1].distance=3.0f;
        airTextureQuadCalls.clear();
        matrix view{}; RenderAirTexture(air,view);
        check(airTextureQuadCalls.size()==1,"air texture renderer draws only active positive-alpha GI lanes");
        if(airTextureQuadCalls.size()==1) {
            const auto& quad=airTextureQuadCalls[0];
            check(quad.texture==&g_texMgr.texture&&quad.color==0x643C3CFFu&&quad.blend==D3DBLEND_ONE,
                "air texture renderer uses texture slot zero, source blue tint and additive blend");
            check(near(quad.b.x,10.0f)&&near(quad.b.y,20.0f)&&near(quad.b.z,28.0f),
                "air texture renderer applies rise rotation and lane position to the second corner");
        }
        air.m_gi[1].alphaB=1;
        airTextureQuadCalls.clear(); RenderAirTexture(air,view);
        check(airTextureQuadCalls.size()==2,"air texture renderer emits one quad for each active lane");
    }
    {
        CEffectPrim hit;
        hit.m_texture={&g_texMgr.texture};
        auto& gi=hit.m_gi[0];
        gi.life=1; gi.alphaB=100; gi.alphaT=0; gi.process=1; gi.distance=90.0f;
        gi.maxHeight=2.0f; gi.vecTNowX=1.0f; gi.vecTNowY=1.0f; gi.height[0]=0.0f; gi.height[1]=30.0f;
        hit.m_gi[1].life=1; hit.m_gi[1].alphaB=100; hit.m_gi[1].process=0;
        airTextureQuadCalls.clear();
        matrix view{}; RenderHitLine3(hit,view);
        check(airTextureQuadCalls.size()==3,"hitline3 emits three narrow ring strips for one segment and skips nonpositive process");
        if(!airTextureQuadCalls.empty()) {
            check(airTextureQuadCalls[0].color==0x64FFC800u&&airTextureQuadCalls[0].blend==D3DBLEND_ONE,
                "hitline3 mode zero uses source segment tint, alpha and additive blend");
        }
        gi.alphaT=3; airTextureQuadCalls.clear(); RenderHitLine3(hit,view);
        check(airTextureQuadCalls.size()==6,"hitline3 mode three emits narrow and wide rings");
        if(airTextureQuadCalls.size()==6) {
            check(airTextureQuadCalls[3].color==0x320000FFu,
                "hitline3 wide ring halves segment alpha and uses blue tint");
        }
        gi.alphaT=4; airTextureQuadCalls.clear(); RenderHitLine3(hit,view);
        check(airTextureQuadCalls.size()==3&&airTextureQuadCalls[0].color==0x64696969u,
            "hitline3 higher modes draw grayscale narrow rings without the mode-three wide pass");
    }
    {
        CEffectPrim soul;
        soul.m_texture={&g_texMgr.texture}; soul.m_pos={1.0f,2.0f,3.0f};
        auto& gi=soul.m_gi[0];
        gi.life=1; gi.alphaB=0; gi.distance=2.0f; gi.process=179;
        gi.height[10]=42.0f; gi.height[13]=77.0f;
        gi.vecBPreX=3.0f; gi.arcBaselineY=-1.0f; gi.vecBPreZ=9.0f;
        CWorld world; CAbleToMakeEffect player; world.m_player=&player;
        CGameMode mode; mode.m_world=&world; g_modeMgr.current=&mode; g_session.m_aid=42;
        matrix view{}; airTextureQuadCalls.clear(); RenderSoulLink(soul,view);
        check(airTextureQuadCalls.size()==1&&airTextureQuadCalls[0].color==0x4D7D7DFFu
            &&airTextureQuadCalls[0].blend==D3DBLEND_ONE,
            "soul link draws a live lane despite zero alpha using the source additive tint");
        check(player.launchCount==1&&player.lastEffectId==506&&player.effect.sendCount==1
            &&player.effect.lastMessage==14&&near(player.effect.lastPosition.x,3.0f)
            &&near(player.effect.lastPosition.y,-1.0f)&&near(player.effect.lastPosition.z,0.0f),
            "soul link self-AID path launches SoulLight and sends its projected position");
        gi.process=180; RenderSoulLink(soul,view);
        check(player.launchCount==1&&airTextureQuadCalls.size()==2,
            "soul link process 180 stops effect launches while retaining the visible quad");
        CAbleToMakeEffect other; world.actorAid=77; world.m_actor=&other;
        gi.process=10; gi.height[10]=77.0f; RenderSoulLink(soul,view);
        check(other.launchCount==1&&other.effect.sendCount==1,
            "soul link resolves non-self AID and sends position to the actor effect");
        g_session.m_aid=16777217u; world.actorAid=16777216u; gi.height[10]=16777216.0f;
        const int precisionLaunches=other.launchCount; RenderSoulLink(soul,view);
        check(other.launchCount==precisionLaunches+1,
            "soul link compares high self AIDs in double precision before actor lookup");
        g_session.m_aid=42;
        const int actorLaunchesBeforeMissing=other.launchCount;
        gi.height[10]=88.0f; RenderSoulLink(soul,view);
        check(other.launchCount==actorLaunchesBeforeMissing,"soul link missing actor skips only the side effect");
        gi.height[10]=42.0f; player.effectAvailable=false; const int sentBefore=player.effect.sendCount;
        RenderSoulLink(soul,view);
        check(player.launchCount==2&&player.effect.sendCount==sentBefore,
            "soul link failed effect creation omits the position message");
        gi.arcBaselineY=1.0f; const size_t beforeHidden=airTextureQuadCalls.size();
        const int launchesBefore=player.launchCount; RenderSoulLink(soul,view);
        check(airTextureQuadCalls.size()==beforeHidden&&player.launchCount==launchesBefore,
            "soul link positive-Y corner gate suppresses both render and launch");
        g_modeMgr.current=nullptr;
    }
    {
        CEffectPrim soulParticle;
        soulParticle.m_texture={&g_texMgr.texture}; soulParticle.m_pos={0.0f,3.0f,0.0f};
        auto& gi=soulParticle.m_gi[0];
        gi.life=1; gi.process=0; gi.distance=2.0f; gi.height[15]=2.0f;
        gi.height[12]=-8.0f; gi.height[13]=50.0f; gi.height[14]=44.0f;
        gi.arcBaselineY=-1.0f; gi.sparkY=1.0f; gi.vecTPreY=0.0f;
        airTextureQuadCalls.clear(); matrix view{}; RenderSoulParticle(soulParticle,view);
        check(airTextureQuadCalls.size()==2,"soul particle independently gates the three source position copies");
        if(airTextureQuadCalls.size()==2) {
            check(airTextureQuadCalls[0].color==0x00FFA0A0u&&airTextureQuadCalls[1].color==0x2CFFA0A0u,
                "soul particle clamps negative alpha to zero and keeps per-copy alpha values");
            check(airTextureQuadCalls[0].blend==D3DBLEND_ONE&&airTextureQuadCalls[0].texture==&g_texMgr.texture,
                "soul particle uses source additive blend and texture slot zero");
            check(near(airTextureQuadCalls[0].a.y,2.0f),
                "soul particle adds primitive position after applying its per-copy visibility gate");
        }
        gi.height[15]=1.0f; gi.sparkY=-1.0f; gi.vecTPreY=2.0f;
        airTextureQuadCalls.clear(); RenderSoulParticle(soulParticle,view);
        check(airTextureQuadCalls.size()==2&&airTextureQuadCalls[0].color==0x00FFC8C8u
            &&airTextureQuadCalls[1].color==0x32FFC8C8u,
            "soul particle mode one tint applies to the visible first and second copies");
        gi.height[15]=4.0f; airTextureQuadCalls.clear(); RenderSoulParticle(soulParticle,view);
        check(airTextureQuadCalls.empty(),"soul particle unsupported tint mode submits no copies");
    }
    {
        CEffectPrim hermode;
        hermode.m_texture={&g_texMgr.texture}; hermode.m_pos={1.0f,2.0f,3.0f};
        auto& gi=hermode.m_gi[0];
        gi.life=1; gi.rotStart=0; gi.distance=10.0f; gi.riseAngle=90; gi.height[0]=3.0f;
        gi.alphaB=100; gi.flag[0]=40;
        airTextureQuadCalls.clear(); matrix view{}; RenderHermode(hermode,view);
        check(airTextureQuadCalls.size()==6,"hermode submits all six ordered faces for a live lane");
        if(airTextureQuadCalls.size()==6) {
            check(airTextureQuadCalls[0].color==0x642828FAu&&airTextureQuadCalls[1].color==0x642828DCu
                &&airTextureQuadCalls[2].color==0x642828BEu&&airTextureQuadCalls[3].color==0x64282882u
                &&airTextureQuadCalls[4].color==0x642828A0u&&airTextureQuadCalls[5].color==0x642828FAu,
                "hermode face order preserves source blue-channel palette");
            check(near(airTextureQuadCalls[0].a.x,11.0f)&&near(airTextureQuadCalls[0].a.y,5.0f)
                &&near(airTextureQuadCalls[0].b.z,13.0f)&&near(airTextureQuadCalls[0].c.x,-9.0f)
                &&near(airTextureQuadCalls[5].a.y,-5.0f),
                "hermode uses source angle/radius vertices, bottom offset and primitive translation");
            check(airTextureQuadCalls[0].blend==D3DBLEND_ONE&&airTextureQuadCalls[0].texture==&g_texMgr.texture,
                "hermode faces use source additive blend and texture slot zero");
        }
        hermode.m_gi[0].life=0; airTextureQuadCalls.clear(); RenderHermode(hermode,view);
        check(airTextureQuadCalls.empty(),"hermode skips inactive GI lanes");
    }
    const std::pair<float,u32> castingColors[]={
        {0.0f,0x00FFFFFFu},{1.0f,0x00FFAFAFu},{2.0f,0x00C3C3FFu},{3.0f,0x00FFFFFFu},
        {4.0f,0x006464FFu},{5.0f,0x00FFFFFFu},{6.0f,0x00FF6464u},{7.0f,0x0064FF64u},
        {8.0f,0x00FFFFAAu},{9.0f,0x008CD28Cu},{10.0f,0x00FF59B6u},{11.0f,0x00AAAAFFu},
        {12.0f,0x00323232u},{13.0f,0x00FF32FFu},{14.0f,0x007878FFu},{15.0f,0x000AFF0Au},
        {100.0f,0x00FAFAFFu}
    };
    for(const auto& item:castingColors) check(CastingSizeRgb(item.first)==item.second,"casting size tint matches original RenderTeiRect case table");
    check(CastingSizeAlpha(2,175)==175 && CastingSizeAlpha(3,175)==80,
        "casting size three fixes alpha to80 while other sizes retain band alpha");
    for(const auto& item:std::array<std::pair<int,float>,4>{{{0,100.0f},{22,10.0f},{45,5.0f},{90,3.0f}}}) {
        double radialExtended=0;float radialStored=0,vertical=0;
        CastingLowPolygonRise(item.first,item.second,&radialExtended,&radialStored,&vertical);
        check(near(radialExtended,static_cast<double>(GetSin(item.first*4))*item.second)
            && near(radialStored,static_cast<float>(radialExtended))
            && near(vertical,-static_cast<double>(GetCos(item.first*4))*item.second),
            "low polygon casting uses source four-times-angle sine radial and cosine vertical tables");
    }
    for(int band=0;band<4;++band) {
        vector3d inner{},outer{};
        const int angle=band*45+30, rise=band*20+10;
        const float height=7.25f,distance=4.5f;
        CastingGi3Sample(band,angle,rise,height,distance,&inner,&outer);
        const int rawSide=std::array<int,4>{{450,270,90,630}}[band]-angle;
        const int side=(rawSide%360+360)%360;
        const double radius=static_cast<double>(GetCos(rise))*height;
        check(near(inner.x,GetCos(angle)*distance)&&near(inner.z,GetSin(angle)*distance)
            && near(outer.x,static_cast<double>(GetCos(side))*radius+inner.x)
            && near(outer.y,-static_cast<double>(GetSin(rise))*height)
            && near(outer.z,static_cast<double>(GetSin(side))*radius+inner.z),
            "GI3 casting sample matches source band rotation and radial/vertical branches");
    }
    for(int rise:{0,23,45,90}) {
        vector3d point{};const float angle=135.0f,heightFactor=0.37f,distance=8.0f;
        ElectricPathPoint(rise,angle,heightFactor,distance,12.0f,30.0f,-4.0f,0.1f,-0.2f,0.3f,&point);
        const double radial=static_cast<double>(angle)*0.011111111*distance;
        const int angleIndex=static_cast<int>(angle);
        check(near(point.x,static_cast<float>(radial*GetCos(rise)+0.1f+12.0f))
            && near(point.y,static_cast<float>(static_cast<double>(-GetSin(angleIndex))*heightFactor*distance-0.2f-2.0f+30.0f))
            && near(point.z,static_cast<float>(radial*GetSin(rise)+0.3f-4.0f)),
            "electric path sample uses the source one-degree rise table, independent jitter and origin");
    }
    for(int rise:{0,23,45,90}) {
        vector3d point{};const int sample=4;const float angle=135.0f,heightFactor=0.37f,distance=8.0f;
        Electric2PathPoint(rise,sample,angle,heightFactor,distance,12.0f,30.0f,-4.0f,0.1f,-0.2f,0.3f,&point);
        const double radial=static_cast<double>(sample)*0.2*distance;
        check(near(point.x,static_cast<float>(radial*GetCos(rise)+0.1f+12.0f))
            && near(point.y,static_cast<float>(static_cast<double>(GetSin(static_cast<int>(angle)))*heightFactor*-15.0-0.2f-8.0+30.0f))
            && near(point.z,static_cast<float>(radial*GetSin(rise)+0.3f-4.0f)),
            "electric2 path sample uses the source one-degree rise table and vertical scale");
    }
    for(int rise:{0,23,45,90}) {
        vector3d base{},top{};const int angle=rise*2+30;const float height=6.25f,distance=8.0f,maxHeight=2.5f;
        WindRenderSample(angle,rise,height,distance,maxHeight,&base,&top);
        const float radial=GetCos(rise)*height,vertical=GetSin(rise)*height;
        check(near(base.x,GetCos(angle)*distance)&&near(base.y,-maxHeight)&&near(base.z,GetSin(angle)*distance)
            && near(top.x,static_cast<double>(radial)*GetCos(angle)+base.x)
            && near(top.y,-(static_cast<double>(vertical)+maxHeight))
            && near(top.z,static_cast<double>(radial)*GetSin(angle)+base.z),
            "wind casting sample matches source base/rise geometry and source float stores");
    }
    for(int angle:{0,90,180,270}) {
        vector3d corners[4]{};const float horizontal=23.0f,diagonal=-23.0f;
        GrandCross2Corners(angle,horizontal,diagonal,corners);
        const double c=GetCos(angle),s=GetSin(angle),hx=horizontal*c,hz=horizontal*s,dx=diagonal*c,dz=diagonal*s;
        check(near(corners[0].x,hx-dz)&&near(corners[0].y,-1.0f)&&near(corners[0].z,hz+dx)
            && near(corners[1].x,hx+dz)&&near(corners[1].z,hz-dx)
            && near(corners[2].x,-hx+dz)&&near(corners[2].z,-hz-dx)
            && near(corners[3].x,-hx-dz)&&near(corners[3].z,-hz+dx),
            "Grand Cross2 four rotated corners match source quadrant formula");
    }
    for(int around=0;around<=1440;around+=144) {
        vector3d point{};const vector3d center{1.0f,2.0f,3.0f};const float radius=0.4f;const int side=135;
        LineLinkRingPoint(center,radius,around,side,&point);
        const int ringAngle=around/4;
        const float horizontal=radius*GetCos(ringAngle);
        check(near(point.x,center.x+horizontal*GetCos(side))
            && near(point.y,center.y+radius*GetSin(ringAngle))
            && near(point.z,center.z+horizontal*GetSin(side)),
            "LineLink ring sample follows source 144-byte/36-degree table stepping");
    }
    for(int process:{0,7,14,15,24}) {
        const vector3d current{2.0f,3.0f,4.0f},previous{8.0f,9.0f,10.0f};vector3d endpoint{};
        LineLinkInterpolatedEnd(current,previous,process,&endpoint);
        const double wave=process>=15?1.0:GetSin(6*process);
        check(near(endpoint.x,process>=15?previous.x:static_cast<float>((previous.x-current.x)*wave+current.x))
            && near(endpoint.y,process>=15?previous.y:static_cast<float>((previous.y-current.y)*wave+current.y))
            && near(endpoint.z,process>=15?previous.z:static_cast<float>((previous.z-current.z)*wave+current.z)),
            "LineLink endpoint follows source six-times-process interpolation and process-15 cutoff");
    }
    check(LineLinkHeading(1.0f,0.0f)==0 && LineLinkHeading(0.0f,1.0f)==90
        && LineLinkHeading(-1.0f,0.0f)==180 && LineLinkHeading(0.0f,-1.0f)==270,
        "LineLink heading search matches all four cardinal directions");
    for(int angle:{0,30,90,180,270}) {
        EffectGi gi{};gi.height[1]=12.0f;gi.height[2]=37.0f;gi.rotStart=static_cast<short>(angle);
        vector3d corners[4]{};IntimidateCorners(gi,corners);
        for(int i=0;i<4;++i) {
            const int rotation=(angle+i*90)%360;
            const double radial=GetCos(rotation)*gi.height[1];
            check(near(corners[i].x,GetCos(37)*radial)&&near(corners[i].y,GetSin(rotation)*gi.height[1])
                && near(corners[i].z,GetSin(37)*radial),
                "Intimidate ring corner matches source rotational and pitch formula");
        }
    }
    {
        EffectGi gi{};gi.height[1]=5.0f;gi.height[2]=70.0f;gi.rotStart=315;gi.flag[0]=2;
        vector3d corners[4]{};IntimidateCorners(gi,corners);
        check(near(corners[0].x,GetCos(315)*5.0)&&near(corners[0].y,0.0f)&&near(corners[0].z,GetSin(315)*5.0)
            && near(corners[1].x,GetCos(45)*5.0)&&near(corners[1].z,GetSin(45)*5.0),
            "Intimidate flag-2 rebuilds a flat ring before screen-facing rotation");
        IntimidateTowardScreen(corners,35,20);
        const double y=0.0,z=GetSin(315)*5.0;
        const double rotatedZ=z*GetCos(20)-y*GetSin(20);
        check(near(corners[0].z,GetCos(315)*5.0*GetSin(35)+rotatedZ*GetCos(35))
            && near(corners[0].x,GetCos(315)*5.0*GetCos(35)-rotatedZ*GetSin(35)),
            "Intimidate screen-facing branch applies source YZ then XZ rotations in order");
    }
    {
        EffectGi gi{};gi.distance=9.0f;gi.fullDisplayAngle=120;gi.riseAngle=35;
        const vector3d center=IntimidateCenter(gi);
        const double fullX=GetCos(120)*9.0;
        check(near(center.x,GetCos(35)*fullX)&&near(center.y,GetSin(120)*9.0)
            && near(center.z,GetSin(35)*fullX),
            "Intimidate band center uses full-display then rise rotation");
    }
    for(int fullAngle:{0,45,135,270}) for(int flag:{0,1}) {
        EffectGi gi{};gi.height[1]=3.25f;gi.distance=17.5f;gi.fullDisplayAngle=static_cast<short>(fullAngle);
        gi.riseAngle=28;gi.flag[1]=static_cast<unsigned char>(flag);
        vector3d corners[4]{};TeiHit1Corners(gi,corners);
        const double c=GetCos(fullAngle),s=GetSin(fullAngle),cr=GetCos(28),sr=GetSin(28),h=gi.height[1];
        const double left=gi.distance+flag*h,right=gi.distance-flag*h;
        const double x[4]={c*left+s*h,c*left-s*h,c*right-s*h,c*right+s*h};
        const double y[4]={s*left-c*h,s*left+c*h,s*right+c*h,s*right-c*h};
        for(int i=0;i<4;++i) check(near(corners[i].x,static_cast<float>(static_cast<double>(static_cast<float>(x[i]))*cr))
            && near(corners[i].y,static_cast<float>(y[i]))
            && near(corners[i].z,static_cast<float>(static_cast<double>(static_cast<float>(x[i]))*sr)),
            "TeiHit1 corner matches source offset, perpendicular, and rise rotations");
    }
    struct ExpectedCloudStyle { int pattern,variant,frame;bool reverse;unsigned int rgb;bool valid; };
    const ExpectedCloudStyle cloudStyles[]={
        {0,0,1,false,0x00FFFFFFu,true},{1,0,0,false,0x009696FAu,true},
        {2,0,0,true,0x008282FAu,true},{3,0,0,true,0x00C8C864u,true},
        {4,0,1,true,0x00FFFFFFu,true},{5,0,0,false,0,false},{6,0,0,false,0,false},
        {7,0,0,true,0x00FFFFFFu,true},{8,0,1,true,0x0087FF87u,true},
        {9,1,1,true,0x00FF8787u,true},{9,2,1,true,0x00FFCD87u,true},
        {9,3,1,true,0x00FFFF87u,true},{9,4,1,true,0x00FFFFFFu,true},
        {9,5,1,true,0x008787FFu,true},{9,6,0,true,0x00FFFFFFu,true},
        {9,7,0,false,0,false},{10,0,0,true,0x00FAFA96u,true},
        {11,0,0,true,0x00DCFADCu,true},{12,0,1,true,0x00FCAB8Fu,true},
        {13,0,0,true,0x00FA6464u,true},{14,0,1,true,0x005E0000u,true},
        {15,0,1,false,0x00FFC8FFu,true},{16,0,1,true,0x00000000u,true},
        {17,0,1,true,0x00FFB4B4u,true}
    };
    for(const auto& expected:cloudStyles) {
        EffectGi gi{};gi.flag[2]=static_cast<unsigned char>(expected.pattern);gi.flag[3]=static_cast<unsigned char>(expected.variant);
        const auto style=CloudStyle(gi);
        check(style.valid==expected.valid && style.texturePart==expected.frame && style.reverse==expected.reverse
            && style.rgb==expected.rgb,"RenderCloud pattern and nested color/frame dispatch matches source switch");
    }
    for(int fullAngle:{0,45,180,315}) for(int phase:{0,45,270}) {
        EffectGi gi{};gi.distance=12.5f;gi.fullDisplayAngle=static_cast<short>(fullAngle);gi.height[8]=static_cast<float>(phase);
        gi.height[9]=10.0f;vector3d corners[4]{};CloudCorners(gi,corners);
        const double radius=gi.distance+GetSin(phase)*gi.distance*0.050000001;
        for(int i=0;i<4;++i) {
            const int angle=(fullAngle+90+i*90)%360;
            check(near(corners[i].x,static_cast<float>(GetCos(angle)*radius+4.0))
                && near(corners[i].y,0.0f)&&near(corners[i].z,static_cast<float>(GetSin(angle)*radius)),
                "RenderCloud pulse ring follows source angle, size, and fixed-height offset");
        }
    }
    for(float invalidPhase:{-1.0f,360.0f}) {
        EffectGi gi{};gi.distance=2.0f;gi.height[8]=invalidPhase;vector3d corners[4]{};
        CloudCorners(gi,corners);
        check(gi.height[8]==0.0f && near(corners[0].x,static_cast<float>(GetCos(90)*2.0))
            && near(corners[0].z,static_cast<float>(GetSin(90)*2.0)),
            "RenderCloud resets negative or 360-degree source pulse phase to zero");
    }
    const int inverseSizes[]={0,2,3,12,13,14,100};
    for(int size=0;size<=100;++size) {
        bool inverse=false;for(int value:inverseSizes)inverse|=size==value;
        check(CastingSizeBlend(static_cast<float>(size))==(inverse?D3DBLEND_INVSRCALPHA:D3DBLEND_ONE),
            "casting destination blend matches original RenderTeiRect case table");
    }

    SprImg indexed;
    indexed.indices={0,1,2,1};
    CSprRes sprites;
    sprites.m_pal[1]=0x123456;sprites.m_pal[2]=0xABCDEF;
    sprites.m_sprites[0].push_back(&indexed);
    CActRes actions;actions.actions.resize(2);
    actions.actions[0].motions.resize(2);actions.actions[1].motions.resize(1);
    CSprClip clip{};clip.x=-4;clip.y=-3;clip.zoomX=2;clip.zoomY=3;
    clip.a=17;clip.r=34;clip.g=51;clip.b=68;
    actions.actions[0].motions[0].sprClips.push_back(clip);
    CEffectPrim prim;prim.m_segments[0].pos={0,0,100};prim.m_segments[0].size=1;
    prim.m_segments[0].argb=0x80706050;
    matrix view{};for(int i=0;i<4;++i)view.m[i][i]=1;
    auto render=[&](float slope=0.16f){g_renderer.submissions.clear();g_renderer.flags.clear();RenderParticleSprites(prim,sprites,actions,view,slope);};
    render();
    check(g_renderer.submissions.size()==1,"one clip yields one submission");
    check(g_texMgr.calls==1 && g_texMgr.lastWidth==2 && g_texMgr.lastHeight==2,"texture uses sprite dimensions, without motion canvas");
    check(g_texMgr.pixels==std::vector<u32>({0,0xFF123456,0xFFABCDEF,0xFF123456}),"indexed palette preserves colors and transparent index zero");
    auto face=g_renderer.submissions.front();
    check(near(face.m_verts[0].x,312.5f) && near(face.m_verts[0].y,231.5f),"clip coordinates are upper-left offsets, scaled with zoom");
    check(near(face.m_verts[3].x,316.5f) && near(face.m_verts[3].y,237.5f),"clip bounds use logical sprite dimensions and zoom");
    check(face.m_verts[0].color==0x80706050 && face.m_verts[3].color==0x80706050,"ordinary clips use segment ARGB without clip modulation");
    check(near(face.m_verts[1].tu,0.5f) && near(face.m_verts[3].tv,0.5f),"padded texture UVs use update dimensions");
    check(face.srcAlphaMode==5 && face.destAlphaMode==2 && g_renderer.flags.front()==5,"flag exactly five uses additive source-alpha blending");
    check(near(face.m_verts[0].oow,1.0f/(100-9*0.16f)+0.00001f) && near(face.m_verts[2].oow,1.0f/(100-3*0.16f)+0.00001f),"top and bottom retain distinct slope-adjusted reciprocal depth");
    check(near(face.m_verts[0].z,float(1500.0/1490.0*(1.0/face.m_verts[0].oow-10)*face.m_verts[0].oow)),"device depth follows original GetZFromOow");
    prim.m_renderFlag=0x201;render();
    check(g_renderer.submissions.front().m_verts[0].color==0x80706050,"render flag 0x200 does not select clip color");
    check(g_renderer.submissions.front().destAlphaMode==6 && g_renderer.flags.front()==0x201,"0x201 uses inverse-source-alpha and retains flags");
    prim.m_pattern=0x200;render();
    check(g_renderer.submissions.front().m_verts[0].color==0x11223344,"pattern 0x200 selects clip ARGB exactly");
    prim.m_renderFlag=7;render();
    check(g_renderer.submissions.front().destAlphaMode==6,"flag seven is not additive despite containing the five bits");
    check(g_texMgr.calls==1,"sprite texture is reused without rebaking clips");
    g_texMgr.texture.m_w=8;g_texMgr.texture.m_h=8;
    g_texMgr.texture.m_surfaceUpdateWidth=4;g_texMgr.texture.m_surfaceUpdateHeight=4;
    render();
    check(g_renderer.submissions.front().m_verts[3].tu==0.5f && g_renderer.submissions.front().m_verts[3].tv==0.5f,"backend upscaling preserves normalized sprite UV coverage");
    g_texMgr.texture={};
    auto& active=actions.actions[0].motions[0].sprClips[0];active.flags=1;
    render();face=g_renderer.submissions.front();
    check(near(face.m_verts[0].x,316.5f) && near(face.m_verts[1].x,312.5f),"horizontal flip reverses geometry while UV order is retained");
    check(face.m_verts[0].tu==0 && face.m_verts[1].tu==0.5f,"flipped clip retains original UV orientation");
    active.flags=0;indexed.isHalfW=1;indexed.isHalfH=1;render();face=g_renderer.submissions.front();
    check(near(face.m_verts[3].x,320.5f) && near(face.m_verts[3].y,243.5f),"half-resolution image flags expand logical width and height");
    indexed.isHalfW=0;indexed.isHalfH=0;active.angle=90;prim.m_roll=0.9f;render();face=g_renderer.submissions.front();
    check(near(face.m_verts[0].x,317.5f) && near(face.m_verts[0].y,232.5f),"clip plus primitive roll truncates before degree-table lookup and rotates around clip center");
    check(near(face.m_verts[3].x,311.5f) && near(face.m_verts[3].y,236.5f),"rotated opposite corner retains half-pixel offset");
    check(face.m_verts[0].oow!=face.m_verts[2].oow,"rotation preserves original unrotated row depths");
    active.angle=0;prim.m_roll=0;prim.m_numSegments=2;
    prim.m_segments[1]=prim.m_segments[0];prim.m_segments[1].pos.x=10;prim.m_segments[1].size=2;prim.m_segments[1].argb=0x10203040;
    prim.m_pattern=0;render();
    check(g_renderer.submissions.size()==2,"every segment submits its motion independently");
    check(g_renderer.submissions[1].m_verts[0].color==0x10203040 && near(g_renderer.submissions[1].m_verts[0].x,374.5f),"history segment retains its own position, size and color");
    prim.m_numSegments=1;prim.m_curMotion=1;render();
    check(g_renderer.submissions.empty(),"explicit empty motion does not reuse another frame");
    prim.m_curMotion=0;prim.m_spriteAction=1;render();
    check(g_renderer.submissions.empty(),"explicit empty action does not fall back to action zero");
    prim.m_spriteAction=0;actions.actions[0].motions[0].sprClips[0].sprIndex=99;render();
    check(g_renderer.submissions.empty(),"missing clip sprite is skipped without billboard substitution");
    actions.actions[0].motions[0].sprClips[0].sprIndex=0;
    prim.m_spriteAction=0;prim.m_segments[0].pos.z=50;render();
    check(g_renderer.submissions.size()==1,"particle path has no invented eighty-unit near-plane cutoff");
    prim.m_segments[0].pos.z=100;active.y=-242;render();
    check(g_renderer.submissions.empty(),"clip whose bottom is above screen is culled");
    active.y=240;render();
    check(g_renderer.submissions.empty(),"clip whose top is below screen is culled");
    active.y=-3;active.x=-1000;render();
    check(g_renderer.submissions.empty(),"normal offscreen horizontal clip is culled");
    active.flags=1;render();
    check(g_renderer.submissions.empty(),"flipped offscreen horizontal clip uses its reference culling branch");
    active.flags=0;active.x=-4;active.zoomX=-2;render();
    check(g_renderer.submissions.size()==1 && g_renderer.submissions.front().m_verts[0].x>g_renderer.submissions.front().m_verts[1].x,"negative zoom is retained without absolute-value normalization");
    active.zoomX=2;actions.actions[0].motions[0].sprClips.push_back(active);render();
    check(g_renderer.submissions.size()==2,"overlapping clips remain separate submissions in motion order");
    SprImg rgba;rgba.rgba={0x01234567,0x89ABCDEF,0x76543210,0};
    check(ResolveParticleTexture(rgba,sprites.m_pal)==&g_texMgr.texture && g_texMgr.pixels==rgba.rgba,"RGBA image channels and alpha reach texture upload unchanged");
    SprImg failed;failed.rgba=rgba.rgba;g_texMgr.fail=true;
    check(ResolveParticleTexture(failed,sprites.m_pal)==nullptr && failed.tex==nullptr,"failed upload leaves no dummy texture cached");
    g_texMgr.fail=false;
    {
        CTexture secondTexture;
        CEffectPrim cylinder;cylinder.m_arcAngle=90;cylinder.m_innerSize=2;cylinder.m_outerSize=4;cylinder.m_heightSize=10;
        cylinder.m_pos={10,20,100};cylinder.m_texture={&g_texMgr.texture,&secondTexture};cylinder.m_curMotion=1;
        cylinder.m_argb=0x12345678;cylinder.m_renderFlag=0x205;
        for(int i=0;i<4;++i)cylinder.m_matrix.m[i][i]=1;
        cylinder.m_matrix.m[3][0]=3;cylinder.m_matrix.m[3][1]=4;cylinder.m_matrix.m[3][2]=5;
        auto renderCylinder=[&](){g_renderer.m_rpAlphaList.clear();g_renderer.m_rpAlphaOPList.clear();g_renderer.m_rpEmissiveList.clear();
            g_renderer.m_rpAlphaNoDepthList.clear();RenderReferenceCylinder(cylinder,view);};
        auto matches=[&](const tlvertex3d& vertex,vector3d point){tlvertex3d expected;ProjectReferenceEffectVertex(point,view,expected);
            return near(vertex.x,expected.x)&&near(vertex.y,expected.y)&&near(vertex.oow,expected.oow)&&near(vertex.z,expected.z);};
        renderCylinder();check(g_renderer.m_rpAlphaList.size()==8,"cylinder submits two independent triangles for each of four wedges");
        auto* first=g_renderer.m_rpAlphaList[0].second;auto* second=g_renderer.m_rpAlphaList[1].second;
        check(matches(first->m_verts[0],{15,24,105}) && matches(first->m_verts[1],{17,14,105}) && matches(first->m_verts[2],{13,24,107}),
            "cylinder first triangle preserves lower-start upper-start lower-end order and negative local height");
        check(matches(second->m_verts[0],{13,24,107}) && matches(second->m_verts[1],{17,14,105}) && matches(second->m_verts[2],{13,14,109}),
            "cylinder second triangle preserves lower-end upper-start upper-end order");
        check(first->m_verts[0].tu==0 && first->m_verts[0].tv==1 && first->m_verts[1].tu==0 && first->m_verts[1].tv==0
            && first->m_verts[2].tu==0.25f && first->m_verts[2].tv==1,"cylinder first triangle preserves source quarter-step UVs");
        check(second->m_verts[0].tu==0.25f && second->m_verts[0].tv==1 && second->m_verts[1].tu==0 && second->m_verts[1].tv==0
            && second->m_verts[2].tu==0.25f && second->m_verts[2].tv==0,"cylinder second triangle preserves source quarter-step UVs");
        check(first->tex==&secondTexture && second->tex==&secondTexture,"cylinder selects current texture motion instead of texture zero");
        check(first->m_verts[0].color==0x12345678 && first->m_verts[1].color==0x12345678 && second->m_verts[2].color==0x12345678,
            "cylinder uses stored primitive packed color without reconstructing alpha");
        check(first->numVerts==3 && first->primType==D3DPT_TRIANGLELIST && first->m_verts[0].specular==0xFF000000
            && first->srcAlphaMode==5 && first->destAlphaMode==6,"cylinder retains independent triangle projection and ordinary alpha blend");
        check(g_renderer.m_rpAlphaOPList.empty() && g_renderer.m_rpEmissiveList.empty() && g_renderer.m_rpAlphaNoDepthList.empty(),
            "cylinder hard-coded flag one ignores primitive renderFlag bits");
        check(near(g_renderer.m_rpAlphaList[0].first,first->m_verts[0].oow),"cylinder queue uses original first-vertex sort depth");
        cylinder.m_matrix.m[0][0]=0;cylinder.m_matrix.m[0][2]=1;cylinder.m_matrix.m[2][0]=-1;cylinder.m_matrix.m[2][2]=0;
        renderCylinder();check(matches(g_renderer.m_rpAlphaList[0].second->m_verts[0],{13,24,107})
            && matches(g_renderer.m_rpAlphaList[0].second->m_verts[1],{13,14,109})
            && matches(g_renderer.m_rpAlphaList[0].second->m_verts[2],{11,24,105}),"cylinder uses full current primitive matrix before adding position");
        cylinder.m_matrix.m[0][0]=1;cylinder.m_matrix.m[0][2]=0;cylinder.m_matrix.m[2][0]=0;cylinder.m_matrix.m[2][2]=1;
        cylinder.m_arcAngle=36;renderCylinder();check(g_renderer.m_rpAlphaList.size()==20 && g_renderer.m_rpAlphaList[8].second->m_verts[0].tu==0,
            "Portal cylinder thirty-six-degree wedges wrap UV phase every fourth wedge");
        cylinder.m_arcAngle=72.5f;renderCylinder();check(g_renderer.m_rpAlphaList.size()==8,"cylinder retains floating loop progression for fractional arc");
        const int end=static_cast<int>(3*72.5f+72.5f);
        check(matches(g_renderer.m_rpAlphaList.back().second->m_verts[0],{13+2*g_cosTable[end],24,105+2*g_sinTable[end]}),
            "cylinder fractional loop truncates table index without truncating accumulated angle");
        cylinder.m_argb=0;cylinder.m_arcAngle=90;renderCylinder();check(g_renderer.m_rpAlphaList.size()==8 && g_renderer.m_rpAlphaList[0].second->m_verts[0].color==0,
            "cylinder retains source zero-alpha triangle submissions");
        cylinder.m_curMotion=2;renderCylinder();check(g_renderer.m_rpAlphaList.empty(),"invalid cylinder texture motion emits no substitute");
        cylinder.m_curMotion=0;cylinder.m_texture[0]=&CTexMgr::s_dummy_texture;renderCylinder();check(g_renderer.m_rpAlphaList.empty(),"missing cylinder texture emits no substitute");
        cylinder.m_texture[0]=&g_texMgr.texture;cylinder.m_arcAngle=0;renderCylinder();check(g_renderer.m_rpAlphaList.empty(),"invalid cylinder arc emits no replacement geometry");
        cylinder.m_arcAngle=360;g_cosTable[360]=0.75f;renderCylinder();
        check(g_renderer.m_rpAlphaList.size()==2 && matches(g_renderer.m_rpAlphaList[0].second->m_verts[2],{14.5f,24,105+2*g_sinTable[360]}),
            "cylinder endpoint directly reads table index three-sixty");
        g_cosTable[360]=GetCos(360);
    }
    {
        CEffectPrim sphere;sphere.m_pos={10,20,100};sphere.m_radius=5;sphere.m_arcAngle=90;sphere.m_longitude=10.75f;
        sphere.m_texture={&g_texMgr.texture};sphere.m_argb=0x10FFFFFF;sphere.m_renderFlag=0x205;
        auto renderSphere=[&](){g_renderer.m_rpAlphaList.clear();g_renderer.m_rpAlphaOPList.clear();g_renderer.m_rpAlphaNoDepthList.clear();RenderReferenceSphere(sphere,view);};
        auto point=[&](int pitch,int yaw){matrix m;MatrixIdentity(m);MatrixAppendXRotation(m,GetRadian(pitch));MatrixAppendYRotation(m,GetRadian(yaw));
            return vector3d{m.m[2][0]*5+10,m.m[2][1]*5+20,m.m[2][2]*5+100};};
        auto matches=[&](const tlvertex3d& vert,vector3d p){tlvertex3d expected;ProjectReferenceEffectVertex(p,view,expected);
            return near(vert.x,expected.x)&&near(vert.y,expected.y)&&near(vert.oow,expected.oow);};
        renderSphere();auto* a=g_renderer.m_rpAlphaList[0].second;auto* b=g_renderer.m_rpAlphaList[1].second;
        check(g_renderer.m_rpAlphaList.size()==16,"MagnumBreak sphere emits two triangles across two latitude bands and four yaw wedges");
        check(matches(a->m_verts[0],point(-90,10))&&matches(a->m_verts[1],point(0,10))&&matches(a->m_verts[2],point(-90,101)),
            "MagnumBreak sphere first triangle retains latitude order longitude truncation and quarter-degree seam bias");
        check(matches(b->m_verts[0],point(0,101))&&matches(b->m_verts[1],point(-90,101))&&matches(b->m_verts[2],point(0,10)),
            "MagnumBreak sphere second triangle preserves source opposite-corner winding");
        check(a->m_verts[0].tu==0&&a->m_verts[0].tv==0&&a->m_verts[1].tu==0&&a->m_verts[1].tv==0.5f
            &&a->m_verts[2].tu==0.25f&&b->m_verts[0].tu==0.25f&&b->m_verts[0].tv==0.5f
            &&g_renderer.m_rpAlphaList[8].second->m_verts[0].tu==0&&g_renderer.m_rpAlphaList[8].second->m_verts[0].tv==0.5f,
            "MagnumBreak sphere maps full source UV grid and resets U for each latitude band");
        check(a->tex==&g_texMgr.texture&&a->m_verts[0].color==0x10FFFFFF&&g_renderer.m_rpAlphaNoDepthList.empty(),
            "MagnumBreak sphere uses texture zero stored packed color and hard-coded alpha flag one");
        check(a->primType==D3DPT_TRIANGLELIST&&a->numVerts==3&&a->cullMode==D3DCULL_NONE
            &&a->srcAlphaMode==D3DBLEND_SRCALPHA&&a->destAlphaMode==D3DBLEND_INVSRCALPHA
            &&g_renderer.m_rpAlphaOPList.empty()&&g_renderer.m_rpAlphaNoDepthList.empty(),
            "sphere triangles preserve cull-none source-alpha/inverse-alpha material in depth-tested alpha queue");
        sphere.m_arcAngle=36;renderSphere();check(g_renderer.m_rpAlphaList.size()==100,"MagnumBreak constructor arc yields original hundred-triangle sphere mesh");
        sphere.m_arcAngle=72.5f;renderSphere();check(g_renderer.m_rpAlphaList.size()==16,"MagnumBreak sphere preserves fractional arc loop bounds across both axes");
        sphere.m_texture.clear();renderSphere();check(g_renderer.m_rpAlphaList.empty(),"MagnumBreak missing sphere texture emits no billboard or glow substitute");
    }
    {
        CEffectPrim ring;ring.m_pos={100,200,900};ring.m_radius=20;ring.m_size=5;ring.m_arcAngle=90;ring.m_argb=0xF7FFFFFF;
        ring.m_texture={&g_texMgr.texture};ring.m_renderFlag=0x205;
        auto renderRing=[&](){g_renderer.m_rpAlphaList.clear();g_renderer.m_rpAlphaOPList.clear();g_renderer.m_rpAlphaNoDepthList.clear();RenderReferenceScreenCircle(ring);};
        renderRing();auto* a=g_renderer.m_rpAlphaList[0].second;auto* b=g_renderer.m_rpAlphaList[1].second;
        check(g_renderer.m_rpAlphaList.size()==8,"OverThrust screen circle submits two triangles per source wedge");
        check(near(a->m_verts[0].x,115)&&near(a->m_verts[0].y,200)&&near(a->m_verts[1].x,100)&&near(a->m_verts[1].y,180)
            &&near(a->m_verts[2].x,120)&&near(a->m_verts[2].y,200),"OverThrust first wedge preserves inner-start outer-end outer-start order without half-pixel shift");
        check(near(b->m_verts[0].x,115)&&near(b->m_verts[0].y,200)&&near(b->m_verts[1].x,100)&&near(b->m_verts[1].y,185)
            &&near(b->m_verts[2].x,100)&&near(b->m_verts[2].y,180),"OverThrust second wedge preserves inner-start inner-end outer-end order");
        check(a->m_verts[0].tu==0&&a->m_verts[0].tv==1&&a->m_verts[1].tu==0.25f&&a->m_verts[1].tv==0
            &&a->m_verts[2].tu==0&&a->m_verts[2].tv==0&&b->m_verts[1].tu==0.25f&&b->m_verts[1].tv==1,
            "OverThrust circle preserves original quarter-phase wedge UVs");
        check(a->m_verts[0].z==0.000001f&&a->m_verts[0].oow==0.99999899f&&a->m_verts[0].color==0xF7FFFFFF
            &&a->m_verts[0].specular==0xFF000000&&a->tex==&g_texMgr.texture&&g_renderer.m_rpAlphaNoDepthList.empty(),
            "OverThrust circle preserves fixed source depth packed alpha texture zero and hard-coded flag one");
        ring.m_arcAngle=10;renderRing();check(g_renderer.m_rpAlphaList.size()==72&&g_renderer.m_rpAlphaList[8].second->m_verts[0].tu==0,
            "OverThrust ten-degree circle uses seventy-two triangles and wraps UV every four wedges");
        ring.m_arcAngle=72.5f;renderRing();check(g_renderer.m_rpAlphaList.size()==8
            &&g_renderer.m_rpAlphaList.back().second->m_verts[2].x==static_cast<float>(static_cast<double>(g_cosTable[290])*20+100),
            "screen circle retains fractional arc accumulation before integer endpoint lookup");
        ring.m_arcAngle=360;const float savedEnd=g_cosTable[360];g_cosTable[360]=0.75f;renderRing();
        check(g_renderer.m_rpAlphaList.size()==2&&g_renderer.m_rpAlphaList[0].second->m_verts[1].x==115,
            "screen circle endpoint directly reads original table index three-sixty");g_cosTable[360]=savedEnd;
        const float savedStart=g_cosTable[0];g_cosTable[0]=0.12345679f;ring.m_arcAngle=90;ring.m_radius=123.456f;ring.m_size=0;ring.m_pos.x=-15.24f;renderRing();
        const float firstX=g_renderer.m_rpAlphaList[0].second->m_verts[0].x,secondX=g_renderer.m_rpAlphaList[1].second->m_verts[0].x;
        check(firstX==static_cast<float>(static_cast<double>(g_cosTable[0])*ring.m_radius+ring.m_pos.x)
            &&secondX==static_cast<float>(g_cosTable[0]*ring.m_radius)+ring.m_pos.x&&firstX!=secondX,
            "screen circle preserves source first-inner double product versus second-inner stored float temporary");g_cosTable[0]=savedStart;
        ring.m_texture.clear();renderRing();check(g_renderer.m_rpAlphaList.empty(),"OverThrust missing circle texture emits no replacement geometry");
    }
    {
        CEffectPrim flash;flash.m_pos={100,200,900};flash.m_longitude=90.9f;flash.m_arcAngle=10.2f;flash.m_heightSize=30;
        flash.m_texture={&g_texMgr.texture};flash.m_argb=0xFEFFFFFF;flash.m_renderFlag=0x205;
        auto renderFlash=[&](){g_renderer.m_rpAlphaList.clear();g_renderer.m_rpAlphaOPList.clear();g_renderer.m_rpAlphaNoDepthList.clear();RenderReferenceFlash(flash);};
        renderFlash();auto* face=g_renderer.m_rpAlphaList[0].second;
        check(g_renderer.m_rpAlphaList.size()==1&&face->m_verts[0].x==100&&face->m_verts[0].y==200,
            "HasteUp flash emits one triangle at exact screen anchor without half-pixel shift");
        check(face->m_verts[1].x==static_cast<float>(static_cast<double>(GetCos(80))*30+100)
            &&face->m_verts[1].y==static_cast<float>(200-static_cast<double>(GetSin(80))*30)
            &&face->m_verts[2].x==static_cast<float>(static_cast<double>(GetCos(101))*30+100)
            &&face->m_verts[2].y==static_cast<float>(200-static_cast<double>(GetSin(101))*30),
            "HasteUp flash truncates longitude minus and plus arc separately before trig lookup");
        check(face->m_verts[0].tu==0.5f&&face->m_verts[0].tv==0&&face->m_verts[1].tu==1&&face->m_verts[1].tv==1
            &&face->m_verts[2].tu==0&&face->m_verts[2].tv==1,"HasteUp flash preserves source triangular UV order");
        check(face->m_verts[0].z==0.000001f&&face->m_verts[0].oow==0.99999899f&&face->m_verts[0].color==0xFEFFFFFF
            &&face->m_verts[0].specular==0xFF000000&&face->tex==&g_texMgr.texture&&g_renderer.m_rpAlphaNoDepthList.empty(),
            "HasteUp flash keeps fixed source depth color constructor specular texture zero and hard-coded flag one");
        flash.m_heightSize=0;renderFlash();check(g_renderer.m_rpAlphaList.size()==1&&g_renderer.m_rpAlphaList[0].second->m_verts[2].x==100,
            "HasteUp initial zero-length flash retains source degenerate submission");
        flash.m_texture.clear();renderFlash();check(g_renderer.m_rpAlphaList.empty(),"HasteUp missing flash texture emits no replacement geometry");
    }
    {
        CEffectPrim line;line.m_pos={12,24,120};
        matrix lineView{};for(int i=0;i<4;++i)lineView.m[i][i]=1;
        tlvertex3d topExpected{},lowerExpected{};
        ProjectReferenceEffectVertex(line.m_pos,lineView,topExpected);
        vector3d lowerPosition=line.m_pos;lowerPosition.y-=10.0f;
        ProjectReferenceEffectVertex(lowerPosition,lineView,lowerExpected);
        g_renderer.m_rpAlphaOPList.clear();g_renderer.m_rpAlphaList.clear();g_renderer.m_rpAlphaNoDepthList.clear();
        RenderReference3DLine(line,lineView);
        check(g_renderer.m_rpAlphaOPList.size()==1&&g_renderer.m_rpAlphaList.empty(),
            "3D line submits exactly one source alpha-operation face with flag five");
        auto* face=g_renderer.m_rpAlphaOPList[0];
        check(face->numVerts==3&&face->tex==nullptr
            &&face->m_verts[0].x==topExpected.x&&face->m_verts[0].y==topExpected.y&&face->m_verts[0].z==topExpected.z
            &&face->m_verts[1].x==static_cast<float>(static_cast<double>(topExpected.x)+1.0)
            &&face->m_verts[1].y==topExpected.y&&face->m_verts[1].z==topExpected.z
            &&face->m_verts[2].x==lowerExpected.x&&face->m_verts[2].y==lowerExpected.y&&face->m_verts[2].z==lowerExpected.z,
            "3D line uses top/top-plus-one/lower projected vertex order and source ten-unit world offset");
        check(face->m_verts[0].color==0x9DFFFFFFu&&face->m_verts[1].color==0x9BFFFFFFu
            &&face->m_verts[2].color==0x9BFFFFFFu&&face->m_verts[0].oow==0.99999899f
            &&face->m_verts[1].oow==0.99999899f&&face->m_verts[2].oow==0.99999899f
            &&face->m_verts[0].tu==0&&face->m_verts[1].tu==0&&face->m_verts[2].tu==0
            &&face->m_verts[0].tv==0&&face->m_verts[1].tv==0&&face->m_verts[2].tv==0,
            "3D line preserves fixed source colors, reciprocal-depth values and zero UVs");
        check(face->m_verts[0].specular==0xFF000000u&&face->m_verts[1].specular==0xFF000000u
            &&face->m_verts[2].specular==0xFF000000u&&face->srcAlphaMode==D3DBLEND_SRCALPHA
            &&face->destAlphaMode==D3DBLEND_INVSRCALPHA&&face->cullMode==D3DCULL_NONE
            &&face->primType==D3DPT_TRIANGLELIST&&face->numIndices==0&&face->indices==nullptr,
            "3D line retains opaque specular, inverse-alpha blend and source triangle state");
    }
    {
        CEffectPrim billboard;billboard.m_pos={2,3,20};billboard.m_widthSize=5;billboard.m_heightSize=3;
        billboard.m_curMotion=1;billboard.m_argb=0xA1B2C3D4;billboard.m_renderFlag=5;
        CTexture firstTexture,secondTexture;billboard.m_texture={&firstTexture,&secondTexture};
        matrix billboardView{};for(int i=0;i<4;++i)billboardView.m[i][i]=1;
        billboardView.m[0][0]=1.25f;billboardView.m[1][0]=0.125f;billboardView.m[3][0]=4;
        billboardView.m[0][1]=-0.25f;billboardView.m[1][1]=0.75f;billboardView.m[3][1]=-2;
        billboardView.m[3][2]=100;
        const double reciprocal=1.0/(static_cast<double>(billboardView.m[2][2])*billboard.m_pos.z
            +static_cast<double>(billboardView.m[1][2])*billboard.m_pos.y
            +static_cast<double>(billboardView.m[0][2])*billboard.m_pos.x+billboardView.m[3][2]);
        const float oow=static_cast<float>(reciprocal);
        const float x=static_cast<float>((static_cast<double>(billboardView.m[2][0])*billboard.m_pos.z
            +static_cast<double>(billboardView.m[1][0])*billboard.m_pos.y
            +static_cast<double>(billboard.m_pos.x)*billboardView.m[0][0]+billboardView.m[3][0])
            *reciprocal*g_renderer.m_hpc+g_renderer.m_xoffset);
        const float y=static_cast<float>((static_cast<double>(billboardView.m[2][1])*billboard.m_pos.z
            +static_cast<double>(billboardView.m[1][1])*billboard.m_pos.y
            +static_cast<double>(billboard.m_pos.x)*billboardView.m[0][1]+billboardView.m[3][1])
            *g_renderer.m_vpc*oow+g_renderer.m_yoffset);
        const double ratio=static_cast<double>(oow)*g_renderer.m_hpc*0.14285715;
        const float right=static_cast<float>(static_cast<double>(static_cast<float>(ratio*billboard.m_widthSize))+x);
        const float bottom=static_cast<float>(static_cast<double>(y)-static_cast<float>(ratio*billboard.m_heightSize));
        g_renderer.submissions.clear();g_renderer.flags.clear();
        RenderReference3DTextureBillboard(billboard,billboardView);
        check(g_renderer.submissions.size()==2&&g_renderer.flags==std::vector<int>({5,5}),
            "3D texture billboard submits two ordered faces through legacy AddRP with raw render flag five");
        auto& a=g_renderer.submissions[0];auto& b=g_renderer.submissions[1];
        check(a.tex==&secondTexture&&b.tex==&secondTexture&&a.m_verts[0].x==x&&a.m_verts[0].y==y
            &&a.m_verts[1].x==right&&a.m_verts[1].y==y&&a.m_verts[2].x==x&&a.m_verts[2].y==bottom
            &&b.m_verts[0].x==right&&b.m_verts[0].y==y&&b.m_verts[1].x==x&&b.m_verts[1].y==bottom
            &&b.m_verts[2].x==right&&b.m_verts[2].y==bottom,
            "3D texture billboard projects source anchor and preserves the two-triangle top-left quad order");
        const float expectedU[2][3]={{0,1,0},{1,0,1}},expectedV[2][3]={{0,0,1},{0,1,1}};
        bool billboardFieldsMatch=true;
        for(int faceIndex=0;faceIndex<2;++faceIndex) {
            const RPFace& face=g_renderer.submissions[static_cast<size_t>(faceIndex)];
            billboardFieldsMatch=billboardFieldsMatch&&face.primType==D3DPT_TRIANGLELIST&&face.numVerts==3
                &&face.numIndices==0&&face.tex==&secondTexture&&face.mtPreset==0&&face.cullMode==D3DCULL_NONE
                &&face.srcAlphaMode==D3DBLEND_SRCALPHA&&face.destAlphaMode==D3DBLEND_INVSRCALPHA;
            for(int vertex=0;vertex<3;++vertex) {
                const tlvertex3d& value=face.m_verts[vertex];
                billboardFieldsMatch=billboardFieldsMatch&&value.z==0.80000001f&&value.oow==1
                    &&value.color==0xA1B2C3D4&&value.specular==0xFF000000u
                    &&value.tu==expectedU[faceIndex][vertex]&&value.tv==expectedV[faceIndex][vertex];
            }
        }
        check(billboardFieldsMatch,
            "3D texture billboard retains fixed depth, reciprocal depth, ARGB/specular, complete UVs, texture, cull and alpha blend state");
    }
    {
        CEffectPrim ice;ice.m_pos={10,20,100};ice.m_size=2;ice.m_heightSize=15;ice.m_argb=0xC8FFFFFF;
        ice.m_texture={&g_texMgr.texture};ice.m_renderFlag=0x205;
        for(int i=0;i<4;++i)ice.m_rotMat.m[i][i]=1;
        auto renderIce=[&](){g_renderer.m_rpAlphaList.clear();g_renderer.m_rpAlphaOPList.clear();g_renderer.m_rpAlphaNoDepthList.clear();RenderReferenceQuadHorn(ice,view);};
        auto matches=[&](const tlvertex3d& v,vector3d point){tlvertex3d expected;ProjectReferenceEffectVertex(point,view,expected);
            return near(v.x,expected.x)&&near(v.y,expected.y)&&near(v.oow,expected.oow);};
        renderIce();check(g_renderer.m_rpAlphaList.size()==4,"FrostDiver quad horn submits original four triangular faces");
        auto* a=g_renderer.m_rpAlphaList[0].second;auto* b=g_renderer.m_rpAlphaList[1].second;
        auto* c=g_renderer.m_rpAlphaList[2].second;auto* d=g_renderer.m_rpAlphaList[3].second;
        check(matches(a->m_verts[0],{8,18,100})&&matches(a->m_verts[1],{10,20,115})&&matches(a->m_verts[2],{12,18,100}),
            "FrostDiver first horn face preserves source square base and positive local Z apex");
        check(matches(b->m_verts[0],{8,18,100})&&matches(b->m_verts[2],{8,22,100})
            &&matches(c->m_verts[0],{8,22,100})&&matches(c->m_verts[2],{12,22,100})
            &&matches(d->m_verts[0],{12,22,100})&&matches(d->m_verts[2],{12,18,100}),"FrostDiver remaining faces preserve source winding and corner order");
        check(a->m_verts[0].tu==0&&a->m_verts[0].tv==1&&a->m_verts[1].tu==0&&a->m_verts[1].tv==0
            &&a->m_verts[2].tu==0.2f&&b->m_verts[0].tu==0.2f&&near(d->m_verts[2].tu,0.8f),"FrostDiver horn advances original zero-point-two UV bands per face");
        check(a->tex==&g_texMgr.texture&&a->m_verts[0].color==0xC8FFFFFF&&a->srcAlphaMode==5&&a->destAlphaMode==6
            &&g_renderer.m_rpAlphaNoDepthList.empty(),"FrostDiver horn retains stored color exact texture and hard-coded flag one");
        ice.m_matrix.m[0][0]=100;ice.m_rotMat.m[3][0]=3;ice.m_rotMat.m[3][1]=4;ice.m_rotMat.m[3][2]=5;renderIce();
        check(matches(g_renderer.m_rpAlphaList[0].second->m_verts[0],{11,22,105}),"FrostDiver geometry uses retained render matrix translation independently of movement matrix");
        ice.m_rotMat.m[0][0]=0;ice.m_rotMat.m[0][2]=1;ice.m_rotMat.m[2][0]=-1;ice.m_rotMat.m[2][2]=0;renderIce();
        check(matches(g_renderer.m_rpAlphaList[0].second->m_verts[1],{-2,24,105}),"FrostDiver apex follows retained rotation before adding primitive world origin");
        ice.m_rotMat={};ice.m_rotMat.m[0][0]=1.25f;ice.m_rotMat.m[1][0]=2.5f;ice.m_rotMat.m[2][0]=3.75f;ice.m_rotMat.m[3][0]=4;
        ice.m_rotMat.m[0][1]=5.25f;ice.m_rotMat.m[1][1]=6.5f;ice.m_rotMat.m[2][1]=7.75f;ice.m_rotMat.m[3][1]=8;
        ice.m_rotMat.m[0][2]=9.25f;ice.m_rotMat.m[1][2]=10.5f;ice.m_rotMat.m[2][2]=11.75f;ice.m_rotMat.m[3][2]=12;renderIce();
        check(matches(g_renderer.m_rpAlphaList[0].second->m_verts[2],{11.5f,25.5f,109.5f}),
            "FrostDiver unusual third-corner transform uses each matrix basis term and translation");
        ice.m_argb=0;renderIce();check(g_renderer.m_rpAlphaList.size()==4&&g_renderer.m_rpAlphaList[0].second->m_verts[0].color==0,
            "FrostDiver retains source zero-alpha face submissions");
        ice.m_texture.clear();renderIce();check(g_renderer.m_rpAlphaList.empty(),"FrostDiver missing texture emits no replacement geometry");
    }
    {
        CEffectPrim cross;cross.m_widthSize=3;cross.m_heightSize=0.2f;cross.m_pos={10,20,100};cross.m_argb=0x09F00000;
        cross.m_texture={&g_texMgr.texture};cross.m_renderFlag=0x205;
        for(int i=0;i<4;++i)cross.m_matrix.m[i][i]=1;
        auto renderCross=[&](){g_renderer.m_rpAlphaList.clear();g_renderer.m_rpAlphaOPList.clear();g_renderer.m_rpAlphaNoDepthList.clear();
            RenderReferenceCrossTexture(cross,view);};
        auto matches=[&](const tlvertex3d& v,vector3d point){tlvertex3d expected;ProjectReferenceEffectVertex(point,view,expected);
            return near(v.x,expected.x)&&near(v.y,expected.y)&&near(v.oow,expected.oow);};
        renderCross();check(g_renderer.m_rpAlphaList.size()==4,"cross texture emits four triangles across two perpendicular strips");
        auto* first=g_renderer.m_rpAlphaList[0].second;auto* second=g_renderer.m_rpAlphaList[1].second;auto* third=g_renderer.m_rpAlphaList[2].second;
        check(matches(first->m_verts[0],{9.8f,20,103})&&matches(first->m_verts[1],{9.8f,20,97})&&matches(first->m_verts[2],{10.2f,20,103}),
            "cross first strip preserves local height versus width dimensions and vertex order");
        check(matches(second->m_verts[0],{10.2f,20,97})&&matches(second->m_verts[1],{10.2f,20,103})&&matches(second->m_verts[2],{9.8f,20,97}),
            "cross second triangle preserves source reversed strip order");
        check(matches(third->m_verts[0],{10,19.8f,103})&&matches(third->m_verts[1],{10,19.8f,97})&&matches(third->m_verts[2],{10,20.2f,103}),
            "cross second strip lies in perpendicular local plane");
        check(first->m_verts[0].tu==0&&first->m_verts[0].tv==1&&first->m_verts[1].tu==1&&first->m_verts[1].tv==1
            &&first->m_verts[2].tu==0&&first->m_verts[2].tv==0&&second->m_verts[0].tu==1&&second->m_verts[0].tv==0,
            "cross texture preserves original triangle UV order");
        check(first->m_verts[0].color==0x09F00000&&third->m_verts[2].color==0x09F00000&&first->srcAlphaMode==5&&first->destAlphaMode==6
            &&g_renderer.m_rpAlphaOPList.empty()&&g_renderer.m_rpAlphaNoDepthList.empty(),"cross texture uses stored color and hard-coded alpha flag one");
        cross.m_matrix.m[1][1]=0;cross.m_matrix.m[1][2]=1;cross.m_matrix.m[2][1]=-1;cross.m_matrix.m[2][2]=0;renderCross();
        check(matches(g_renderer.m_rpAlphaList[0].second->m_verts[0],{9.8f,17,100})
            &&matches(g_renderer.m_rpAlphaList[2].second->m_verts[0],{10,17,99.8f}),"cross texture uses launch matrix instead of camera billboard orientation");
        cross.m_argb=0;renderCross();check(g_renderer.m_rpAlphaList.size()==4,"cross texture retains zero-alpha source submissions");
        cross.m_texture[0]=&CTexMgr::s_dummy_texture;renderCross();check(g_renderer.m_rpAlphaList.empty(),"cross missing texture emits no glow substitute");
        cross.m_texture[0]=&g_texMgr.texture;cross.m_curMotion=1;renderCross();check(g_renderer.m_rpAlphaList.empty(),"cross invalid current texture motion emits no substitute");
    }
    {
        CEffectPrim panel;panel.m_pos={10,20,100};panel.m_widthSize=3;panel.m_heightSize=2;
        panel.m_argb=0x80ABCDEF;panel.m_texture={&g_texMgr.texture};panel.m_v1=0.2f;panel.m_v2=0.8f;panel.m_renderFlag=1;
        MatrixIdentity(panel.m_rotMat);
        auto renderPanel=[&](){g_renderer.m_rpAlphaList.clear();g_renderer.m_rpAlphaOPList.clear();g_renderer.m_rpAlphaNoDepthList.clear();
            g_renderer.m_rpAlphaDepthList.clear();g_renderer.m_rpEmissiveList.clear();g_renderer.m_rpEmissiveDepthList.clear();
            g_renderer.m_rpEmissiveNoDepthList.clear();g_renderer.m_rpFaceList.clear();
            RenderReferenceTexture(panel,view);};
        auto matches=[&](const tlvertex3d& v,vector3d point){tlvertex3d expected;ProjectReferenceEffectVertex(point,view,expected);
            return near(v.x,expected.x)&&near(v.y,expected.y)&&near(v.oow,expected.oow);};
        renderPanel();check(g_renderer.m_rpAlphaList.size()==2,"3D texture emits two triangles for one projected quad");
        auto* first=g_renderer.m_rpAlphaList[0].second;auto* second=g_renderer.m_rpAlphaList[1].second;
        check(matches(first->m_verts[0],{10,18,103})&&matches(first->m_verts[1],{10,18,97})&&matches(first->m_verts[2],{10,22,103})
            &&matches(second->m_verts[0],{10,22,97})&&matches(second->m_verts[1],{10,22,103})&&matches(second->m_verts[2],{10,18,97}),
            "3D texture ordinary pattern preserves source quad corners and both triangle orders");
        check(first->m_verts[0].tu==0&&first->m_verts[0].tv==0.2f&&first->m_verts[1].tu==1&&first->m_verts[1].tv==0.2f
            &&first->m_verts[2].tu==0&&first->m_verts[2].tv==0.8f&&second->m_verts[0].tu==1&&second->m_verts[0].tv==0.8f
            &&second->m_verts[1].tu==0&&second->m_verts[1].tv==0.8f&&second->m_verts[2].tu==1&&second->m_verts[2].tv==0.2f,
            "3D texture preserves full source UV orientation and v1/v2 values");
        check(first->tex==&g_texMgr.texture&&second->tex==&g_texMgr.texture&&first->m_verts[0].color==0x80ABCDEF
            &&first->srcAlphaMode==5&&first->destAlphaMode==6&&g_renderer.m_rpAlphaOPList.empty(),
            "3D texture selects current texture and stored color and forwards render flags");
        panel.m_pattern=0x80;renderPanel();first=g_renderer.m_rpAlphaList[0].second;second=g_renderer.m_rpAlphaList[1].second;
        check(matches(first->m_verts[0],{10,16,103})&&matches(first->m_verts[2],{10,20,103})
            &&matches(second->m_verts[0],{10,20,97}),"3D texture pattern 0x80 extends lower edge to minus twice height and pins upper edge");
        panel.m_pattern=0x8;renderPanel();first=g_renderer.m_rpAlphaList[0].second;
        check(matches(first->m_verts[0],{10,18,103})&&matches(first->m_verts[2],{10,20,103}),
            "3D texture pattern 0x8 collapses upper edge to the origin plane");
        panel.m_pattern=0x88;renderPanel();first=g_renderer.m_rpAlphaList[0].second;
        check(matches(first->m_verts[0],{10,16,103})&&matches(first->m_verts[2],{10,20,103}),
            "3D texture combined pattern bits preserve lower extension and upper collapse");
        panel.m_pattern=0;panel.m_rotMat.m[2][0]=1;panel.m_rotMat.m[2][2]=0;renderPanel();
        first=g_renderer.m_rpAlphaList[0].second;
        check(matches(first->m_verts[0],{13,18,100}),"3D texture applies retained render matrix before world translation and view projection");
        panel.m_rotMat.m[2][0]=0;panel.m_rotMat.m[2][2]=1;
        panel.m_renderFlag=3;renderPanel();first=g_renderer.m_rpEmissiveList[0].second;
        check(g_renderer.m_rpEmissiveList.size()==2&&first->srcAlphaMode==D3DBLEND_ONE&&first->destAlphaMode==D3DBLEND_ONE,
            "3D texture forwards source render flags to triangle blend and queue selection");
        panel.m_texture[0]=&CTexMgr::s_dummy_texture;renderPanel();check(g_renderer.m_rpAlphaList.empty(),"3D texture missing texture emits no substitute geometry");
    }
    {
        CTexture secondTexture;CEffectPrim sand;sand.m_pos={100,200,900};sand.m_widthSize=30;sand.m_heightSize=30;sand.m_argb=0x80FFFFFF;
        sand.m_texture={&g_texMgr.texture,&secondTexture};sand.m_curMotion=1;sand.m_renderFlag=0x205;
        auto renderSand=[&](){g_renderer.m_rpAlphaList.clear();g_renderer.m_rpAlphaOPList.clear();g_renderer.m_rpAlphaNoDepthList.clear();
            RenderReferenceScreenTexture(sand);};
        renderSand();check(g_renderer.m_rpAlphaList.size()==2,"SandWind submits exactly two screen triangles");
        auto* first=g_renderer.m_rpAlphaList[0].second;auto* second=g_renderer.m_rpAlphaList[1].second;
        check(first->m_verts[0].x==69.5f&&first->m_verts[0].y==229.5f&&first->m_verts[1].x==69.5f&&first->m_verts[1].y==169.5f
            &&first->m_verts[2].x==129.5f&&first->m_verts[2].y==229.5f,"SandWind screen geometry preserves dimensions order and half-pixel offset without perspective scaling");
        check(second->m_verts[0].x==129.5f&&second->m_verts[0].y==229.5f&&second->m_verts[1].x==69.5f&&second->m_verts[1].y==169.5f
            &&second->m_verts[2].x==129.5f&&second->m_verts[2].y==169.5f,"SandWind second triangle preserves original shared-edge order");
        check(first->m_verts[0].tu==0&&first->m_verts[0].tv==1&&first->m_verts[1].tv==0&&first->m_verts[2].tu==1&&first->m_verts[2].tv==1
            &&second->m_verts[2].tu==1&&second->m_verts[2].tv==0,"SandWind preserves source screen UV order");
        check(first->m_verts[0].z==0.000001f&&first->m_verts[0].oow==0.99999899f&&first->m_verts[0].color==0x80FFFFFF&&first->tex==&secondTexture,
            "SandWind preserves fixed depth reciprocal stored color and current texture");
        check(g_renderer.m_rpAlphaOPList.empty()&&g_renderer.m_rpAlphaNoDepthList.empty()&&first->srcAlphaMode==5&&first->destAlphaMode==6,
            "SandWind uses original hard-coded alpha flag one regardless of renderFlag");
        sand.m_argb=0;renderSand();check(g_renderer.m_rpAlphaList.size()==2&&g_renderer.m_rpAlphaList[0].second->m_verts[0].color==0,"SandWind retains source zero-alpha submissions");
        sand.m_texture[1]=&CTexMgr::s_dummy_texture;renderSand();check(g_renderer.m_rpAlphaList.empty(),"SandWind missing texture draws no glow substitute");
    }
    {
        CEffectPrim hit;hit.m_pos={100,200,900};hit.m_widthSize=10;hit.m_heightSize=20;hit.m_pattern=1;hit.m_argb=0x1F7878FF;
        hit.m_texture={&g_texMgr.texture};
        auto renderHit=[&](){g_renderer.m_rpAlphaList.clear();RenderReferenceScreenTexture(hit);};
        renderHit();auto* first=g_renderer.m_rpAlphaList[0].second;
        check(g_renderer.m_rpAlphaList.size()==2&&first->m_verts[0].x==89.5f&&first->m_verts[0].y==199.5f
            &&first->m_verts[1].x==89.5f&&first->m_verts[1].y==159.5f&&first->m_verts[2].x==109.5f&&first->m_verts[2].y==199.5f,
            "Hit2 bottom-pivot geometry extends twice source height above anchor");
        hit.m_roll=90.9f;renderHit();first=g_renderer.m_rpAlphaList[0].second;
        check(near(first->m_verts[0].x,99.5f)&&near(first->m_verts[0].y,189.5f)&&near(first->m_verts[1].x,139.5f)
            &&near(first->m_verts[1].y,189.5f)&&near(first->m_verts[2].y,209.5f),"Hit2 screen renderer truncates roll before source trig-table rotation");
        check(first->m_verts[0].color==0x1F7878FF&&first->m_verts[0].z==0.000001f&&first->m_verts[0].oow==0.99999899f,
            "Hit2 retains packed blue tint and original fixed screen depth");
        check(first->m_verts[0].tu==0&&first->m_verts[0].tv==1&&first->m_verts[1].tu==0&&first->m_verts[1].tv==0
            &&first->m_verts[2].tu==1&&first->m_verts[2].tv==1,"Hit2 preserves source UVs after rotated bottom pivot");
    }
    {
        CEffectPrim hit;hit.m_pos={100,200,900};hit.m_widthSize=10;hit.m_heightSize=20;hit.m_pattern=0;hit.m_roll=90;
        hit.m_argb=0xFEFFFFFF;hit.m_texture={&g_texMgr.texture};g_renderer.m_rpAlphaList.clear();RenderReferenceScreenTexture(hit);
        auto* face=g_renderer.m_rpAlphaList[0].second;
        check(near(face->m_verts[0].x,79.5f)&&near(face->m_verts[0].y,189.5f)&&near(face->m_verts[1].x,119.5f)
            &&near(face->m_verts[1].y,189.5f)&&near(face->m_verts[2].x,79.5f)&&near(face->m_verts[2].y,209.5f),
            "Hit5/6 centered rotated geometry retains source pivot rather than Hit2 bottom origin");
        check(face->m_verts[0].color==0xFEFFFFFF&&face->srcAlphaMode==5&&face->destAlphaMode==6,"Hit5/6 screen streaks preserve stored alpha and ordinary alpha blend");
    }
    {
        CTexture firstTexture,secondTexture;CEffectPrim napalm;napalm.m_pos={100,200,900};
        napalm.m_widthSize=20;napalm.m_heightSize=20;napalm.m_argb=0x19FFFFFF;
        napalm.m_texture={&firstTexture,&secondTexture};napalm.m_curMotion=1;
        g_renderer.m_rpAlphaList.clear();RenderReferenceScreenTexture(napalm);auto* face=g_renderer.m_rpAlphaList[0].second;
        check(g_renderer.m_rpAlphaList.size()==2&&face->tex==&secondTexture&&face->m_verts[0].color==0x19FFFFFF,
            "Napalm screen renderer selects advanced animation texture and packed rising alpha");
        check(near(face->m_verts[0].x,79.5f)&&near(face->m_verts[0].y,219.5f)&&near(face->m_verts[1].y,179.5f)&&near(face->m_verts[2].x,119.5f)
            &&near(face->m_verts[2].y,219.5f),"Napalm centered square uses original unscaled screen half extents");
        napalm.m_curMotion=2;g_renderer.m_rpAlphaList.clear();RenderReferenceScreenTexture(napalm);
        check(g_renderer.m_rpAlphaList.empty(),"Napalm exhausted animation does not substitute a texture or glow");
    }
    {
        CTexture firstTexture,secondTexture;CEffectPrim orbit;
        orbit.m_type=PP_2DTEXTUREORBIT;orbit.m_pos={140,260,0};orbit.m_widthSize=18;orbit.m_heightSize=24;
        orbit.m_argb=0xA0C0E0FF;orbit.m_texture={&firstTexture,&secondTexture};orbit.m_curMotion=1;orbit.m_roll=35.8f;
        g_renderer.m_rpAlphaList.clear();RenderReferenceScreenTexture(orbit);
        check(g_renderer.m_rpAlphaList.size()==2&&g_renderer.m_rpAlphaList[0].second->tex==&secondTexture
            &&g_renderer.m_rpAlphaList[0].second->m_verts[0].color==0xA0C0E0FF,
            "2D texture orbit uses shared screen renderer with current-motion texture and stored color");
        check(g_renderer.m_rpAlphaList[0].second->m_verts[0].z==0.000001f
            &&g_renderer.m_rpAlphaList[0].second->srcAlphaMode==D3DBLEND_SRCALPHA,
            "2D texture orbit preserves fixed screen depth and shared flag-one material");
    }
    {
        CTexture texture;CEffectPrim nonselected;
        nonselected.m_type=PP_2DTEXTURE;nonselected.m_pos={140,260,0};
        nonselected.m_widthSize=18;nonselected.m_heightSize=24;nonselected.m_argb=0xA0C0E0FF;
        nonselected.m_texture={&texture};nonselected.m_pattern=0;nonselected.m_roll=0;
        g_renderer.m_rpAlphaList.clear();RenderReferenceScreenTexture(nonselected);
        auto* face=g_renderer.m_rpAlphaList.empty()?nullptr:g_renderer.m_rpAlphaList[0].second;
        check(g_renderer.m_rpAlphaList.size()==2&&face&&face->tex==&texture
            &&near(face->m_verts[0].z,0.000001f)&&near(face->m_verts[0].oow,0.99999899f)
            &&face->m_verts[0].color==0xA0C0E0FF
            &&near(face->m_verts[0].x,nonselected.m_pos.x-18.5f),
            "nonselected master PP_2DTEXTURE uses source screen geometry");
    }
    {
        CEffectPrim circle;circle.m_numSegments=2;circle.m_size=2;circle.m_arcAngle=90;circle.m_renderFlag=0x201;
        circle.m_texture.push_back(&g_texMgr.texture);
        for(int i=0;i<4;++i)circle.m_matrix.m[i][i]=1;
        circle.m_matrix.m[3][0]=3;circle.m_matrix.m[3][1]=4;circle.m_matrix.m[3][2]=5;
        circle.m_segments[0].pos={10,20,50};circle.m_segments[0].radius=6;circle.m_segments[0].size=50;
        circle.m_segments[0].alpha=0;circle.m_segments[0].argb=0x12345678;
        circle.m_segments[1].pos={20,30,60};circle.m_segments[1].radius=10;circle.m_segments[1].size=100;
        circle.m_segments[1].argb=0x00FEABCD;
        circle.m_segments[0].transform.m[3][0]=100000;
        g_renderer.m_hpc=100;g_renderer.m_vpc=100;g_renderer.m_xoffset=1;g_renderer.m_yoffset=2;
        auto clearQueues=[&]{
            g_renderer.m_rpAlphaOPList.clear();g_renderer.m_rpLMGroundList.clear();g_renderer.m_rpLMLightList.clear();g_renderer.m_rpFaceList.clear();
            g_renderer.m_rpAlphaDepthList.clear();g_renderer.m_rpAlphaList.clear();g_renderer.m_rpAlphaNoDepthList.clear();
            g_renderer.m_rpEmissiveDepthList.clear();g_renderer.m_rpEmissiveList.clear();g_renderer.m_rpEmissiveNoDepthList.clear();
            silentDevice.draws.clear();
        };
        {
            CEffectPrim segment; CTexture firstTexture, animatedTexture;
            segment.m_numSegments=2; segment.m_renderFlag=0x101; segment.m_curMotion=1;
            segment.m_texture={&firstTexture,&animatedTexture};
            segment.m_segments[0].argb=0xA1B2C3D4u; segment.m_segments[1].argb=0x55667788u;
            segment.m_segmentQuadPositions[0]={vector3d{1,2,100},vector3d{3,4,100},vector3d{5,6,100},vector3d{7,8,100}};
            segment.m_segmentQuadPositions[1]={vector3d{11,12,100},vector3d{13,14,100},vector3d{15,16,100},vector3d{17,18,100}};
            clearQueues(); RenderReferenceSegment(segment,view);
            check(g_renderer.m_rpAlphaDepthList.size()==4,
                "3D spline/spiral emits two ordered texture triangles for every history segment");
            if(g_renderer.m_rpAlphaDepthList.size()==4) {
                const auto* first=g_renderer.m_rpAlphaDepthList[0].second;
                const auto* second=g_renderer.m_rpAlphaDepthList[1].second;
                const auto* third=g_renderer.m_rpAlphaDepthList[2].second;
                check(first->tex==&firstTexture&&first->m_verts[0].color==0xA1B2C3D4u
                    &&first->m_verts[0].x==321.0f&&first->m_verts[1].x==323.0f&&first->m_verts[2].x==325.0f
                    &&first->m_verts[0].tu==0.0f&&first->m_verts[0].tv==1.0f
                    &&first->m_verts[1].tu==1.0f&&first->m_verts[1].tv==1.0f
                    &&first->m_verts[2].tu==0.0f&&first->m_verts[2].tv==0.0f,
                    "first segment face preserves source corners, color, slot-zero texture and UV order");
                check(second->m_verts[0].x==327.0f&&second->m_verts[1].x==325.0f&&second->m_verts[2].x==321.0f
                    &&second->m_verts[0].y==248.0f&&second->m_verts[0].tu==1.0f&&second->m_verts[0].tv==0.0f
                    &&second->m_verts[1].tu==0.0f&&second->m_verts[1].tv==0.0f
                    &&second->m_verts[2].tu==1.0f&&second->m_verts[2].tv==1.0f,
                    "opposing segment face preserves source reversed corners and UV order");
                check(third->tex==&firstTexture&&third->m_verts[0].color==0x55667788u
                    &&third->m_verts[0].x==331.0f&&third->m_verts[1].x==333.0f
                    &&third->primType==D3DPT_TRIANGLELIST&&third->numVerts==3
                    &&third->srcAlphaMode==D3DBLEND_SRCALPHA&&third->destAlphaMode==D3DBLEND_INVSRCALPHA,
                    "each segment keeps its own packed color and legacy reference triangle material");
            }
            segment.m_texture[0]=&CTexMgr::s_dummy_texture; clearQueues(); RenderReferenceSegment(segment,view);
            check(g_renderer.m_rpAlphaDepthList.empty(),"missing slot-zero segment texture emits no substitute geometry");
        }
        clearQueues();RenderReferenceCircle(circle,view);
        check(g_renderer.m_rpAlphaNoDepthList.size()==16,"circle emits two independent triangles for each arc and every segment");
        auto* first=g_renderer.m_rpAlphaNoDepthList[0].second;
        auto* second=g_renderer.m_rpAlphaNoDepthList[1].second;
        check(first->primType==D3DPT_TRIANGLELIST && first->numVerts==3 && first!=second,"reference circle keeps distinct pooled triangle submissions");
        check(near(first->m_verts[0].x,320+17.0f/55*100) && near(first->m_verts[0].y,240+24.0f/55*100),
            "circle uses primitive width and current matrix translation with segment position and half-screen projection");
        check(near(first->m_verts[1].x,320+13.0f/55*100) && near(first->m_verts[1].y,240+28.0f/55*100)
            && near(first->m_verts[2].x,320+19.0f/55*100),"first triangle preserves inner-start, inner-end, outer-start order");
        check(near(second->m_verts[0].y,240+30.0f/55*100) && near(second->m_verts[1].x,first->m_verts[2].x)
            && near(second->m_verts[2].y,first->m_verts[1].y),"second triangle preserves outer-end, outer-start, inner-end order");
        check(first->m_verts[0].color==0x12345678 && first->m_verts[0].specular==0xFF000000,
            "circle uses stored segment ARGB without current-alpha clamp or tint reconstruction");
        check(g_renderer.m_rpAlphaNoDepthList[8].second->m_verts[0].color==0x00FEABCD,
            "circle submits even transparent history segments with their own color");
        check(first->m_verts[0].tu==0 && first->m_verts[0].tv==1 && first->m_verts[1].tu==0.25f && first->m_verts[2].tv==0
            && second->m_verts[0].tu==0.25f && second->m_verts[1].tu==0 && second->m_verts[2].tv==1,
            "circle preserves both reference triangle UV orders");
        check(g_renderer.m_rpAlphaNoDepthList[6].second->m_verts[0].tu==0.75f && g_renderer.m_rpAlphaNoDepthList[8].second->m_verts[0].tu==0,
            "circle UV phase wraps after four wedges");
        check(near(first->m_verts[0].oow,1.0f/55) && first->destAlphaMode==6 && first->srcAlphaMode==5,
            "circle has no invented eighty-unit cutoff and Blessing flags retain ordinary alpha blending");
        circle.m_arcAngle=120;clearQueues();RenderReferenceCircle(circle,view);
        check(g_renderer.m_rpAlphaNoDepthList.size()==12 && g_renderer.m_rpAlphaNoDepthList[6].second->m_verts[0].tu==0.75f,
            "circle carries UV phase across segment boundaries instead of restarting it");
        circle.m_numSegments=1;circle.m_arcAngle=90.5f;clearQueues();RenderReferenceCircle(circle,view);
        check(g_renderer.m_rpAlphaNoDepthList.size()==6 && near(g_renderer.m_rpAlphaNoDepthList[0].second->m_verts[1].x,320+13.0f/55*100),
            "2008 circle truncates each fractional next angle and compares against unrounded stop");
        circle.m_arcAngle=360;g_sinTable[360]=0.125f;clearQueues();RenderReferenceCircle(circle,view);
        check(g_renderer.m_rpAlphaNoDepthList.size()==2 && near(g_renderer.m_rpAlphaNoDepthList[1].second->m_verts[0].y,240+24.75f/55*100),
            "circle reads endpoint table index three-sixty directly without wrapping to zero");
        circle.m_arcAngle=0;clearQueues();RenderReferenceCircle(circle,view);
        check(g_renderer.m_rpAlphaNoDepthList.empty(),"invalid circle arc does not invent replacement geometry");
        circle.m_arcAngle=36;circle.m_numSegments=0;RenderReferenceCircle(circle,view);
        check(g_renderer.m_rpAlphaNoDepthList.empty(),"zero reference segments produce no circle submissions");
        circle.m_numSegments=1;circle.m_texture[0]=nullptr;RenderReferenceCircle(circle,view);
        check(g_renderer.m_rpAlphaNoDepthList.empty(),"missing circle texture does not substitute another asset");
        circle.m_texture[0]=&CTexMgr::s_dummy_texture;RenderReferenceCircle(circle,view);
        check(g_renderer.m_rpAlphaNoDepthList.empty(),"dummy circle texture produces no replacement geometry");
        circle.m_texture[0]=&g_texMgr.texture;circle.m_numSegments=50;circle.m_arcAngle=360;
        for(auto& segment:circle.m_segments)segment=circle.m_segments[0];
        clearQueues();RenderReferenceCircle(circle,view);
        check(circle.m_segments.size()==50 && g_renderer.m_rpAlphaNoDepthList.size()==100,"circle retains all fifty original history segments without reconstructed twelve-slot truncation");
        tlvertex3d projected;ProjectReferenceEffectVertex({0,0,10},view,projected);
        check(projected.x==320 && projected.y==240 && near(projected.z,0),"reference triangle projection has no half-pixel displacement or configurable-screen-offset substitution");
        ProjectReferenceEffectVertex({1,2,5},view,projected);
        check(projected.z<0 && near(projected.oow,0.2f),"reference triangle projection preserves near device depths without clamping");
        ProjectReferenceEffectVertex({0,0,2000},view,projected);
        check(projected.z>1,"reference triangle projection preserves far device depths without clamping");
        ProjectReferenceEffectVertex({0,0,-50},view,projected);
        check(projected.oow<0,"reference triangle projection does not invent behind-camera culling");
        auto makeFace=[&](float a,float b,float c,CTexture* tex){
            auto* face=g_renderer.BorrowNullRP();face->verts=face->m_verts;face->numVerts=3;face->tex=tex;
            face->m_verts[0].oow=a;face->m_verts[1].oow=b;face->m_verts[2].oow=c;return face;
        };
        CTexture farTex,nearTex;
        auto* face=makeFace(0.8f,0.2f,0.01f,&nearTex);
        clearQueues();g_renderer.AddReferenceEffectTriangle(face,1);
        check(g_renderer.m_rpAlphaList.size()==1 && g_renderer.m_rpAlphaList[0].first==0.8f,"reference ordinary alpha triangle sorts by its first vertex, not minimum depth");
        g_renderer.AddReferenceEffectTriangle(face,3);
        check(g_renderer.m_rpEmissiveList.size()==1 && near(g_renderer.m_rpEmissiveList[0].first,0.5f),"reference emissive triangle sorts by first-two-vertex midpoint");
        g_renderer.AddReferenceEffectTriangle(face,0x101);g_renderer.AddReferenceEffectTriangle(face,0x103);
        check(g_renderer.m_rpAlphaDepthList.size()==1 && g_renderer.m_rpEmissiveDepthList.size()==1,"reference flag one-hundred selects separate depth-writing passes");
        g_renderer.AddReferenceEffectTriangle(face,0x201);g_renderer.AddReferenceEffectTriangle(face,0x203);
        check(g_renderer.m_rpAlphaNoDepthList.size()==1 && g_renderer.m_rpEmissiveNoDepthList.size()==1
            && near(g_renderer.m_rpAlphaNoDepthList[0].first,0.5f),"reference flag two-hundred selects separate depth-disabled passes and midpoint sorting");
        g_renderer.AddReferenceEffectTriangle(face,0x301);
        check(g_renderer.m_rpAlphaDepthList.size()==2 && g_renderer.m_rpAlphaNoDepthList.size()==1,"reference depth-writing bit takes priority over no-depth bit");
        g_renderer.AddReferenceEffectTriangle(face,0x207);
        check(g_renderer.m_rpAlphaOPList.size()==1 && g_renderer.m_rpEmissiveNoDepthList.size()==1,"reference AlphaOP bit takes priority over emissive and no-depth bits");
        g_renderer.AddReferenceEffectTriangle(face,0x800);g_renderer.AddReferenceEffectTriangle(face,0x1000);g_renderer.AddReferenceEffectTriangle(face,0);
        check(g_renderer.m_rpLMGroundList.size()==1 && g_renderer.m_rpLMLightList.size()==1 && g_renderer.m_rpFaceList.size()==1,
            "reference non-alpha triangle routes ground, light and opaque queues");
        auto* far=makeFace(0.1f,0.3f,0.9f,&farTex);
        clearQueues();g_renderer.AddReferenceEffectTriangle(face,0x101);g_renderer.AddReferenceEffectTriangle(far,0x101);
        g_renderer.AddReferenceEffectTriangle(face,0x103);g_renderer.AddReferenceEffectTriangle(far,0x103);
        g_renderer.FlushEffectDepthLists();
        check(silentDevice.draws.size()==4 && silentDevice.draws[0].bound==&farTex && silentDevice.draws[1].bound==&nearTex
            && silentDevice.draws[2].bound==&farTex,"depth-writing alpha and emissive passes sort independently back-to-front");
        check(silentDevice.draws[0].depthWrite==1 && silentDevice.draws[0].depthTest==1 && silentDevice.draws[0].srcBlend==5 && silentDevice.draws[0].destBlend==6
            && silentDevice.draws[2].srcBlend==2 && silentDevice.draws[2].destBlend==2,"depth-writing passes retain reference depth and blend states");
        check(g_renderer.m_rpAlphaDepthList.empty() && g_renderer.m_rpEmissiveDepthList.empty(),"depth-writing queues are reclaimed after drawing");
        clearQueues();g_renderer.AddReferenceEffectTriangle(face,0x201);g_renderer.AddReferenceEffectTriangle(far,0x201);
        g_renderer.AddReferenceEffectTriangle(face,0x203);g_renderer.AddReferenceEffectTriangle(far,0x203);
        g_renderer.FlushEffectNoDepthLists();
        check(silentDevice.draws.size()==4 && silentDevice.draws[0].bound==&farTex && silentDevice.draws[1].bound==&nearTex
            && silentDevice.draws[2].bound==&farTex,"no-depth alpha and emissive passes sort independently back-to-front");
        check(silentDevice.draws[0].depthTest==0 && silentDevice.draws[0].depthWrite==0 && silentDevice.draws[0].srcBlend==5 && silentDevice.draws[0].destBlend==6
            && silentDevice.draws[2].srcBlend==2 && silentDevice.draws[2].destBlend==2,"no-depth passes disable testing/writes and retain source alpha/emissive blends");
        check(g_renderer.m_rpAlphaNoDepthList.empty() && g_renderer.m_rpEmissiveNoDepthList.empty() && silentDevice.states[0]==1 && silentDevice.states[2]==5 && silentDevice.states[3]==6,
            "no-depth queues clear and restore subsequent depth/blend state");
        clearQueues();g_renderer.AddReferenceEffectTriangle(face,0x201);g_renderer.FlushEffectNoDepthLists();const auto drawCount=silentDevice.draws.size();
        g_renderer.FlushEffectNoDepthLists();check(silentDevice.draws.size()==drawCount,"no-depth flush never redraws stale previous-frame faces");
    }
    {
        CEffectPrim panel;panel.m_widthSize=5;panel.m_heightSize=2.7f;panel.m_pos={10,20,100};panel.m_argb=0x78FFFFFF;
        panel.m_texture={&g_texMgr.texture};panel.m_renderFlag=1;panel.m_v1=0.9f;panel.m_v2=1.9f;
        MatrixIdentity(panel.m_matrix);MatrixIdentity(panel.m_rotMat);
        auto renderPanel=[&](){g_renderer.m_rpAlphaList.clear();g_renderer.m_rpAlphaNoDepthList.clear();RenderReferenceTexture(panel,view);};
        auto matches=[&](const tlvertex3d& v,vector3d point){tlvertex3d expected;ProjectReferenceEffectVertex(point,view,expected);
            return near(v.x,expected.x)&&near(v.y,expected.y)&&near(v.oow,expected.oow)&&near(v.z,expected.z);};
        renderPanel();check(g_renderer.m_rpAlphaList.size()==2,"SprayPond 3D texture emits exactly two original triangles without billboard fallback");
        auto* first=g_renderer.m_rpAlphaList[0].second;auto* second=g_renderer.m_rpAlphaList[1].second;
        check(matches(first->m_verts[0],{10,17.3f,105})&&matches(first->m_verts[1],{10,17.3f,95})&&matches(first->m_verts[2],{10,22.7f,105}),
            "SprayPond first triangle uses source local Y height and Z width with exact world projection");
        check(matches(second->m_verts[0],{10,22.7f,95})&&matches(second->m_verts[1],{10,22.7f,105})&&matches(second->m_verts[2],{10,17.3f,95}),
            "SprayPond second triangle retains original shared edge and winding");
        check(first->m_verts[0].tu==0&&first->m_verts[0].tv==0.9f&&first->m_verts[1].tu==1&&first->m_verts[1].tv==0.9f
            &&first->m_verts[2].tu==0&&first->m_verts[2].tv==1.9f&&second->m_verts[0].tu==1&&second->m_verts[0].tv==1.9f
            &&second->m_verts[1].tv==1.9f&&second->m_verts[2].tv==0.9f,"SprayPond both triangle UVs preserve unwrapped source V scroll beyond one");
        check(first->m_verts[0].color==0x78FFFFFF&&second->m_verts[2].color==0x78FFFFFF&&first->tex==&g_texMgr.texture&&first->srcAlphaMode==5&&first->destAlphaMode==6,
            "SprayPond uses stored source ARGB and selected texture with original alpha blend");
        MatrixAppendYRotation(panel.m_rotMat,GetRadian(90));panel.m_rotMat.m[3][0]=2;panel.m_rotMat.m[3][1]=3;panel.m_rotMat.m[3][2]=4;
        panel.m_matrix.m[3][0]=900;renderPanel();
        check(matches(g_renderer.m_rpAlphaList[0].second->m_verts[0],{17,20.3f,104}),
            "SprayPond geometry uses retained render rotation and translation independently of motion matrix");
        panel.m_renderFlag=0x201;renderPanel();check(g_renderer.m_rpAlphaList.empty()&&g_renderer.m_rpAlphaNoDepthList.size()==2,
            "SprayPond renderer forwards original render flags instead of forcing alpha depth mode");
        panel.m_texture.clear();renderPanel();check(g_renderer.m_rpAlphaList.empty()&&g_renderer.m_rpAlphaNoDepthList.empty(),
            "SprayPond missing texture emits no replacement geometry");
    }
    {
        CEffectPrim shard;CTexture secondTexture;shard.m_texture={&g_texMgr.texture,&secondTexture};shard.m_curMotion=1;
        shard.m_pos={100,200,900};shard.m_widthSize=10;shard.m_heightSize=120;shard.m_roll=0;shard.m_argb=0xC8123456;shard.m_renderFlag=0x205;
        auto renderShard=[&](){g_renderer.m_rpAlphaList.clear();g_renderer.m_rpAlphaOPList.clear();g_renderer.m_rpAlphaNoDepthList.clear();RenderReferenceScreenQuad(shard);};
        renderShard();check(g_renderer.m_rpAlphaList.size()==2,"ColdHit screen quad emits exactly two source shard triangles");
        auto* first=g_renderer.m_rpAlphaList[0].second;auto* second=g_renderer.m_rpAlphaList[1].second;
        check(first->m_verts[0].x==95&&first->m_verts[0].y==190&&first->m_verts[1].x==100&&first->m_verts[1].y==260
            &&first->m_verts[2].x==100&&first->m_verts[2].y==140,"ColdHit first shard face uses half dimensions and source five-sixths inset without half-pixel offset");
        check(second->m_verts[0].x==105&&second->m_verts[0].y==210&&second->m_verts[1].x==100&&second->m_verts[1].y==260
            &&second->m_verts[2].x==100&&second->m_verts[2].y==140,"ColdHit second shard face preserves opposite side inset and original shared-edge order");
        check(first->m_verts[0].tu==0&&first->m_verts[0].tv==0.5f&&first->m_verts[1].tu==0.5f&&first->m_verts[1].tv==1
            &&first->m_verts[2].tu==0.5f&&first->m_verts[2].tv==0&&second->m_verts[0].tu==1&&second->m_verts[0].tv==0.5f,
            "ColdHit shard faces retain original side and center UVs");
        check(first->m_verts[0].color==0xC8123456&&first->m_verts[1].color==0xFFFFFFFF&&first->m_verts[2].color==0xC8123456
            &&second->m_verts[1].color==0xFFFFFFFF&&first->tex==&secondTexture&&second->tex==&secondTexture
            &&first->m_verts[0].z==0.000001f&&first->m_verts[0].oow==0.99999899f
            &&g_renderer.m_rpAlphaOPList.empty()&&g_renderer.m_rpAlphaNoDepthList.empty(),
            "ColdHit uses original white center vertex selected motion fixed screen depth and hard-coded alpha flag one");
        shard.m_roll=-15.9f;renderShard();first=g_renderer.m_rpAlphaList[0].second;
        check(first->m_verts[0].x==static_cast<float>(-5.0*GetCos(-15)+10.0*GetSin(-15)+100)
            &&first->m_verts[0].y==static_cast<float>(-10.0*GetCos(-15)-5.0*GetSin(-15)+200),
            "ColdHit shard rotation truncates negative fractional roll and retains source float sine cosine products through screen translation");
        shard.m_argb=0;renderShard();first=g_renderer.m_rpAlphaList[0].second;
        check(g_renderer.m_rpAlphaList.size()==2&&first->m_verts[0].color==0&&first->m_verts[1].color==0xFFFFFFFF&&first->m_verts[2].color==0,
            "ColdHit retains source white center even when edge alpha reaches zero");
        shard.m_texture.clear();renderShard();check(g_renderer.m_rpAlphaList.empty(),"ColdHit missing shard texture produces no fallback geometry");
    }
    {
        const int differences[]={-271,-270,-80,-79,0,69,70,270,271};
        for(int difference:differences) {
            const bool front=(difference < 70 && difference > -80)||difference < -270||difference > 270;
            check(GumGangFrontFacing(difference,0,0.0f)==front
                && GumGangFrontFacing(difference,0,2.0f)==front
                && GumGangFrontFacing(difference,0,1.0f)==!front,
                "GumGang facing branch preserves strict source bounds and mode inversion");
        }
        CTexture textures[5];CEffectPrim prim;prim.m_pos={10,20,130};prim.m_renderFlag=0x205;
        for(auto& texture:textures)prim.m_texture.push_back(&texture);
        prim.m_gi[0].life=1;prim.m_gi[0].alphaT=0;prim.m_gi[0].distance=10.0f;
        prim.m_gi[0].height[1]=70;prim.m_gi[0].height[2]=0;prim.m_gi[0].height[3]=0;
        prim.m_gi[0].height[5]=130;prim.m_gi[0].height[6]=1;prim.m_gi[0].height[7]=0;
        prim.m_gi[0].height[9]=190;prim.m_gi[0].height[10]=2;prim.m_gi[0].height[11]=0;
        prim.m_gi[0].height[13]=250;prim.m_gi[0].height[14]=4;prim.m_gi[0].height[15]=0;
        prim.m_gi[0].arcBaselineY=-2;prim.m_gi[0].sparkY=-3;prim.m_gi[0].vecTPreY=-4;prim.m_gi[0].vecTNowY=-5;
        prim.m_gi[0].alphaB=0; // The source gates on life only, not alphaB.
        g_renderer.submissions.clear();g_renderer.flags.clear();
        RenderGumGang(prim,view,1.0f,100,100,255);
        check(g_renderer.submissions.size()==8,"GumGang submits two triangles for each of four source strips on a live band");
        if(g_renderer.submissions.size()==8) {
            const int textureIds[]={0,1,2,4};const int alphaValues[]={70,130,190,250};
            for(int strip=0;strip<4;++strip) {
                const auto& face=g_renderer.submissions[static_cast<size_t>(strip*2)];
                check(face.tex==&textures[textureIds[strip]]&&face.m_verts[0].color==((static_cast<u32>(alphaValues[strip])<<24)|0x006464FFu)
                    &&face.srcAlphaMode==D3DBLEND_SRCALPHA&&face.destAlphaMode==D3DBLEND_ONE
                    &&face.m_verts[0].tu==0&&face.m_verts[0].tv==1&&face.m_verts[2].tu==1&&face.m_verts[2].tv==0,
                    "GumGang strip preserves its source alpha, texture index, packed RGB, blend and UV rectangle");
                check(g_renderer.flags[static_cast<size_t>(strip*2)]==0x205,
                    "GumGang forwards the primitive render flags");
            }
            tlvertex3d expectedFirst;ProjectReferenceEffectVertex({static_cast<float>(GetCos(45)*10.0+4.0+10.0),18.0f,static_cast<float>(GetSin(45)*10.0+130.0)},view,expectedFirst);
            check(near(g_renderer.submissions[0].m_verts[0].x,expectedFirst.x)
                &&near(g_renderer.submissions[0].m_verts[0].y,expectedFirst.y)
                &&near(g_renderer.submissions[0].m_verts[0].oow,expectedFirst.oow),
                "GumGang first strip caches the four-unit angle offset and adds the primitive world position");
            tlvertex3d reversedFirst;ProjectReferenceEffectVertex({static_cast<float>(GetCos(135)*10.0+4.0+10.0),17.0f,static_cast<float>(GetSin(135)*10.0+130.0)},view,reversedFirst);
            check(near(g_renderer.submissions[2].m_verts[0].x,reversedFirst.x)
                &&near(g_renderer.submissions[2].m_verts[0].y,reversedFirst.y),
                "GumGang nonzero orientation mode reverses the first strip edge order");
        }
        prim.m_gi[0].flag[4]=1;prim.m_gi[1].life=1;prim.m_gi[1].distance=10;
        prim.m_gi[1].height=prim.m_gi[0].height;g_renderer.submissions.clear();g_renderer.flags.clear();
        RenderGumGang(prim,view,1.0f,100,100,255);
        check(g_renderer.submissions.size()==16&&g_renderer.submissions[0].m_verts[0].color==0x46FF1414u
            &&g_renderer.submissions[8].m_verts[0].color==0x46FF1414u,
            "GumGang red flag overrides and retains color across subsequent active bands");
        prim.m_gi[0].life=0;prim.m_gi[1].life=0;g_renderer.submissions.clear();
        RenderGumGang(prim,view,1.0f,100,100,255);
        check(g_renderer.submissions.empty(),"GumGang skips all bands whose life is not one");
    }
    {
        CTexture textures[5];CEffectPrim prim;prim.m_pos={10,20,130};prim.m_renderFlag=0x205;
        for(auto& texture:textures)prim.m_texture.push_back(&texture);
        EffectGi& gi=prim.m_gi[0];gi.life=1;gi.process=55;gi.distance=10.0f;gi.alphaT=0;
        gi.height[1]=80;gi.height[2]=0;gi.height[3]=0;
        gi.height[5]=130;gi.height[6]=1;gi.height[7]=0;
        gi.height[9]=190;gi.height[10]=2;gi.height[11]=0;
        gi.height[13]=250;gi.height[14]=4;gi.height[15]=0;gi.arcBaselineY=-2;
        const unsigned int colors[3][3]={{0x006464FFu,0x00000078u,0x00000078u},
            {0x005A5A50u,0x003C3C32u,0x001E1E14u},{0x00FF6464u,0x00780000u,0x00780000u}};
        const float scales[3]={1.0f,1.2f,2.0f};
        for(int mode=0;mode<3;++mode) {
            gi.flag[4]=static_cast<unsigned char>(mode);
            g_renderer.submissions.clear();g_renderer.flags.clear();
            RenderDoubleGumGang(prim,view);
            check(g_renderer.submissions.size()==24,"DoubleGumGang renders three layers of four strips for each live band");
            if(g_renderer.submissions.size()==24) {
                for(int layer=0;layer<3;++layer) {
                    const RPFace& face=g_renderer.submissions[static_cast<size_t>(layer*8)];
                    const float radius=10.0f*scales[layer];
                    tlvertex3d expected;ProjectReferenceEffectVertex({static_cast<float>(GetCos(45)*radius+4.0+10.0),18.0f,
                        static_cast<float>(GetSin(45)*radius+130.0)},view,expected);
                    check(face.m_verts[0].color==((55u<<24)|colors[mode][layer])&&face.tex==&textures[0]
                        &&face.destAlphaMode==D3DBLEND_ONE
                        &&near(face.m_verts[0].x,expected.x)&&near(face.m_verts[0].y,expected.y)
                        &&near(face.m_verts[0].oow,expected.oow),
                        "DoubleGumGang preserves source layer style, process-capped alpha, radius, texture and blend");
                }
                check(g_renderer.submissions[2].tex==&textures[1]&&g_renderer.submissions[4].tex==&textures[2]
                    &&g_renderer.submissions[6].tex==&textures[4],
                    "DoubleGumGang selects each strip's own source texture index");
                check(g_renderer.flags[0]==0x205&&g_renderer.flags[23]==0x205,
                    "DoubleGumGang forwards render flags for every layer and band");
            }
        }
        gi.life=0;g_renderer.submissions.clear();RenderDoubleGumGang(prim,view);
        check(g_renderer.submissions.empty(),"DoubleGumGang skips bands whose life is not one");
    }
    {
        CTexture textures[5];CEffectPrim prim;prim.m_pos={10,20,130};prim.m_renderFlag=0x205;
        for(auto& texture:textures)prim.m_texture.push_back(&texture);
        EffectGi& gi=prim.m_gi[0];gi.life=1;gi.alphaB=100;gi.alphaT=0;gi.maxHeight=10;gi.rotStart=0;
        gi.sparkX=1;gi.sparkY=2;gi.sparkZ=3;gi.vecBPreX=4;gi.arcBaselineY=5;gi.vecBPreZ=6;
        const unsigned int colors[5]={0x004646FFu,0x00FF4646u,0x0046FF46u,0x00FFFF5Au,0x00FF46FFu};
        for(int style=0;style<5;++style) {
            gi.flag[0]=static_cast<unsigned char>(style);gi.flag[3]=0;
            g_renderer.submissions.clear();g_renderer.flags.clear();RenderPokJuk(prim,view);
            check(g_renderer.submissions.size()==4,"PokJuk submits two additive textured triangles for each of two passes");
            if(g_renderer.submissions.size()==4) {
                tlvertex3d expected;ProjectReferenceEffectVertex({21,22,143},view,expected);
                const RPFace& first=g_renderer.submissions[0];const RPFace& secondPass=g_renderer.submissions[2];
                check(first.m_verts[0].color==((100u<<24)|colors[style])&&first.tex==&textures[0]
                    &&first.destAlphaMode==D3DBLEND_INVSRCALPHA&&near(first.m_verts[0].x,expected.x)
                    &&near(first.m_verts[0].y,expected.y),"PokJuk maps all style colors and the base spark offset");
                ProjectReferenceEffectVertex({23.5f,25,145.5f},view,expected);
                check(secondPass.m_verts[0].color==((50u<<24)|colors[style])
                    &&near(secondPass.m_verts[0].x,expected.x)&&near(secondPass.m_verts[0].y,expected.y),
                    "PokJuk divides second-pass alpha and uses vecBPre offset");
            }
        }
        gi.flag[3]=2;g_renderer.submissions.clear();RenderPokJuk(prim,view);
        check(g_renderer.submissions.size()==4&&g_renderer.submissions[0].tex==&textures[2]
            &&g_renderer.submissions[1].tex==&textures[0]&&g_renderer.submissions[2].tex==&textures[2]
            &&g_renderer.submissions[3].tex==&textures[0]&&g_renderer.submissions[0].m_verts[0].color==0x19FFFFFFu
            &&g_renderer.submissions[2].m_verts[0].color==0x0CFFFFFFu,
            "PokJuk textured branch selects its indexed texture, white tint and quarter alpha");
        gi.alphaB=0;g_renderer.submissions.clear();RenderPokJuk(prim,view);
        check(g_renderer.submissions.empty(),"PokJuk skips a live band when alphaB is not positive");
        gi.alphaB=100;gi.life=0;RenderPokJuk(prim,view);
        check(g_renderer.submissions.empty(),"PokJuk skips bands whose life is not one");
    }
    {
        CTexture textures[2];CEffectPrim prim;prim.m_pos={10,20,130};prim.m_renderFlag=0x105;
        prim.m_texture={&textures[0],&textures[1]};
        EffectGi& gi=prim.m_gi[0];gi.life=1;gi.alphaB=100;gi.rotStart=0;gi.maxHeight=10;
        gi.distance=99;gi.height[0]=2;gi.flag[0]=2;
        g_renderer.submissions.clear();g_renderer.flags.clear();RenderGuard(prim,view);
        check(g_renderer.submissions.size()==90,"Guard emits three tiers across three rows and five angular slices per live band");
        if(g_renderer.submissions.size()==90) {
            check(g_renderer.submissions[0].m_verts[0].color==0x64FF9B00u
                &&g_renderer.submissions[2].m_verts[0].color==0x64FFFF9Bu
                &&g_renderer.submissions[4].m_verts[0].color==0x64FF9B00u,
                "Guard special color mode preserves all three tier tints and alphaB");
            check(g_renderer.submissions[0].destAlphaMode==D3DBLEND_ONE
                &&g_renderer.submissions[2].destAlphaMode==D3DBLEND_ONE
                &&g_renderer.submissions[4].destAlphaMode==D3DBLEND_INVSRCALPHA
                &&g_renderer.submissions[4].tex==&textures[1]&&g_renderer.submissions[5].tex==&textures[0],
                "Guard uses source tier blend and split texture selection");
            tlvertex3d expected;ProjectReferenceEffectVertex({8.0f,2.34314575f,121.5147186f},view,expected);
            check(near(g_renderer.submissions[0].m_verts[0].x,expected.x)
                &&near(g_renderer.submissions[0].m_verts[0].y,expected.y),
                "Guard first tier preserves source row/slice transform and parent translation");
            ProjectReferenceEffectVertex({12.0f,2.34314575f,121.5147186f},view,expected);
            check(near(g_renderer.submissions[0].m_verts[1].x,expected.x)
                &&near(g_renderer.submissions[0].m_verts[1].y,expected.y),
                "Guard second corner preserves the source signed cross-section");
            ProjectReferenceEffectVertex({12.0f,-0.4857864f,124.3431458f},view,expected);
            check(near(g_renderer.submissions[0].m_verts[2].x,expected.x)
                &&std::fabs(g_renderer.submissions[0].m_verts[2].y-expected.y)<0.001f,
                "Guard third corner preserves source reverse-side geometry");
            ProjectReferenceEffectVertex({8.0f,4.201724f,121.3734f},view,expected);
            check(std::fabs(g_renderer.submissions[2].m_verts[0].x-expected.x)<0.001f
                &&std::fabs(g_renderer.submissions[2].m_verts[0].y-expected.y)<0.001f,
                "Guard second tier uses maxHeight plus 0.2 and its source vertical offset");
            ProjectReferenceEffectVertex({8.0f,2.646191f,119.8178f},view,expected);
            check(std::fabs(g_renderer.submissions[4].m_verts[0].x-expected.x)<0.001f
                &&std::fabs(g_renderer.submissions[4].m_verts[0].y-expected.y)<0.001f,
                "Guard third tier includes height[0] and its source inverse-alpha surface");
            check(g_renderer.flags[0]==0x105&&g_renderer.flags[89]==0x105,
                "Guard forwards primitive render flags for all submissions");
        }
        check(gi.distance==2.0f,"Guard preserves the source distance scratch mutation ending at two");
        gi.flag[0]=0;gi.alphaB=0;g_renderer.submissions.clear();RenderGuard(prim,view);
        check(g_renderer.submissions.size()==90&&g_renderer.submissions[0].m_verts[0].color==0x00CDFFCDu,
            "Guard has no alphaB gate and selects ordinary tier colors");
        gi.life=0;g_renderer.submissions.clear();RenderGuard(prim,view);
        check(g_renderer.submissions.empty(),"Guard skips bands whose life is not one");
    }
    {
        CTexture textures[21];CEffectPrim prim;prim.m_pos={10,20,130};prim.m_renderFlag=0x305;
        for(auto& texture:textures)prim.m_texture.push_back(&texture);
        EffectGi& gi=prim.m_gi[0];gi.life=1;gi.alphaB=200;gi.fullDisplayAngle=20;
        gi.rotStart=0;gi.distance=10;gi.riseAngle=0;
        g_renderer.submissions.clear();g_renderer.flags.clear();RenderRainbow(prim,view);
        check(g_renderer.submissions.size()==294,
            "Rainbow emits two triangle faces for 21 samples in each of seven bands");
        if(g_renderer.submissions.size()==294) {
            const unsigned int colors[7]={0xC8FF0000u,0xC8FF7E00u,0xC8FFFF00u,
                0xC800FF00u,0xC80000FFu,0xC87332C8u,0xC8FF00FFu};
            bool allColors=true;
            for(int pass=0;pass<7;++pass) {
                for(int sample=0;sample<21;++sample) {
                    const int face=pass*42+sample*2;
                    if(g_renderer.submissions[face].m_verts[0].color!=colors[pass]
                        ||g_renderer.submissions[face+1].m_verts[0].color!=colors[pass]) allColors=false;
                }
            }
            check(allColors,"Rainbow preserves all seven source switch colors and alphaB");
            check(g_renderer.submissions[0].tex==&textures[0]
                &&g_renderer.submissions[40].tex==&textures[0]
                &&g_renderer.submissions[42].tex==&textures[0]
                &&std::fabs(g_renderer.submissions[0].m_verts[0].tv)<0.0001f
                &&std::fabs(g_renderer.submissions[40].m_verts[0].tv-20.0f*0.047619049f)<0.0001f
                &&std::fabs(g_renderer.submissions[42].m_verts[0].tv)<0.0001f
                &&g_renderer.flags[0]==0x305&&g_renderer.flags[293]==0x305,
                "Rainbow uses the source texture, advances TexPart UV frames, restarts each pass and forwards flags");
            check(g_renderer.submissions[0].destAlphaMode==D3DBLEND_ONE,
                "Rainbow passes source PW zero through the additive blend mapping");
        }
        gi.alphaB=0;g_renderer.submissions.clear();RenderRainbow(prim,view);
        check(g_renderer.submissions.empty(),"Rainbow skips a live band when alphaB is not positive");
        gi.alphaB=200;gi.life=0;g_renderer.submissions.clear();RenderRainbow(prim,view);
        check(g_renderer.submissions.empty(),"Rainbow skips bands whose life is not one");
        gi.life=1;gi.fullDisplayAngle=-20;g_renderer.submissions.clear();RenderRainbow(prim,view);
        check(g_renderer.submissions.empty(),"Rainbow negative angle span retains the source zero-sample behavior");
    }
    {
        CTexture textures[2];CEffectPrim prim;prim.m_pos={100,200,300};prim.m_renderFlag=0x405;
        prim.m_texture={&textures[0],&textures[1]};
        EffectGi& gi=prim.m_gi[0];gi.life=1;gi.alphaB=180;gi.rotStart=0;gi.maxHeight=10;
        gi.height[0]=2;gi.height[4]=2.0f;
        gi.sparkX=4;gi.sparkY=5;gi.sparkZ=6;gi.vecTNowX=14;gi.vecTNowY=15;gi.vecTNowZ=16;
        g_renderer.submissions.clear();g_renderer.flags.clear();RenderTripleAttack(prim,view);
        check(g_renderer.submissions.size()==20,
            "TripleAttack emits five quads in each of two passes for one active band");
        if(g_renderer.submissions.size()==20) {
            const RPFace& first=g_renderer.submissions[0];
            const RPFace& secondPass=g_renderer.submissions[10];
            check(first.m_verts[0].color==0xB4FFFFFFu&&secondPass.m_verts[0].color==0xB4FFFFFFu
                &&first.destAlphaMode==D3DBLEND_INVSRCALPHA&&secondPass.destAlphaMode==D3DBLEND_ONE,
                "TripleAttack mode two uses white and source PW blend per pass");
            check(first.tex==&textures[0]&&g_renderer.submissions[1].tex==&textures[0]
                &&first.m_verts[0].tu==0.0f&&first.m_verts[0].tv==1.0f
                &&first.m_verts[1].tu==1.0f&&first.m_verts[1].tv==1.0f
                &&first.m_verts[2].tu==1.0f&&first.m_verts[2].tv==0.0f,
                "TripleAttack TN and F1 select texture zero for both triangles with fixed source UVs");
            tlvertex3d expected;
            ProjectReferenceEffectVertex({4.0f,3.0f,16.0f},view,expected);
            check(near(first.m_verts[0].x,expected.x)&&near(first.m_verts[0].y,expected.y),
                "TripleAttack preserves source seed endpoint and F1 suppresses parent translation");
            ProjectReferenceEffectVertex({4.0f,12.510565f,9.090170f},view,expected);
            check(near(first.m_verts[1].x,expected.x)&&near(first.m_verts[1].y,expected.y),
                "TripleAttack advances angular sample using source radius and orientation table");
            ProjectReferenceEffectVertex({14.0f,22.510565f,19.090170f},view,expected);
            check(near(first.m_verts[2].x,expected.x)&&near(first.m_verts[2].y,expected.y),
                "TripleAttack carries the top endpoint alongside the bottom endpoint");
            check(g_renderer.flags[0]==0x405&&g_renderer.flags[19]==0x405,
                "TripleAttack forwards primitive render flags");
        }
        gi.height[4]=3.0f;g_renderer.submissions.clear();RenderTripleAttack(prim,view);
        if(g_renderer.submissions.size()==20) {
            tlvertex3d expected;ProjectReferenceEffectVertex({4.0f,3.0f,26.0f},view,expected);
            check(near(g_renderer.submissions[10].m_verts[0].x,expected.x)
                &&near(g_renderer.submissions[10].m_verts[0].y,expected.y),
                "TripleAttack mode three doubles the second-pass radius");
        } else check(false,"TripleAttack mode-three radius check has complete submissions");
        const unsigned int passZeroColors[3]={0xB4FFFF37u,0xB4FF59B6u,0xB4FFFFFFu};
        const unsigned int passOneColors[3]={0xB4FFFFC8u,0xB4FF59B6u,0xB4FFFFFFu};
        for(int modeIndex=0;modeIndex<3;++modeIndex) {
            gi.height[4]=modeIndex==0?0.0f:(modeIndex==1?3.0f:2.0f);
            g_renderer.submissions.clear();RenderTripleAttack(prim,view);
            const unsigned int firstColor=g_renderer.submissions.empty()?0:g_renderer.submissions[0].m_verts[0].color;
            const unsigned int secondColor=g_renderer.submissions.size()<11?0:g_renderer.submissions[10].m_verts[0].color;
            check(firstColor==passZeroColors[modeIndex]&&secondColor==passOneColors[modeIndex],
                "TripleAttack preserves all three source mode color branches across both passes");
        }
        gi.height[4]=2.0f;prim.m_gi[1].life=1;prim.m_gi[1].alphaB=180;g_renderer.submissions.clear();
        RenderTripleAttack(prim,view);
        check(g_renderer.submissions.size()==40,"TripleAttack independently renders each active GI band");
        gi.alphaB=0;g_renderer.submissions.clear();RenderTripleAttack(prim,view);
        check(g_renderer.submissions.size()==20,"TripleAttack skips only the alpha-gated first band");
        gi.life=0;prim.m_gi[1].life=0;g_renderer.submissions.clear();RenderTripleAttack(prim,view);
        check(g_renderer.submissions.empty(),"TripleAttack skips inactive bands");
    }
    {
        C3dGround ground;CWorld world;world.m_ground=&ground;
        CGameMode mode;mode.m_world=&world;mode.serverX=10;mode.serverY=20;
        g_modeMgr.current=&mode;g_modeMgr.lookups=0;
        CEffectPrim prim;prim.m_pos={12,0,34};EffectGi& gi=prim.m_gi[0];
        gi.life=1;gi.alphaB=0;gi.rotStart=0;gi.maxHeight=1.0f;
        RenderGroundSample(prim,view);
        check(ground.calls.size()==98,"GroundSample renders both 7x7 tile layers for a live band without an alpha gate");
        if(ground.calls.size()==98) {
            const GroundTileCall& first=ground.calls[0];
            const GroundTileCall& second=ground.calls[1];
            const GroundTileCall& last=ground.calls[97];
            check(first.x==7&&first.y==17&&last.x==13&&last.y==23
                &&mode.coordinateCalls==1&&g_modeMgr.lookups==2,
                "GroundSample converts the primitive position once and uses the source 7x7 map window");
            check(first.color==0xAA9999FFu&&second.color==0x559999FFu
                &&first.texture==&g_texMgr.texture&&second.texture==&g_texMgr.texture,
                "GroundSample submits the two source layer colors and exact texture");
            check(near(first.uv[0].x,0.0f)&&near(first.uv[0].y,0.0f)
                &&near(first.uv[1].x,1.0f/7.0f)&&near(first.uv[1].y,0.0f)
                &&near(first.uv[2].x,0.0f)&&near(first.uv[2].y,1.0f/7.0f)
                &&near(second.uv[0].x,-0.05f)&&near(second.uv[0].y,-0.05f),
                "GroundSample preserves normalized inner and expanded outer UV geometry");
        }
        gi.life=0;ground.calls.clear();RenderGroundSample(prim,view);
        check(ground.calls.empty(),"GroundSample skips bands whose life is not one");
    }
    {
        CEffectPrim prim;EffectGi& first=prim.m_gi[0];EffectGi& wrap=prim.m_gi[1];
        first.life=1;first.alphaB=0;first.rotStart=0;first.maxHeight=99.0f;
        wrap.life=1;wrap.rotStart=359;wrap.maxHeight=99.0f;
        prim.m_gi[2].life=0;prim.m_gi[2].rotStart=17;prim.m_gi[2].maxHeight=88.0f;
        UpdateGroundSample(&prim);
        check(first.rotStart==1&&near(first.maxHeight,g_sinTable[1]*0.5f+1.5f)
            &&wrap.rotStart==0&&near(wrap.maxHeight,1.5f),
            "PrimGroundSample increments live rotations and wraps 359 to zero before recomputing scale");
        check(prim.m_gi[2].rotStart==17&&near(prim.m_gi[2].maxHeight,88.0f),
            "PrimGroundSample leaves non-live GI slots unchanged");
    }
    {
        CTexture texture;CEffectPrim prim;prim.m_texture={&texture};prim.m_pos={10,20,30};prim.m_renderFlag=5;
        EffectGi& gi=prim.m_gi[0];gi.life=1;gi.distance=10;gi.flag[1]=2;
        gi.vecBPreX=4;gi.arcBaselineY=-1;gi.vecBPreZ=6;
        gi.sparkX=7;gi.sparkY=-3;gi.sparkZ=8;
        gi.vecTPreY=0.5f;
        gi.vecTNowX=9;gi.vecTNowY=-40;gi.vecTNowZ=11;
        g_renderer.submissions.clear();g_renderer.flags.clear();
        RenderReferenceMapParticle(prim,view);
        check(g_renderer.submissions.size()==6,
            "MapParticle renders the three visible ordered bands as two triangles each and skips positive Y");
        if(g_renderer.submissions.size()==6) {
            const RPFace& first=g_renderer.submissions[0];
            const RPFace& secondBand=g_renderer.submissions[2];
            const RPFace& clampedBand=g_renderer.submissions[4];
            check(first.tex==&texture&&first.primType==D3DPT_TRIANGLELIST
                &&first.srcAlphaMode==D3DBLEND_SRCALPHA&&first.destAlphaMode==D3DBLEND_ONE,
                "MapParticle uses texture zero, triangle submission, and source additive blend state");
            check(first.m_verts[0].color==0x969BFF9Bu&&secondBand.m_verts[0].color==0x7A9BFF9Bu
                &&clampedBand.m_verts[0].color==0x009BFF9Bu,
                "MapParticle preserves base alpha, vertical fade alpha, and lower clamp branches");
            check(first.m_verts[0].tu==0.0f&&first.m_verts[0].tv==1.0f
                &&first.m_verts[1].tu==1.0f&&first.m_verts[1].tv==1.0f
                &&first.m_verts[2].tu==1.0f&&first.m_verts[2].tv==0.0f
                &&g_renderer.flags[0]==5&&g_renderer.flags[5]==5,
                "MapParticle preserves the canonical triangle corner/UV order and primitive render flags");
            tlvertex3d expected{};
            ProjectReferenceEffectVertex({14.0f,19.0f,36.0f},view,expected);
            check(near(first.m_verts[0].x,expected.x)&&near(first.m_verts[0].y,expected.y),
                "MapParticle uses executable-zero orientation values and parent translation");
            check(near(first.m_verts[0].x,first.m_verts[1].x)
                &&near(first.m_verts[0].y,first.m_verts[1].y),
                "MapParticle preserves the executable's degenerate zero-radius corner geometry");
        }
        gi.flag[1]=0;gi.arcBaselineY=-0.5f;gi.sparkY=0.5f;gi.vecTPreY=0.5f;gi.vecTNowY=0.5f;
        g_renderer.submissions.clear();g_renderer.flags.clear();RenderReferenceMapParticle(prim,view);
        check(g_renderer.submissions.size()==2&&g_renderer.submissions[0].m_verts[0].color==0x82FFFFFFu,
            "MapParticle non-mode-two branch uses white RGB and the base alpha");
        gi.life=0;g_renderer.submissions.clear();RenderReferenceMapParticle(prim,view);
        check(g_renderer.submissions.empty(),"MapParticle skips GI lanes whose life is not one");
        for(EffectGi& lane:prim.m_gi){lane.life=1;lane.distance=5.0f;lane.arcBaselineY=0;
            lane.sparkY=0;lane.vecTPreY=0;lane.vecTNowY=0;}
        g_renderer.submissions.clear();RenderReferenceMapParticle(prim,view);
        check(g_renderer.submissions.size()==32,
            "MapParticle visits all four live GI lanes and submits all four bands per lane");
    }
    {
        CTexture texture;CEffectPrim prim;prim.m_texture={&texture};prim.m_pos={100,200,300};prim.m_renderFlag=5;
        EffectGi& firstLane=prim.m_gi[0];firstLane.life=1;firstLane.alphaB=170;
        firstLane.distance=10;firstLane.rotStart=0;firstLane.sparkX=4;firstLane.sparkY=-2;firstLane.sparkZ=6;
        EffectGi& wrappedLane=prim.m_gi[1];wrappedLane.life=1;wrappedLane.alphaB=100;
        wrappedLane.distance=5;wrappedLane.rotStart=355;wrappedLane.sparkX=11;wrappedLane.sparkY=12;wrappedLane.sparkZ=13;
        g_renderer.submissions.clear();g_renderer.flags.clear();RenderReferenceSoulBreaker(prim,view);
        check(g_renderer.submissions.size()==4,
            "SoulBreaker renders two live positive-alpha lanes as two triangles each");
        if(g_renderer.submissions.size()==4) {
            const RPFace& first=g_renderer.submissions[0];
            const RPFace& wrap=g_renderer.submissions[2];
            check(first.tex==&texture&&first.destAlphaMode==D3DBLEND_INVSRCALPHA
                &&first.srcAlphaMode==D3DBLEND_SRCALPHA&&first.m_verts[0].color==0xAAC8C8FFu,
                "SoulBreaker uses texture zero, source inverse-alpha blend, and canonical RGB 200/200/255");
            check(first.m_verts[0].tu==0.0f&&first.m_verts[0].tv==1.0f
                &&first.m_verts[1].tu==1.0f&&first.m_verts[1].tv==1.0f
                &&first.m_verts[2].tu==1.0f&&first.m_verts[2].tv==0.0f
                &&g_renderer.flags[0]==5&&g_renderer.flags[3]==5,
                "SoulBreaker preserves RenderTeiRect triangle order, UVs, and raw render flags");
            tlvertex3d expected{};
            ProjectReferenceEffectVertex({static_cast<float>(g_cosTable[45]*10.0+4.0),-2.0f,
                static_cast<float>(g_sinTable[45]*10.0+6.0)},view,expected);
            check(near(first.m_verts[0].x,expected.x)&&near(first.m_verts[0].y,expected.y),
                "SoulBreaker begins at RotStart minus 45 plus 90 and omits parent-position translation");
            ProjectReferenceEffectVertex({static_cast<float>(g_cosTable[40]*5.0+11.0),12.0f,
                static_cast<float>(g_sinTable[40]*5.0+13.0)},view,expected);
            check(near(wrap.m_verts[0].x,expected.x)&&near(wrap.m_verts[0].y,expected.y),
                "SoulBreaker wraps the first angle above 360 exactly once");
        }
        wrappedLane.alphaB=0;g_renderer.submissions.clear();RenderReferenceSoulBreaker(prim,view);
        check(g_renderer.submissions.size()==2,"SoulBreaker skips a live lane when alphaB is not positive");
        firstLane.life=0;wrappedLane.alphaB=100;g_renderer.submissions.clear();RenderReferenceSoulBreaker(prim,view);
        check(g_renderer.submissions.size()==2,"SoulBreaker skips a lane whose life is not one");
        wrappedLane.life=0;g_renderer.submissions.clear();RenderReferenceSoulBreaker(prim,view);
        check(g_renderer.submissions.empty(),"SoulBreaker emits nothing when all four GI lanes are inactive");
    }
    {
        CTexture texture;CEffectPrim prim;prim.m_texture={&texture};
        prim.m_pos={10,20,30};prim.m_renderFlag=9;
        EffectGi& red=prim.m_gi[0];red.life=1;red.alphaB=0;red.flag[0]=2;
        red.height[0]=8;red.vecBPreX=3;red.arcBaselineY=0;red.vecBPreZ=4;
        EffectGi& gray=prim.m_gi[1];gray.life=1;gray.alphaB=120;gray.flag[0]=3;
        gray.height[0]=6;gray.arcBaselineY=-1;
        EffectGi& white=prim.m_gi[2];white.life=1;white.alphaB=200;white.height[0]=5;
        white.arcBaselineY=1;
        g_renderer.submissions.clear();g_renderer.flags.clear();RenderReferencePeongUp(prim,view);
        check(g_renderer.submissions.size()==4,
            "PeongUp routes with no master and draws live lanes at relative Y<=0 without an alpha gate");
        if(g_renderer.submissions.size()==4){
            const RPFace& first=g_renderer.submissions[0];const RPFace& grayFace=g_renderer.submissions[2];
            check(first.tex==&texture&&first.m_verts[0].color==0x006969E1u
                &&grayFace.m_verts[0].color==0x78373737u,
                "PeongUp selects flag-2 blue and flag-3 gray RGB with lane alphaB");
            check(first.srcAlphaMode==D3DBLEND_SRCALPHA&&first.destAlphaMode==D3DBLEND_INVSRCALPHA
                &&first.m_verts[0].tu==0.0f&&first.m_verts[0].tv==1.0f
                &&g_renderer.flags[0]==9&&g_renderer.flags[3]==9,
                "PeongUp uses RenderTeiRect texture, blend, UV, and raw flag semantics");
            tlvertex3d expected{};ProjectReferenceEffectVertex({13,20,34},view,expected);
            check(near(first.m_verts[0].x,expected.x)&&near(first.m_verts[0].y,expected.y)
                &&near(first.m_verts[0].x,first.m_verts[1].x),
                "PeongUp follows executable-zero orientation globals, adds BPre and parent position, and keeps degenerate corners");
        }
        white.arcBaselineY=0;g_renderer.submissions.clear();RenderReferencePeongUp(prim,view);
        check(g_renderer.submissions.size()==6&&g_renderer.submissions[4].m_verts[0].color==0xC8FFFFFFu,
            "PeongUp includes the zero-Y boundary and uses white RGB for other flags");
        gray.life=0;red.life=0;white.life=0;g_renderer.submissions.clear();RenderReferencePeongUp(prim,view);
        check(g_renderer.submissions.empty(),"PeongUp skips every GI lane whose life is not one");
    }
    {
        CTexture texture;CEffectPrim prim;prim.m_texture={&texture};prim.m_pos={100,200,300};prim.m_renderFlag=11;
        EffectGi& first=prim.m_gi[0];first.life=1;first.alphaB=0;first.distance=9;
        first.vecBPreX=3;first.arcBaselineY=-4;first.vecBPreZ=5;
        EffectGi& second=prim.m_gi[1];second.life=1;second.alphaB=120;second.distance=17;
        second.vecBPreX=-2;second.arcBaselineY=8;second.vecBPreZ=1;
        second.height[0]=123; // Canonical radius is distance, not height[0].
        g_renderer.submissions.clear();g_renderer.flags.clear();g_modeMgr.lookups=0;
        RenderReferencePeong(prim,view);
        check(g_renderer.submissions.size()==4,
            "Peong renders every live lane with no alpha or vertical visibility gate");
        if(g_renderer.submissions.size()==4){
            const RPFace& firstFace=g_renderer.submissions[0];const RPFace& secondFace=g_renderer.submissions[2];
            check(firstFace.tex==&texture&&firstFace.m_verts[0].color==0x00FFFFFFu
                &&secondFace.m_verts[0].color==0x78FFFFFFu,
                "Peong uses texture slot zero, white RGB, and the stored alphaB even when it is zero");
            check(firstFace.destAlphaMode==D3DBLEND_INVSRCALPHA
                &&firstFace.m_verts[0].tu==0.0f&&firstFace.m_verts[0].tv==1.0f
                &&g_renderer.flags[0]==11&&g_renderer.flags[3]==11,
                "Peong preserves PW=1 blend, full UVs, and raw primitive render flags");
            tlvertex3d expected{};ProjectReferenceEffectVertex({103,196,305},view,expected);
            check(near(firstFace.m_verts[0].x,expected.x)&&near(firstFace.m_verts[0].y,expected.y)
                &&near(firstFace.m_verts[0].x,firstFace.m_verts[1].x),
                "Peong follows executable-zero orientation, offsets by BPre and primitive position, and produces degenerate corners");
        }
        check(g_modeMgr.lookups==0,"Peong render has no effect-launch or game-mode side effects");
        second.life=0;first.life=0;g_renderer.submissions.clear();RenderReferencePeong(prim,view);
        check(g_renderer.submissions.empty(),"Peong skips lanes whose life is not one");
        for(EffectGi& gi:prim.m_gi)gi.life=1;
        g_renderer.submissions.clear();RenderReferencePeong(prim,view);
        check(g_renderer.submissions.size()==8,"Peong visits all four live GI lanes and submits one two-face rectangle each");
    }
    {
        CTexture texture; CEffectPrim prim; prim.m_texture={&texture}; prim.m_pos={10,20,30}; prim.m_renderFlag=7;
        for(int i=0;i<2;++i){auto& gi=prim.m_gi[static_cast<size_t>(i)];gi.life=1;gi.alphaB=180;
            gi.height[0]=2;gi.height[1]=3;gi.height[2]=4;}
        prim.m_gi[2].life=1;prim.m_gi[2].alphaB=180;prim.m_gi[2].height[0]=10;
        prim.m_gi[2].height[1]=10;prim.m_gi[2].height[2]=2;prim.m_gi[2].height[3]=5;prim.m_gi[2].distance=5;
        g_renderer.submissions.clear();g_renderer.flags.clear();RenderReferenceRectUp2(prim,view);
        check(g_renderer.submissions.size()==18,
            "RectUp2 submits four rectangles for each first-pass lane and one sweep rectangle for GI2");
        if(g_renderer.submissions.size()==18){
            const RPFace& first=g_renderer.submissions[0];const RPFace& sweep=g_renderer.submissions[16];
            check(first.tex==&texture&&first.srcAlphaMode==D3DBLEND_SRCALPHA
                &&first.destAlphaMode==D3DBLEND_INVSRCALPHA&&first.m_verts[0].color==0xB4C8FAC8u,
                "RectUp2 uses slot zero, inverse-alpha blending, and the first-pass green color");
            check(sweep.m_verts[0].color==0xB496FA96u&&first.m_verts[0].tu==0.0f
                &&first.m_verts[0].tv==1.0f&&g_renderer.flags[0]==7&&g_renderer.flags[17]==7,
                "RectUp2 preserves sweep color, RenderTeiRect UV order, and raw render flags");
            tlvertex3d expected{};ProjectReferenceEffectVertex({12,19,33},view,expected);
            check(near(first.m_verts[0].x,expected.x)&&near(first.m_verts[0].y,expected.y),
                "RectUp2 adds parent position because canonical RenderTeiRect F1 is zero");
            check(near(prim.m_gi[0].arcBaselineY,-1)&&near(prim.m_gi[0].sparkY,-1)
                &&near(prim.m_gi[0].vecTNowY,-1)&&near(prim.m_gi[0].vecTPreY,-1),
                "RectUp2 stores source-space corners at Y=-1 before local height translation");
        }
        prim.m_gi[0].alphaB=0;prim.m_gi[1].life=0;g_renderer.submissions.clear();
        RenderReferenceRectUp2(prim,view);
        check(g_renderer.submissions.size()==2,"RectUp2 independently gates the first-pass life and alpha and retains GI2");
        prim.m_gi[2].distance=20;g_renderer.submissions.clear();RenderReferenceRectUp2(prim,view);
        check(g_renderer.submissions.size()==2,"RectUp2 uses strict interval boundaries so distance 2w enters interval two");
        prim.m_gi[2].distance=45;g_renderer.submissions.clear();RenderReferenceRectUp2(prim,view);
        check(g_renderer.submissions.size()==2,"RectUp2 uses strict interval boundaries so distance 4w enters interval three");
        prim.m_gi[2].distance=65;g_renderer.submissions.clear();RenderReferenceRectUp2(prim,view);
        check(g_renderer.submissions.size()==2,"RectUp2 routes distance beyond 6w into interval four");
        auto& sweep=prim.m_gi[2];
        sweep.height[3]=30;sweep.distance=5;g_renderer.submissions.clear();RenderReferenceRectUp2(prim,view);
        check(g_renderer.submissions.size()==4&&near(sweep.vecBPreZ,5)&&near(sweep.sparkZ,-10)
            &&near(sweep.vecTNowX,-5),
            "RectUp2 interval one clamps the bottom edge and advances the adjacent edge on overflow");
        sweep.distance=25;g_renderer.submissions.clear();RenderReferenceRectUp2(prim,view);
        check(g_renderer.submissions.size()==4&&near(sweep.vecTNowX,-10)&&near(sweep.vecTPreZ,5),
            "RectUp2 interval two clamps the left edge and transfers the overflow to the next corner");
        sweep.distance=55;g_renderer.submissions.clear();RenderReferenceRectUp2(prim,view);
        check(g_renderer.submissions.size()==4&&near(sweep.vecTPreZ,10)&&near(sweep.vecBPreX,15),
            "RectUp2 interval three clamps the top edge and transfers the overflow to the next corner");
        sweep.distance=75;g_renderer.submissions.clear();RenderReferenceRectUp2(prim,view);
        check(g_renderer.submissions.size()==4&&near(sweep.vecBPreX,10)&&near(sweep.sparkZ,-15),
            "RectUp2 interval four clamps the right edge and transfers the overflow to the next corner");
        if(g_renderer.submissions.size()==4){tlvertex3d expected{};
            ProjectReferenceEffectVertex({20,19,15},view,expected);
            check(near(g_renderer.submissions[2].m_verts[0].x,expected.x)
                &&near(g_renderer.submissions[2].m_verts[0].y,expected.y),
                "RectUp2 interval four overflow submits the BNow-to-TNow edge after the base edge");}
    }
    std::printf("%d particle render checks passed; no media loaded or rendered.\n",checks-failures);
    return failures?1:0;
}
