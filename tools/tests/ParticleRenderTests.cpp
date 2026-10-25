#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>

using u32 = std::uint32_t;
struct RECT { long left, top, right, bottom; };
#include "ParticleActTypes.inc"
struct vector3d { float x=0, y=0, z=0; };
struct matrix { float m[4][4]{}; };
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
    float m_roll=0;
    std::array<EffectSegmentState,12> m_segments;
};
constexpr int PF_A8R8G8B8=4, D3DPT_TRIANGLESTRIP=5, D3DCULL_NONE=1;
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
};
struct SilentRenderer {
    int m_width=640,m_height=480,m_xoffset=320,m_yoffset=240;
    float m_hpc=700,m_vpc=700;
    RPFace face;
    std::vector<RPFace> submissions;
    std::vector<int> flags;
    RPFace* BorrowNullRP() { face={};return &face; }
    void AddRP(RPFace* value,int flag) { submissions.push_back(*value);flags.push_back(flag); }
} g_renderer;
float GetCos(int angle) { return std::cos(angle*3.14159265f/180.0f); }
float GetSin(int angle) { return std::sin(angle*3.14159265f/180.0f); }
#include "ParticleRenderBodies.inc"

int main() {
    int checks=0,failures=0;
    auto check=[&](bool ok,const char* message){++checks;if(!ok){++failures;std::printf("FAIL: %s\n",message);}};
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
    std::printf("%d particle render checks passed; no media loaded or rendered.\n",checks-failures);
    return failures?1:0;
}
