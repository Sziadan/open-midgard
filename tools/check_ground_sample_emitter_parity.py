#!/usr/bin/env python3
"""Compare the source and port for the GroundSample effect emitter."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
REF = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF_DISPATCH = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed body: {marker}")


def check(name: str, source_patterns: list[str], impl_patterns: list[str], source: str, impl: str) -> None:
    for pattern in source_patterns:
        assert re.search(pattern, source, re.S), f"source mismatch in {name}: {pattern}"
    for pattern in impl_patterns:
        assert re.search(pattern, impl, re.S), f"implementation mismatch in {name}: {pattern}"
    print(f"PASS {name}")


source = body(REF, "void __thiscall CRagEffect::GROUNDSAMPLE(CRagEffect *this)\n{")
exe = body(EXE, "void __thiscall CRagEffect::GROUNDSAMPLE(CRagEffect *this)\n{")
source_dispatch = EXE
impl_dispatch = body(IMPL, "bool CRagEffect::ProcessEffectFrame()")
impl = body(IMPL, "void CRagEffect::ProcessGroundSample()")

assert re.sub(r"\s+", " ", source) == re.sub(r"\s+", " ", exe), "Ref and executable GROUNDSAMPLE bodies differ"
print("PASS Ref/executable emitter body equality")

check("canonical effect type 0x201 dispatch", [r"case 0x201:\s*CRagEffect::GROUNDSAMPLE\(this\)"],
      [r"m_type == 0x201\)\s*\{\s*ProcessGroundSample\(\);\s*return true;"], source_dispatch, impl_dispatch)

check("position copy precedes first-state-only setup", [
          r"this->m_pos = this->m_master->m_pos;\s*if \( !this->m_stateCnt \)",
          r"if \( !this->m_stateCnt \)[\s\S]*?memset\(&v3, 0, sizeof\(v3\)\);",
      ], [
          r"m_effectPos = m_master->m_pos;\s*if \(m_stateCnt != 0\) return;",
          r"LaunchEffectPrim\(PP_GROUNDSAMPLE, vector3d\{\}\)",
      ], source, impl)

check("primitive type, zero delta, render flag and duration", [
          r"LaunchEffectPrim\(this, PP_GROUNDSAMPLE, v3\);\s*g_prim->m_renderFlag = 5;\s*g_prim->m_duration = this->m_duration;",
      ], [
          r"LaunchEffectPrim\(PP_GROUNDSAMPLE, vector3d\{\}\);\s*prim->m_renderFlag = 5;\s*prim->m_duration = m_duration;",
      ], source, impl)

check("texture storage and exact magic_target resource", [
          r"m_texture = \(CTexture \*\*\)operator new\(4 \* g_prim->m_totalTexture\);\s*\*g_prim->m_texture = CTexMgr::GetTexture\(&g_texMgr, aEffectMagicTar, 0\);",
      ], [
          r"m_texture\.resize\(static_cast<size_t>\(prim->m_totalTexture\), nullptr\);\s*prim->m_texture\[0\] = g_texMgr\.GetTexture\(\"effect\\\\magic_target\.tga\", false\);",
      ], source, impl)
assert "char aEffectMagicTar[] = \"effect\\\\magic_target.tga\";" in EXE
assert re.search(r"texture\[0\].*GetTexture", impl, re.S)
print("PASS executable asset literal resolves to effect\\magic_target.tga")

check("four source life-byte resets precede activation", [
          r"for \( i = 0; i < 720; \*\(\(_BYTE \*\)&g_prim->m_latiSpeed \+ i\) = 0 \)\s*i \+= 180;\s*g_prim->m_GI\[0\]\.life = 1;\s*g_prim->m_GI\[0\]\.RotStart = 0;",
      ], [
          r"for \(int band = 0; band < 4; \+\+band\)\s*\{\s*prim->m_gi\[static_cast<size_t>\(band\)\]\.life = 0;\s*\}\s*prim->m_gi\[0\]\.life = 1;\s*prim->m_gi\[0\]\.rotStart = 0;",
      ], source, impl)
assert len(re.findall(r"i \+= 180", source)) == 1
assert re.search(r"m_latiSpeed \+ i\) = 0", source)
assert len(re.findall(r"prim->m_gi\[static_cast<size_t>\(band\)\]\.life = 0", impl)) == 1
print("PASS reset offsets 0/180/360/540 correspond to four 180-byte GI life slots")

print("6 mechanical GroundSample emitter comparisons passed")
