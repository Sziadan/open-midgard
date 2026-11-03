"""Exercise the production map-particle updater across map-loading view gaps."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
source = (root / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
start = source.index("bool UpdateReferenceMapParticle(CEffectPrim* prim)")
end = source.index("// Ref/dhxj/RagEffect2.cpp:9320", start)
body = source[start:end]
build = root / "build-mingw-preset/map-particle-check"
build.mkdir(parents=True, exist_ok=True)
cpp = build / "check.cpp"
cpp.write_text(r'''
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
struct Vec { float x=0,y=0,z=0; };
struct Master { Vec EffectPosition() const { return {11,22,33}; } } master;
struct CView {
    float longitude=15, latitude=45;
    float GetCameraLongitude() { return longitude; }
    float GetCameraLatitude() { return latitude; }
} view;
struct CGameMode { CView* m_view = nullptr; } mode;
struct Manager { CGameMode* current = nullptr; CGameMode* GetCurrentGameMode() { return current; } } g_modeMgr;
struct EffectGi {
    short alphaT=0; int life=0; float height[8]{};
    float vecBPreX=0,vecBPreZ=0,sparkX=0,sparkZ=0,vecTPreX=0,vecTPreZ=0,vecTNowX=0,vecTNowZ=0;
    float arcBaselineY=0,sparkY=0,vecTPreY=0,vecTNowY=0;
};
struct CEffectPrim { EffectGi m_gi[4]; Vec m_pos; Master* m_master = &master; };
float GetSin(int angle) { return std::sin(angle * 0.01745329252f); }
float GetCos(int angle) { return std::cos(angle * 0.01745329252f); }
void check(bool ok) { if (!ok) throw std::runtime_error("map-particle loading regression"); }
''' + body + r'''
int main() {
    CEffectPrim prim;
    prim.m_gi[0].life=1;
    prim.m_gi[0].height[0]=10; prim.m_gi[0].height[1]=20;
    prim.m_gi[0].height[2]=30; prim.m_gi[0].height[3]=20;
    check(UpdateReferenceMapParticle(&prim)); // no active game mode
    check(prim.m_pos.x==0 && prim.m_gi[0].height[0]==10);
    g_modeMgr.current=&mode;
    for (int i=0; i<100; ++i) check(UpdateReferenceMapParticle(&prim)); // loading before view
    check(prim.m_pos.x==0 && prim.m_gi[0].height[0]==10 && prim.m_gi[0].alphaT==0);
    mode.m_view=&view;
    check(UpdateReferenceMapParticle(&prim)); // normal processing resumes
    check(prim.m_pos.x==11 && prim.m_pos.y==22 && prim.m_pos.z==33);
    check(prim.m_gi[0].alphaT==345 && prim.m_gi[1].alphaT==45);
    check(prim.m_gi[0].height[0]==11 && prim.m_gi[0].height[2]==29);
    mode.m_view=nullptr; // another map transition
    check(UpdateReferenceMapParticle(&prim) && prim.m_gi[0].height[0]==11);
    mode.m_view=&view;
    check(UpdateReferenceMapParticle(&prim) && prim.m_gi[0].height[0]==12);
    std::cout << "PASS: absent mode, loading view gap, preserved state, resumed camera/particle updates, repeated transitions\n";
}
''')
compiler = Path("C:/Qt/Tools/mingw1310_64/bin/g++.exe")
exe = build / "check.exe"
subprocess.run([str(compiler), "-std=c++17", "-O3", str(cpp), "-o", str(exe)], check=True)
env = os.environ.copy()
env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
subprocess.run([str(exe)], check=True, env=env)
