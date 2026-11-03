"""Replay wide vital status updates through production branches and bootstrap assignments."""
import os
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
packet = (root / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
mode = (root / "src/gamemode/GameMode.cpp").read_text(encoding="utf-8")
session = (root / "src/session/Session.cpp").read_text(encoding="utf-8")
def vital_cases(function):
    start = packet.index("bool " + function + "(")
    # ApplySelfStatusUpdate has a forward declaration as well.
    if function == "ApplySelfStatusUpdate":
        start = packet.index("bool " + function + "(", start + 1)
    start = packet.index("    case kStatusHp:", start)
    return packet[start:packet.index("    case kStatusBaseLevel:", start)]
bootstrap = "\n".join(re.findall(r"        actor->m_(?:Hp|MaxHp|Sp|MaxSp) = g_session.GetCharacterVitals\(\)\.[^;]+;", mode))
assert len(bootstrap.splitlines()) == 4
seed = re.search(r"m_characterVitals = \{.*?\};", session, re.S).group()
build = root / "build-mingw-preset/vitals-check"
build.mkdir(parents=True, exist_ok=True)
cpp = build / "check.cpp"
cpp.write_text(r'''
#include "core/Vitals.h"
#include "world/GameActor.h"
#include "ui/UIBasicInfoWnd.h"
#include "ui/UIPcGage.h"
#include <iostream>
#include <stdexcept>
#include <type_traits>
static_assert(sizeof(CHARACTER_INFO)==108, "wire character list changed");
static_assert(std::is_same<decltype(CGameActor::m_Hp),u32>::value);
static_assert(std::is_same<decltype(CGameActor::m_MaxHp),u32>::value);
static_assert(std::is_same<decltype(UIBasicInfoWnd::DisplayData::hp),long long>::value);
static_assert(std::is_same<decltype(std::declval<UIPcGage>().GetHp()),long long>::value);
constexpr u32 kStatusHp=5,kStatusMaxHp=6,kStatusSp=7,kStatusMaxSp=8;
struct TestSession {
    CharacterVitals m_characterVitals;
    bool UpdateCharacterVital(u32 type,u32 value) { return m_characterVitals.Update(type,value); }
    const CharacterVitals& GetCharacterVitals() const { return m_characterVitals; }
    void Select(const CHARACTER_INFO& info) {
''' + seed + r'''
    }
} g_session;
CHARACTER_INFO legacy{};
struct Player { u32 m_Hp=0,m_MaxHp=0,m_Sp=0,m_MaxSp=0; } player;
bool Store(u32 statusType,u32 value) {
    g_session.UpdateCharacterVital(statusType,value);
    auto* info=&legacy;
    switch(statusType) {
''' + vital_cases("ApplySelfStatusUpdateToSession") + r'''
    default:return false;
    }
}
bool UpdateActor(u32 statusType,u32 value) {
    auto* player=&::player;
    switch(statusType) {
''' + vital_cases("ApplySelfStatusUpdate") + r'''
    default:return false;
    }
}
void Bootstrap() { auto* actor=&player;
''' + bootstrap + r'''
}
void check(bool ok) { if(!ok) throw std::runtime_error("high vital regression"); }
int main() {
    legacy.hp=static_cast<s16>(33414); legacy.maxhp=static_cast<s16>(33414);
    g_session.Select(legacy); Bootstrap(); check(player.m_Hp==33414 && player.m_MaxHp==33414);
    for (u32 value : {0u,32767u,32768u,33414u,65535u,65536u,1000000u,2147483647u,2147483648u,4294967295u}) {
        Store(5,value); UpdateActor(5,value); Store(6,value); UpdateActor(6,value);
        Store(7,value); UpdateActor(7,value); Store(8,value); UpdateActor(8,value);
        for(int tick=0;tick<100;++tick) Bootstrap();
        check(player.m_Hp==value && player.m_MaxHp==value && player.m_Sp==value && player.m_MaxSp==value);
        check(legacy.hp>=0 && legacy.maxhp>=0 && legacy.sp>=0 && legacy.maxsp>=0);
        check(VitalBarFill(value,value,114)==(value ? 114 : 0));
        check(VitalBarFill(value,value,100)==(value ? 100 : 0));
    }
    Store(6,9579);Store(5,33414);Bootstrap();check(player.m_Hp==33414);
    Store(6,33414);Bootstrap();check(player.m_Hp==33414 && player.m_MaxHp==33414);
    check(VitalBarFill(2147483648ll,4294967295ll,100)==50);
    check(VitalBarFill(-1,4294967295ll,100)==0);
    check(VitalBarFill(4294967295ll,1,114)==114);
    check(AddVital(4294967290u,100)==4294967295u);
    check(AddVital(65535,1)==65536);
    std::cout << "PASS: legacy wire size, signed-16 boundary, full unsigned-32 HP/SP, repeated bootstrap, packet order, bars, recovery saturation\n";
}
''')
compiler = Path("C:/Qt/Tools/mingw1310_64/bin/g++.exe")
exe = build / "check.exe"
subprocess.run([str(compiler), "-std=c++17", "-O3", "-DNOMINMAX", "-DRO_PLATFORM_WINDOWS=1",
                str(cpp), "-I", str(root / "src"), "-o", str(exe)], check=True)
env = os.environ.copy()
env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
subprocess.run([str(exe)], check=True, env=env)
