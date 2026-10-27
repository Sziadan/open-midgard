from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF_H = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="cp949", errors="replace")
REF = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="cp949", errors="replace")
REF2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="cp949", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="cp949", errors="replace")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")


def need(value, label):
    if not value:
        raise AssertionError(label)


def slice_between(text, begin, end):
    first = text.index(begin)
    last = text.index(end, first + len(begin))
    return text[first:last]


enum_tokens = re.findall(r"\bEF_[A-Z0-9_]+\b", slice_between(REF_H, "EF_COOKING_FAIL", "EF_MAP_MAGICZONE"))
enum_id = {name: 609 + offset for offset, name in enumerate(enum_tokens)}
need(enum_id.get("EF_BASH3D5") == 628, f"enum resolves EF_BASH3D5 to {enum_id.get('EF_BASH3D5')}")
ref_case = slice_between(REF, "case\tEF_BASH3D5:", "case\tEF_RG_COIN3:")
exe_case = slice_between(EXE, "case 0x274:", "case 0x275:")
impl_case = slice_between(IMPL, "void CRagEffect::ApplyBash3D5()", "void CRagEffect::ApplyBottomMagnus(")
route = re.search(r"if\s*\(m_type\s*==\s*(\d+)\)\s*\{\s*ApplyBash3D5\(\);\s*return true;", IMPL)
need(route and int(route.group(1)) == enum_id["EF_BASH3D5"], "current frame dispatcher route id and method")

need("m_stateCnt == 0" in ref_case and "this->m_stateCnt" in exe_case and "m_stateCnt == 0" in impl_case,
     "state-zero audio gate present across all callers")
need(ref_case.index("m_pos = m_master->m_pos") < ref_case.index('PlayWave("effect\\\\bash3d5.wav"') < ref_case.index("for (int i=0;i<6;i++)"),
     "readable refresh/audio/six-call order")
need(exe_case.index("this->m_pos = this->m_master->m_pos") < exe_case.index("PlayWave(aEffect_56") < exe_case.index("for ( j = 0; j < 6; ++j )"),
     "canonical refresh/audio/six-call order")
need(impl_case.index("m_effectPos = master->m_pos") < impl_case.index('PlayWave("effect\\\\bash3d5.wav"') < impl_case.index("for (int lane = 0; lane < 6; ++lane)"),
     "current refresh/audio/six-call order")

ref_calls = re.findall(r'BASH3D\("([^"]+)",\s*i,\s*5\)', ref_case)
impl_calls = re.findall(r'SpawnBash3D\("([^"]+)",\s*static_cast<char>\(lane\),\s*5\)', impl_case)
need(ref_calls == ["effect\\\\alpha_center.tga"], f"readable shared helper call and texture: {ref_calls}")
need(impl_calls == ["effect\\\\alpha_center.tga"], f"current shared helper call and texture: {impl_calls}")
need("CRagEffect::BASH3D(this, aEffectAlphaCen, j, 5)" in exe_case,
     "canonical calls the shared emitter with lane index and mode5")
need(re.search(r"for\s*\(int i=0;i<6;i\+\+\)", ref_case) and re.search(r"for \( j = 0; j < 6; \+\+j \)", exe_case)
     and re.search(r"for \(int lane = 0; lane < 6; \+\+lane\)", impl_case), "six-lane caller loop bounds")

ref_emitter = slice_between(REF2, "void\tCRagEffect :: BASH3D(char *tName,char F1,char F2)", "void\tCRagEffect :: FirstAid()")
exe_emitter = slice_between(EXE, "void __thiscall CRagEffect::BASH3D(CRagEffect *this, char *tName, char F1, char F2)\n{", "//----- (0060FC10)")
impl_emitter = slice_between(IMPL, "void CRagEffect::SpawnBash3D(", "void CRagEffect::ApplyColorPaper(")
for label, body in (("Ref", ref_emitter), ("canonical", exe_emitter), ("current", impl_emitter)):
    need("F2 == 2 || F2 == 3" in body if label == "Ref" else "F2 == 2 || F2 == 3" in body if label == "canonical" else "f2 == 2 || f2 == 3" in body,
         f"{label} BASH3D mode-dependent process branch")
    need("F2 == 3" in body if label != "current" else "f2 == 3 ? 7 : 22" in body,
         f"{label} BASH3D mode-three angle branch")
need("process=-24" in ref_emitter and "m_GI[ec].height[4]=F2" in ref_emitter and "height[0]=-12.0f" in ref_emitter,
     "readable mode5 process, mode payload and vertical offset")
need("m_GI[v7].process = -24" in exe_emitter and "m_GI[v7].height[4] = F2a" in exe_emitter and "= -12.0" in exe_emitter,
     "canonical mode5 process, mode payload and vertical offset")
need("gi.process = f2 == 2 || f2 == 3 ? 0 : -24" in impl_emitter and "gi.height[4] = static_cast<float>(f2)" in impl_emitter
     and "gi.height[0] = -12.0f" in impl_emitter, "current mode5 process, payload and vertical offset")

# The readable source has the complete audio filename. The decompiler symbol
# is known to resolve only to the directory prefix; this is recorded in MISMATCHES.
need('char aEffect_56[] = "effect\\\\";' in EXE and "PlayWave(aEffect_56" in exe_case,
     "canonical truncated audio symbol discrepancy is still visible")
print("enum extraction EF_COOKING_FAIL=609 -> EF_BASH3D5=628 / case0x274")
print("caller branch comparison passed: state-zero refresh/audio followed by six ordered BASH3D mode5 calls")
print("shared BASH3D mode5 payload comparison passed; canonical audio-symbol discrepancy documented")
