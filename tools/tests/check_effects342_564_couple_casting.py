from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402


def body(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(f"unterminated function: {signature}")


exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
primary = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
secondary = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
onprocess2 = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")
onprocess3 = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")

pe_process2 = body(exe, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
pe_process3 = body(exe, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
ref_process2 = body(primary, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
ref_process3 = body(primary, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
for effect_id, pe, ref, route in (
    (342, pe_process2, ref_process2, onprocess2),
    (564, pe_process3, ref_process3, onprocess3),
):
    token = "case 0x156:" if effect_id == 342 else "case 0x234:"
    assert token in pe and token in ref, f"effect {effect_id} PE/Ref caller missing"
    assert "CoupleCasting" in pe[pe.index(token):pe.index(token) + 160]
    assert "CoupleCasting" in ref[ref.index(token):ref.index(token) + 160]
    assert f'case {effect_id}: ApplyCoupleCasting("effect\\\\ring_red.tga"); return true;' in route

pe_helper = body(exe, "void __thiscall CRagEffect::CoupleCasting(CRagEffect *this, char *tName)")
ref_helper = body(secondary, "void\tCRagEffect :: CoupleCasting(char *tName)")
for helper, label in ((pe_helper, "PE"), (ref_helper, "secondary Ref")):
    assert "m_stateCnt" in helper and "m_pos =" in helper and "m_master->m_pos" in helper, f"{label} master/state flow missing"
    assert "SAINTCASTING" in helper and "45" in helper and "25" in helper and "3" in helper
assert "aEffectEfBegins" in pe_helper
assert "effect\\\\EF_BeginSpell.wav" in ref_helper
resources = ExeImage()
wave = resources.read_cstring_at(0x7193CC)
texture = resources.read_cstring_at(0x719454)
assert wave is not None and wave.raw == b"effect\\EF_BeginSpell.wav"
assert texture is not None and texture.raw == b"effect\\ring_red.tga"

local_helper = body(local, "void CRagEffect::ApplyCoupleCasting(const char* textureName)")
for token in ("TryPlayEffectWaveAt(wavePos,", "ApplySaintCasting(45, textureName, 3)",
              "ApplySaintCasting(25, textureName, 3)"):
    assert token in local_helper, f"local CoupleCasting call order missing {token}"
assert "case 342:" in onprocess2 and "case 564:" in onprocess3
print("effects342/564 CoupleCasting caller, helper, resource, route and order checks passed")
