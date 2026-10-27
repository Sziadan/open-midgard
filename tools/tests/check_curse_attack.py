import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ref = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="cp1252")
ref_h = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="cp1252")
exe = (ROOT / "HighPriest.exe.c").read_text(encoding="cp1252")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")


def function(text, marker):
    start = text.index(marker)
    brace = text.index("{", start)
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{": depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise AssertionError(f"unterminated {marker}")


def ordered(body, steps, label):
    cursor = 0
    for step in steps:
        try:
            cursor = body.index(step, cursor) + len(step)
        except ValueError:
            raise AssertionError(f"{label} missing/out of order: {step}")


enum = ref_h[ref_h.index("EF_CHANGEDARK"):ref_h.index("EF_PONG")]
enum = re.sub(r"/\*.*?\*/", "", enum, flags=re.S)
ids = re.findall(r"\bEF_[A-Z0-9_]+\b", enum)
assert ids[196 - 172] == "EF_CURSEATTACK", (196 - 172, ids[196 - 172])
assert "EF_CURSEATTACK" in ref_h

ref_body = function(ref, "void\tCRagEffect :: CurseAttack()")
exe_inline_start = exe.index("          case 196:", exe.index("CRagEffect::VenomDust2(this)"))
exe_inline = exe[exe_inline_start:exe.index("          case 200:", exe_inline_start)]
exe_body = function(exe[exe.index("void __thiscall CRagEffect::CurseAttack(CRagEffect *this)\n{"):],
                    "void __thiscall CRagEffect::CurseAttack(CRagEffect *this)")
impl = function(src, "void CRagEffect::ApplyCurseAttack()")

ordered(ref_body, [
    "if (m_stateCnt == 0)", "LaunchEffectPrim(PP_3DTEXTURE)", "m_duration = 9999",
    "m_longitude = 0", "m_longSpeed = 4.5f", "m_deltaPos2.y -= 30.0f",
    "m_widthSize = 4.0f", "m_heightSize = 4.0f", 'GetTexture("effect\\\\curse.bmp")',
], "Ref CurseAttack")
for label, body in (("EXE inline", exe_inline), ("EXE function", exe_body)):
    ordered(body, [
        "m_stateCnt", "PP_3DTEXTURE", "m_duration = 9999", "m_longitude = 0.0",
        "m_longSpeed = 4.5", "m_deltaPos2.y = g_prim->m_deltaPos2.y - 30.0",
        "m_widthSize = 4.0", "m_heightSize = 4.0", "aEffectCurseBmp",
    ], label)
ordered(impl, [
    "if (m_stateCnt != 0) return", "LaunchEffectPrim(PP_3DTEXTURE, vector3d{})",
    "prim->m_duration = 9999", "prim->m_longitude = 0.0f", "prim->m_longSpeed = 4.5f",
    "prim->m_deltaPos2.y -= 30.0f", "prim->m_widthSize = 4.0f", "prim->m_heightSize = 4.0f",
    'GetTexture("effect\\\\curse.bmp", false)',
], "implementation")
assert "case 196:" in function(src, "void CRagEffect::Init(CRenderObject* master")
assert "if (m_type == 196)" in function(src, "bool CRagEffect::ProcessEffectFrame()")
assert 'char aEffectCurseBmp[] = "effect\\\\curse.bmp"' in exe
assert "rand()" not in ref_body and "PlayWave" not in ref_body and "SendMsg" not in ref_body
print("PASS: CurseAttack Ref/EXE/current zero-state payload, texture, ID196 and Init/ProcessEffectFrame routes match")

