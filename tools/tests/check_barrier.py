from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ref_text = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="cp1252")
exe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="cp1252")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")

def function(text, marker):
    start = text.index(marker)
    brace = text.index("{", start)
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{": depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0: return text[start:i + 1]
    raise AssertionError(f"unterminated {marker}")

ref = function(ref_text, "void CRagEffect :: Barrier()")
exe = function(exe_text[exe_text.index("void __thiscall CRagEffect::Barrier(CRagEffect *this)\n{"):],
               "void __thiscall CRagEffect::Barrier(CRagEffect *this)")
impl = function(src, "void CRagEffect::ApplyBarrier()")

for label, body in (("Ref", ref), ("EXE", exe)):
    for token in ("m_stateCnt", "PP_3DSPHERE", "m_duration", "m_radius", "m_deltaPos2.y",
                  "m_longitude", "m_alpha", "m_texture", "GetTexture"):
        assert token in body, f"{label} Barrier missing {token}"
    assert "random(" not in body and "rand()" not in body, f"{label} Barrier unexpectedly consumes RNG"
assert 'GetTexture("effect\\\\bigbang.tga")' in ref
assert "aEffect_27" in exe

steps = [
    "if (m_stateCnt != 0) return", "LaunchEffectPrim(PP_3DSPHERE, vector3d{})",
    "prim->m_duration = 250", "prim->m_radius = 13.5f", "prim->m_deltaPos2.y = -10.0f",
    "prim->m_longitude = 2.0f", "prim->m_alpha = 100.0f",
    'prim->m_texture.push_back(g_texMgr.GetTexture("effect\\\\bigbang.tga", false))',
]
cursor = 0
for step in steps:
    cursor = impl.index(step, cursor) + len(step)
assert "if (m_type == 63)" in function(src, "bool CRagEffect::ProcessEffectFrame()")
assert "case 63:" in function(src, "void CRagEffect::Init(")
print("PASS: Ref, EXE and current Barrier state gate/payload/route match; records Ref-vs-EXE texture-string mismatch")
