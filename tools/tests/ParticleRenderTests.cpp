#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <list>

using u32 = std::uint32_t;
struct RECT { long left, top, right, bottom; };
#include "ParticleActTypes.inc"
struct vector3d { float x=0, y=0, z=0; };
struct matrix { float m[4][4]{}; };
void MatrixIdentity(matrix& m){m={};for(int i=0;i<4;++i)m.m[i][i]=1;}
#include "ParticleMatrixBodies.inc"
#include "EffectSegmentTypes.inc"
struct CTexture {
    unsigned int m_w=4, m_h=4, m_updateWidth=2, m_updateHeight=2;
    unsigned int m_surfaceUpdateWidth=2,m_surfaceUpdateHeight=2;
};
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
    int m_numSegments=1,m_spriteAction=0,m_curMotion=0,m_pattern=0;
    u32 m_renderFlag=5;
    float m_roll=0,m_size=1,m_arcAngle=36,m_longitude=0;
    matrix m_matrix;
    matrix m_rotMat;
    vector3d m_pos;
    float m_innerSize=0,m_outerSize=0,m_heightSize=0,m_widthSize=0,m_radius=0;
    float m_v1=0,m_v2=1;
    u32 m_argb=0xFFFFFFFF;
    std::vector<CTexture*> m_texture;
    std::array<EffectSegmentState,50> m_segments;
};
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
#include "ParticleRenderBodies.inc"
float g_cosTable[361],g_sinTable[361];
vector3d AddVec3(const vector3d& a,const vector3d& b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
#include "CircleRenderBodies.inc"
#include "CastingColorBodies.inc"
#include "CastingLowRiseBody.inc"
#include "ReferenceTriangleQueueBodies.inc"

int main() {

    for(int i=0;i<=360;++i){g_cosTable[i]=GetCos(i);g_sinTable[i]=GetSin(i);}
    int checks=0,failures=0;
    auto check=[&](bool ok,const char* message){++checks;if(!ok){++failures;std::printf("FAIL: %s\n",message);}};
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
    for(const auto& item:std::array<std::pair<int,float>,4>{{{0,100.0f},{45,10.0f},{90,5.0f},{180,3.0f}}}) {
        double radialExtended=0;float radialStored=0,vertical=0;
        CastingLowPolygonRise(item.first,item.second,&radialExtended,&radialStored,&vertical);
        check(near(radialExtended,static_cast<double>(GetSin(item.first*4))*item.second)
            && near(radialStored,static_cast<float>(radialExtended))
            && near(vertical,-static_cast<double>(GetCos(item.first*4))*item.second),
            "low polygon casting uses source four-times-angle sine radial and cosine vertical tables");
    }
    const int inverseSizes[]={0,2,3,12,13,14,100};
    for(int size=0;size<=100;++size) {
        bool inverse=false;for(int value:inverseSizes)inverse|=size==value;
        check(CastingSizeBlend(static_cast<float>(size))==(inverse?D3DBLEND_INVSRCALPHA:D3DBLEND_ONE),
            "casting destination blend matches original RenderTeiRect case table");
    }

    auto near=[](float a,float b){return std::fabs(a-b)<0.0001f;};
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
    std::printf("%d particle render checks passed; no media loaded or rendered.\n",checks-failures);
    return failures?1:0;
}
