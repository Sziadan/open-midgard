"""Exercise production map-emitter scheduling and independent screen-effect flags."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
text = (root / "src/world/World.cpp").read_text(encoding="utf-8")
start = text.index("class CFixedWorldEffect")
constructor = text[text.index("    explicit CFixedWorldEffect", start):text.index("    ~CFixedWorldEffect", start)]
process = text[text.index("    u8 OnProcess() override", start):text.index("    bool IsPortalLike()", start)]
build = root / "build-mingw-preset/ambient-check"
build.mkdir(parents=True, exist_ok=True)
cpp = build / "check.cpp"
cpp.write_text(r'''
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <iostream>
#include "render3d/ScreenEffects.h"
using u8=unsigned char; using DWORD=unsigned int; using msgparam_t=std::intptr_t;
struct vector3d { float x=0,y=0,z=0; };
DWORD timeGetTime() { return 0; }
int g_isSkipFrame=1;
struct C3dWorldRes { struct effectSrcInfo { char name[80]{}; vector3d pos; int type=0; float emitSpeed=0; float param[4]{}; }; };
struct CRagEffect {
    int calls=0; float params[4]{};
    void SendMsg(void*, int msg, msgparam_t arg, int, int) {
        if (msg!=46) throw std::runtime_error("wrong parameter message");
        const auto* p=reinterpret_cast<const vector3d*>(arg);
        params[0]=p[0].x; params[1]=p[0].y; params[2]=p[0].z; params[3]=p[1].x;
        ++calls;
    }
    u8 OnProcess() { return 1; }
} effect;
CRagEffect* CreateWorldRagEffect(const C3dWorldRes::effectSrcInfo&) { throw std::runtime_error("numeric id entered label fallback"); }
struct Player { vector3d m_pos; } player;
struct World { Player* m_player=&player; } world;
struct CGameMode { void* m_view=reinterpret_cast<void*>(1); World* m_world=&world; } mode;
struct Manager { CGameMode* current=&mode; CGameMode* GetCurrentGameMode() { return current; } } g_modeMgr;
struct Session { bool m_isEffectOn=true; } g_session;
struct CAbleToMakeEffect {
    vector3d m_pos; std::vector<int> emitted; int detaches=0;
    virtual u8 OnProcess() { return 1; }
    CRagEffect* LaunchEffect(int id, vector3d, float) { emitted.push_back(id); return &effect; }
    void DetachEffects() { ++detaches; }
};
class CFixedWorldEffect : public CAbleToMakeEffect {
public:
''' + constructor + process + r'''
    C3dWorldRes::effectSrcInfo m_source;
    DWORD m_spawnTick; CRagEffect* m_effect; bool m_useEmitter;
    int m_emitFrames,m_processCnt; bool m_emitted;
};
void check(bool ok) { if (!ok) throw std::runtime_error("ambient regression"); }
int main() {
    for (int id : {109,45,44,47,162,163,231,247,333,665}) {
        C3dWorldRes::effectSrcInfo source{}; source.type=id; source.emitSpeed=200;
        source.param[0]=1;source.param[1]=2;source.param[2]=3;source.param[3]=4;
        CFixedWorldEffect emitter(source);
        mode.m_view=nullptr;
        for (int i=0;i<30;++i) emitter.OnProcess();
        check(emitter.emitted.empty());
        mode.m_view=reinterpret_cast<void*>(1);
        g_isSkipFrame=0; for (int i=0;i<30;++i) emitter.OnProcess();
        check(emitter.emitted.empty()); g_isSkipFrame=1;
        for (int i=0;i<25 && emitter.emitted.empty();++i) emitter.OnProcess();
        check(emitter.emitted.size()==1 && emitter.emitted[0]==id);
        for (int i=0;i<4;++i) check(effect.params[i]==i+1);
        for (int i=0;i<199;++i) emitter.OnProcess();
        check(emitter.emitted.size()==1); emitter.OnProcess(); check(emitter.emitted.size()==2);
        player.m_pos.x=131; emitter.OnProcess(); check(emitter.detaches==1);
        player.m_pos.x=0; emitter.OnProcess(); check(emitter.emitted.size()==3);
    }
    C3dWorldRes::effectSrcInfo source{}; source.type=109; source.emitSpeed=0;
    CFixedWorldEffect once(source);
    for (int i=0;i<1000;++i) once.OnProcess();
    check(once.emitted.size()==1);
    g_session.m_isEffectOn=false; once.OnProcess(); check(once.detaches==1);
    g_session.m_isEffectOn=true; once.OnProcess(); check(once.emitted.size()==2);
    check(ResolveScreenEffectFlags(false,true,true,8)==0);
    check(ResolveScreenEffectFlags(false,false,true,8)==0);
    check(ResolveScreenEffectFlags(true,false,true,8)==1);
    check(ResolveScreenEffectFlags(true,true,true,8)==1);
    check(ResolveScreenEffectFlags(false,true,true,16)==4);
    check(ResolveScreenEffectFlags(false,true,false,8)==0);
    std::cout << "PASS: numeric ambient handlers, parameters, intervals, one-shot effects, range reentry, loading/frame gates, effect toggle, independent confusion/SSAO flags\n";
}
''')
compiler = Path("C:/Qt/Tools/mingw1310_64/bin/g++.exe")
exe = build / "check.exe"
subprocess.run([str(compiler), "-std=c++17", "-O3", str(cpp), "-I", str(root / "src"), "-o", str(exe)], check=True)
env = os.environ.copy()
env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
subprocess.run([str(exe)], check=True, env=env)
