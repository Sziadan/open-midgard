"""Compile the production cloud draw path and check Ref's PW/TN separation."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
source = (root / 'src/world/RagEffect.cpp').read_text(encoding='utf-8')

def definition(marker):
    start = source.index(marker)
    brace = source.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

build = root / 'build-mingw-preset/cloud-blending-check'
build.mkdir(parents=True, exist_ok=True)
cpp = build / 'check.cpp'
cpp.write_text(r'''
#include <array>
#include <vector>
#include <deque>
#include <cassert>
#include <cmath>
#include <iostream>
using D3DBLEND=int;
constexpr int D3DBLEND_SRCALPHA=5,D3DBLEND_INVSRCALPHA=6,D3DBLEND_ONE=2;
constexpr int D3DPT_TRIANGLELIST=4,D3DCULL_NONE=1;
struct vector3d {float x=0,y=0,z=0;}; struct matrix {};
struct CTexture {}; struct CTexMgr {static CTexture s_dummy_texture;}; CTexture CTexMgr::s_dummy_texture;
struct Vertex {float x=0,y=0,z=0,tu=0,tv=0;unsigned int color=0,specular=0;};
struct RPFace {
    int primType=0,numVerts=0,numIndices=0,mtPreset=0,cullMode=0,srcAlphaMode=0,destAlphaMode=0;
    Vertex m_verts[3],*verts=nullptr;void* indices=nullptr;CTexture* tex=nullptr;float alphaSortKey=0;
};
struct Renderer {
    std::deque<RPFace> pool;
    std::vector<RPFace> submitted;
    RPFace* BorrowNullRP(){pool.emplace_back();return &pool.back();}
    void AddRP(RPFace* face,int flags){assert(flags==5);submitted.push_back(*face);}
} g_renderer;
bool ProjectPoint(const vector3d& p,const matrix&,Vertex* v){v->x=p.x;v->y=p.y;v->z=p.z;return true;}
struct EffectGi {
    int life=0;short alphaB=0,alphaT=0,fullDisplayAngle=45;
    std::array<float,10> height{};float distance=35,sparkX=0,sparkY=0,sparkZ=0;
    std::array<unsigned char,4> flag{};
};
struct CEffectPrim {std::array<EffectGi,4> m_gi;std::vector<CTexture*> m_texture;unsigned int m_renderFlag=5;};
float g_sinTable[360],g_cosTable[360];
void IntimidateTowardScreen(vector3d*,int,int){}
''' + definition('void SubmitCloudRect(') + '\n'
    + definition('struct CloudRenderStyle {') + ';\n'
    + definition('CloudRenderStyle CloudStyle(') + '\n'
    + definition('void CloudCorners(') + '\n'
    + definition('void RenderReferenceCloud(') + r'''
int main(){
    for(int i=0;i<360;++i){g_sinTable[i]=std::sin(i*.01745329252f);g_cosTable[i]=std::cos(i*.01745329252f);}
    CTexture fog,alternate;matrix view;
    // PW is the blend selector in Ref's RenderTeiRect; TN is a separate texture
    // index. Exercise every valid style, including the nested pattern-9 table.
    for(int pattern:{0,1,2,3,4,7,8,9,10,11,12,13,14,15,16,17})
    for(int variant=1;variant<=6;++variant)for(int tn=0;tn<=1;++tn){
        CEffectPrim prim;prim.m_texture={&fog,&alternate};
        prim.m_gi[0].flag[2]=pattern;prim.m_gi[0].flag[3]=variant;
        for(auto& gi:prim.m_gi){gi.life=1;gi.alphaB=169;gi.flag[0]=tn;}
        const bool alphaBlend=pattern==0||pattern==4||pattern==8||pattern==12||pattern==14
            ||pattern==15||pattern==16||pattern==17||(pattern==9&&variant<=5);
        g_renderer.submitted.clear();RenderReferenceCloud(prim,view);
        assert(g_renderer.submitted.size()==8);
        for(const auto& face:g_renderer.submitted){
            assert(face.destAlphaMode==(alphaBlend?D3DBLEND_INVSRCALPHA:D3DBLEND_ONE));
            assert(face.srcAlphaMode==D3DBLEND_SRCALPHA);
            assert(face.tex==(tn?&alternate:&fog));
            assert((face.m_verts[0].color>>24)==169);
        }
    }
    CEffectPrim prim;prim.m_texture={&fog};prim.m_gi[0].flag[2]=12;
    prim.m_gi[0].life=1;prim.m_gi[0].alphaB=169;
    for(int tn:{1,255}){
        prim.m_gi[0].flag[0]=tn;g_renderer.submitted.clear();RenderReferenceCloud(prim,view);
        assert(g_renderer.submitted.empty());
    }
    prim.m_gi[0].flag[0]=0;
    for(int alpha:{0,-1}){
        prim.m_gi[0].alphaB=alpha;g_renderer.submitted.clear();RenderReferenceCloud(prim,view);
        assert(g_renderer.submitted.empty());
    }
    std::cout<<"PASS: all shared cloud styles keep reference blend modes independent of texture indices; invalid textures and faded bands draw nothing\n";
}
''', encoding='utf-8')
compiler = Path('C:/Qt/Tools/mingw1310_64/bin/g++.exe')
exe = build / 'check.exe'
subprocess.run([str(compiler), '-std=c++17', '-O2', str(cpp), '-o', str(exe)], check=True)
env = os.environ.copy()
env['PATH'] = str(compiler.parent) + os.pathsep + env.get('PATH', '')
subprocess.run([str(exe)], env=env, check=True)

# Exercise the production updater over repeated fade/recycle cycles too.
header = (root / 'src/world/RagEffect.h').read_text(encoding='utf-8')
start = header.index('struct EffectGi {')
end = header.index('\n};', start) + 3
(build / 'WeatherCloudGi.inc').write_text(header[start:end], encoding='utf-8')
(build / 'WeatherCloudCycleBody.inc').write_text(definition('bool UpdateWeatherCloudPrimitive('), encoding='utf-8')
cycles = build / 'cycles.exe'
subprocess.run([str(compiler), '-std=c++17', '-O2', '-I' + str(build),
    str(root / 'tools/tests/WeatherCloudCycleTests.cpp'), '-o', str(cycles)], check=True)
subprocess.run([str(cycles)], env=env, check=True)
