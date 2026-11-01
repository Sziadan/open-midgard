#include <algorithm>
#include <cassert>
#include <iostream>
#include <utility>
#include <vector>
using D3DBLEND=int;
constexpr int D3DBLEND_ONE=2,D3DBLEND_SRCALPHA=5,D3DBLEND_INVSRCALPHA=6;
constexpr int D3DRENDERSTATE_SRCBLEND=0,D3DRENDERSTATE_DESTBLEND=1,
    D3DRENDERSTATE_FOGENABLE=2,D3DRENDERSTATE_COLORKEYENABLE=3,
    D3DRENDERSTATE_ZENABLE=4,D3DRENDERSTATE_ZWRITEENABLE=5,D3DRENDERSTATE_ZFUNC=6;
constexpr int FALSE=0,TRUE=1,D3DZB_TRUE=1,D3DCMP_LESSEQUAL=4;
struct Vertex { float oow=0.002f; };
struct Device {
    int src=D3DBLEND_ONE,dest=D3DBLEND_ONE;
    std::vector<std::pair<int,int>> draws;
    void SetRenderState(int state,int value) {
        if(state==D3DRENDERSTATE_SRCBLEND)src=value;
        if(state==D3DRENDERSTATE_DESTBLEND)dest=value;
    }
};
struct RPFace {
    int mtPreset=0,srcAlphaMode=D3DBLEND_SRCALPHA,destAlphaMode=D3DBLEND_ONE;
    float alphaSortKey=0;Vertex* verts=nullptr;int numVerts=0;void* tex=nullptr;
    static void DrawPri(RPFace*,Device& device){device.draws.push_back({device.src,device.dest});}
};
struct Renderer {
    Device* m_renderDevice;
    std::vector<RPFace*> m_rpAlphaOPList;
    void SetMultiTextureMode(int){}
    void SetTexture(void*){}
    void Pass(D3DBLEND activeSrcBlend,D3DBLEND activeDestBlend,int activeBlendedMtPreset=0) {
#include "EffectBlendPass.inc"
    }
};
int main()
{
    Device device;RPFace heal;Renderer renderer{&device,{&heal}};
    // An emissive scene pass ends in ONE/ONE. The restore must update the
    // cache too, or the first Heal face incorrectly keeps destination INV.
    renderer.Pass(D3DBLEND_ONE,D3DBLEND_ONE);
    assert(device.draws.size()==1);
    assert(device.draws[0]==std::make_pair(D3DBLEND_SRCALPHA,D3DBLEND_ONE));
    device.draws.clear();RPFace normal;normal.destAlphaMode=D3DBLEND_INVSRCALPHA;
    renderer.m_rpAlphaOPList={&normal,&heal};
    renderer.Pass(D3DBLEND_ONE,D3DBLEND_ONE);
    assert(device.draws[0]==std::make_pair(D3DBLEND_SRCALPHA,D3DBLEND_INVSRCALPHA));
    assert(device.draws[1]==std::make_pair(D3DBLEND_SRCALPHA,D3DBLEND_ONE));
    std::cout<<"Production effect-pass blend-state checks passed\n";
}
