"""Compare effect457's inline PE behavior with the explicit local port."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
pe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref_sources = [(ROOT / name).read_text(encoding="latin-1") for name in ("Ref/RagEffect.cpp", "Ref/RagEffect2.cpp")]
source = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
caller = (ROOT / "src/world/SkillSpecialEffect.cpp").read_text(encoding="utf-8")

signature = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
start = pe_text.rindex(signature)
opening = pe_text.index("{", start)
depth = 0
for pos in range(opening, len(pe_text)):
    if pe_text[pos] == "{":
        depth += 1
    elif pe_text[pos] == "}":
        depth -= 1
        if depth == 0:
            pe = pe_text[start:pos + 1]
            break
else:
    raise SystemExit("unterminated canonical OnProcess")
case_start = pe.index("case 457:")
case_end = pe.index("case 459:", case_start)
case = pe[case_start:case_end]
for token in (
    "m_stateCnt == 5", "this->m_pos = (*p_m_master)->m_pos",
    "PlayWave(aEffectT_1, xbr, 0.0, zbr, 250, 40, 1.0)",
    "m_stateId = 27", "SetForceAnimation(*p_m_master, 0x60u, 5u, 5u, 0x60u, 5u, 1, 0x19u, 0)",
    "m_stateCnt == 35", "m_stateId == 27", "m_stateId = 2",
):
    if token not in case:
        raise SystemExit(f"canonical PE effect457 case missing {token!r}")
if 'char aEffectT_1[] = "effect\\\\T_";' not in pe_text:
    raise SystemExit("canonical effect457 wave literal changed")
if any("case 457:" in ref for ref in ref_sources):
    raise SystemExit("a Ref RagEffect source unexpectedly contains case457")
skill421 = caller[caller.index("case 421:"):caller.index("case 467:")]
for call in ("LaunchOn(source, 457)", "AimEffect(LaunchOn(source, 439), target->m_pos)", "LaunchOn(target, 458)"):
    if call not in skill421:
        raise SystemExit(f"skill421 caller sequence changed: {call}")

sig = "bool CRagEffect::ProcessEffect457()"
local_start = source.rindex(sig)
opening = source.index("{", local_start)
depth = 0
for pos in range(opening, len(source)):
    if source[pos] == "{":
        depth += 1
    elif source[pos] == "}":
        depth -= 1
        if depth == 0:
            local = source[local_start:pos + 1]
            break
else:
    raise SystemExit("unterminated local effect457 handler")
if "bool ProcessEffect457();" not in header or "case 457: return ProcessEffect457();" not in dispatch:
    raise SystemExit("local effect457 declaration or generated route missing")
ordered = (
    "if (m_stateCnt == 5)", "m_effectPos = master->m_pos", '"effect\\\\T_"',
    "master->m_stateId = 27", "SetForceAnimation(0x60, 5, 5, 0x60, 5, 1, 25, 0)",
    "if (m_stateCnt == 35 && master->m_stateId == 27)", "master->m_stateId = 2",
)
cursor = 0
for token in ordered:
    found = local.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local effect457 behavior missing or out of order: {token}")
    cursor = found + len(token)
if "rand(" in local or "LaunchEffectPrim" in local:
    raise SystemExit("effect457 must not consume RNG or emit a primitive")
print("effect457 parity: skill421 launch sequence, state5 audio/forced motion and state35 reset passed")
