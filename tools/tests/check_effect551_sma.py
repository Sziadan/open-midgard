from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


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
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
secondary = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8", errors="replace")
routes = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
generator = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")

signature = "void __thiscall CRagEffect::SMA(CRagEffect *this, char F1)"
pe_helper = body(exe, signature)
ref_helper = body(secondary, "void\tCRagEffect :: SMA(char F1)")
assert "PP_SMA" in pe_helper and "PP_SMA" in ref_helper
assert "case 0x227:" in body(exe, "unsigned __int8 __thiscall CRagEffect::OnProcess3")
pe_case = body(exe, "unsigned __int8 __thiscall CRagEffect::OnProcess3")
primary_case = body(ref, "unsigned __int8 __thiscall CRagEffect::OnProcess3")
for label, source in (("PE", pe_case), ("primary Ref", primary_case)):
    assert "case 0x227:" in source and "SMA(this, 0)" in source, f"{label} effect551 caller mismatch"

sma = body(local, "void CRagEffect::SpawnSma(int variant)")
for token in (
    "m_effectPos = master->m_pos", "m_stateCnt != 0", "LaunchEffectPrim(PP_SMA",
    "prim->m_duration = m_duration", "prim->m_totalTexture = 0", "prim->m_size = 0.0f",
    "prim->m_deltaPos = m_deltaPos", "prim->m_pos = m_effectPos", "gi.process = static_cast<int>(index) - 3",
    "gi.sparkX = m_effectPos.x", "gi.sparkZ = m_effectPos.z", "rand() % 5", "gi.rotStart = 0",
    "gi.height[1] = 0.80000001f", "gi.flag[0] = static_cast<u8>(variant)",
):
    assert token in sma, f"local SMA profile missing {token}"
assert "SpawnSma" in header
assert "case 551: SpawnSma(0); return true;" in routes
assert '551: "SpawnSma(0); return true;"' in generator
assert re.search(r"offsetof\(EffectGi, sparkX\) == 132", local)
assert re.search(r"offsetof\(EffectGi, sparkY\) == 136", local)
assert re.search(r"offsetof\(EffectGi, sparkZ\) == 140", local)
assert re.search(r"sizeof\(EffectGi\) == 180", local)
assert not re.search(r"effect551|551.*(?:texture|PlayWave)", sma, re.I)
print("effect551 SMA source, four-lane payload, offset mapping and route checks passed")
