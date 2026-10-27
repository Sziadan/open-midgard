from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
ref_h = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="cp949", errors="replace")
ref = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="cp949", errors="replace")
ref2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="cp949", errors="replace")
exe = (ROOT / "HighPriest.exe.c").read_text(encoding="cp949", errors="replace")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")


def require(ok, message):
    if not ok:
        raise AssertionError(message)


def between(text, start, end):
    a = text.index(start)
    b = text.index(end, a + len(start))
    return text[a:b]


# Resolve ids from the commented EF_COOKING_FAIL=609 anchor and ordered tokens.
enum_tail = between(ref_h, "EF_COOKING_FAIL", "EF_MAP_MAGICZONE")
tokens = re.findall(r"\bEF_[A-Z0-9_]+\b", enum_tail)
base = 609
enum_ids = {name: base + i for i, name in enumerate(tokens)}
require(enum_ids.get("EF_RG_COIN3") == 627, f"enum derivation: EF_RG_COIN3={enum_ids.get('EF_RG_COIN3')}")
print(f"enum ordered from anchor EF_COOKING_FAIL=609: EF_RG_COIN3={enum_ids['EF_RG_COIN3']}")

read_caller = between(ref, "case\tEF_RG_COIN3:", "case\tEF_STIN5:")
exe_caller = between(exe, "case 0x273:", "case 0x274:")
src_caller = between(src, "void CRagEffect::ApplyRgCoin3()", "void CRagEffect::SpawnRgCoin2(")
route = re.search(r"if\s*\(m_type\s*==\s*(\d+)\)\s*\{\s*ApplyRgCoin3\(\);\s*return true;", src)
require(route is not None and int(route.group(1)) == enum_ids["EF_RG_COIN3"], "current ProcessEffectFrame route id/call")

paths = ["UserInterface\\item\\gold_lux.bmp", "UserInterface\\item\\deogaeriseun.bmp",
         "UserInterface\\item\\double_shotgun.bmp", "UserInterface\\item\\deoseukeu.bmp"]
read_calls = [p.replace("\\\\", "\\") for p in re.findall(r'RG_COIN2\("([^"]+)",\s*5\.0f?\)', read_caller)]
src_calls = [p.replace("\\\\", "\\") for p in re.findall(r'SpawnRgCoin2\("([^"]+)",\s*5\.0f\)', src_caller)]
canonical_symbols = re.findall(r"RG_COIN2\(this,\s*&?(byte_719[A-F0-9]+),\s*5\.0\)", exe_caller)
require(read_calls == paths, f"readable caller texture/call order: {read_calls}")
require(src_calls == paths, f"current caller texture/call order: {src_calls}")
require(canonical_symbols == ["byte_719AF4", "byte_719AD0", "byte_719AAC", "byte_719A8C"],
        f"canonical caller symbol order: {canonical_symbols}")
require("m_stateCnt == 21" in read_caller and "m_stateCnt == 21" in src_caller and "m_stateCnt == 21" in exe_caller,
        "state-21 sound guard present in all callers")
require('PlayWave("effect\\\\disarm.wav"' in src_caller and "aEffect_38" in exe_caller,
        "exact source wave and canonical wave symbol")
require(re.search(r"m_pos\s*=\s*m_master->m_pos", read_caller) and re.search(r"this->m_pos\s*=\s*this->m_master->m_pos", exe_caller),
        "state-21 master refresh in both reference bodies")
print("caller comparison: enum 627/case 0x273, state-21 refresh/audio, four ordered texture calls matched")

read_emit = between(ref2, "void\tCRagEffect :: RG_COIN2(", "void\tCRagEffect :: TEIHIT1(")
exe_emit = between(exe, "void __thiscall CRagEffect::RG_COIN2(CRagEffect *this, char *tName, float size)\n{", "//----- (0062E390)")
src_emit = between(src, "void CRagEffect::SpawnRgCoin2(", "void CRagEffect::ApplyBottomMagnus(")
for label, body in (("readable", read_emit), ("canonical", exe_emit), ("current", src_emit)):
    require(re.search(r"stateCnt\s*==\s*0|!this->m_stateCnt|m_stateCnt\s*!=\s*0", body), f"{label} state-zero guard")

# Source-to-source ordered payload checks.  Canonical decompiler names several
# structure members through byte offsets, so compare its RHS/order at each store.
require(read_emit.index("m_pos = m_master->m_pos") < read_emit.index("LaunchEffectPrim") < read_emit.index("m_pos.y-=12.0f") < read_emit.index("for (ec=0;ec<4;ec++)"), "readable emitter branch order")
require(exe_emit.index("this->m_pos = this->m_master->m_pos") < exe_emit.index("LaunchEffectPrim") < exe_emit.index("this->m_pos.y = this->m_pos.y - 12.0") < exe_emit.index("do\n    {") < exe_emit.index("while ( v4 < 4 )"), "canonical emitter branch order")
require(src_emit.index("m_effectPos = master->m_pos") < src_emit.index("LaunchEffectPrim") < src_emit.index("m_effectPos.y -= 12.0f") < src_emit.index("for (int lane = 0; lane < 4; ++lane)"), "current emitter branch order")

read_fields = ["life=1", "process=-random(30)-18", "distance=12.0f", "height[0]=1.0f",
               "rise_angle=random(360)", "full_display_angle=random(360)", "RotStart=random(360)",
               "height[2]=random(360)", "height[1]=size", "alphaB=0", "flag1[0]=2"]
src_fields = ["gi.life = 1", "gi.process = -18 - rand() % 30", "gi.distance = 12.0f", "gi.height[0] = 1.0f",
              "gi.riseAngle = static_cast<short>(rand() % 360)", "gi.fullDisplayAngle = static_cast<short>(rand() % 360)",
              "gi.rotStart = static_cast<short>(rand() % 360)", "gi.height[2] = static_cast<float>(rand() % 360)",
              "gi.height[1] = size", "gi.alphaB = 0", "gi.flag[0] = 2"]
require(all(x in read_emit for x in read_fields), "readable emitter complete GI field set")
require(all(x in src_emit for x in src_fields), "current emitter complete GI field set")
read_rng = len(re.findall(r"random\((?:30|360)\)", read_emit))
exe_rng = len(re.findall(r"rand\(\)\s*%\s*(?:30|360)", exe_emit))
src_rng = len(re.findall(r"rand\(\)\s*%\s*(?:30|360)", src_emit))
require((read_rng, exe_rng, src_rng) == (5, 5, 5), f"per-lane RNG sites readable/canonical/current={(read_rng, exe_rng, src_rng)}")
require("ec<4" in read_emit and "v4 < 4" in exe_emit and "lane < 4" in src_emit, "four-lane loop bounds")
require("RF_EFFECT" in read_emit and "m_renderFlag = 5" in exe_emit and "m_renderFlag = 5" in src_emit, "render flag 5")
require("GetTexture(tName)" in read_emit and "GetTexture(&g_texMgr, tName, 0)" in exe_emit and "GetTexture(textureName, false)" in src_emit,
        "single source texture lookup semantics")
require("m_duration" in read_emit and "this->m_duration" in exe_emit and "m_duration" in src_emit, "parent duration propagation")
print("emitter comparison: state-zero branch, launch/shift/four-lane order, fields, five RNG sites per lane, exact resources matched")
print("RG_COIN3 source-to-implementation comparison passed")
