"""Compare effect445's inline PE behavior with the explicit local port."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
pe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref_text = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local_text = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
pe_caller = pe_text[pe_text.index("case 19:", pe_text.index("CGameActor::")) :]
local_caller = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")

onprocess_sig = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
onprocess_at = pe_text.rindex(onprocess_sig)
onprocess_open = pe_text.index("{", onprocess_at)
depth = 0
for pos in range(onprocess_open, len(pe_text)):
    if pe_text[pos] == "{":
        depth += 1
    elif pe_text[pos] == "}":
        depth -= 1
        if depth == 0:
            onprocess = pe_text[onprocess_at:pos + 1]
            break
else:
    raise SystemExit("unterminated canonical OnProcess")
start = onprocess.index("case 445:")
end = onprocess.index("case 446:", start)
case = onprocess[start:end]
for token in (
    "m_stateCnt == 30", "this->m_pos = (*p_m_master)->m_pos",
    "PlayWave(aEffectT_8, xbn, 0.0, zbn, 250, 40, 1.0)",
    "m_BodyLight & 0x10000", "m_stateCnt > 35", "m_BodyLight += 0x10000",
    "m_BodyTime = LOWORD(this->m_stateCnt) - 35",
):
    if token not in case:
        raise SystemExit(f"canonical PE effect445 case missing {token!r}")
if 'char aEffectT_8[] = "effect\\\\T_";' not in pe_text:
    raise SystemExit("canonical effect445 wave literal changed")
if "case 445:" in ref_text:
    raise SystemExit("primary Ref unexpectedly contains an effect445 case")
if "CAbleToMakeEffect::LaunchEffect(this, 445" not in pe_caller:
    raise SystemExit("canonical action19 caller no longer launches effect445")
if "GetEffectType() == 445" not in local_caller or "LaunchEffect(445, vector3d{}" not in local_caller:
    raise SystemExit("local action19 remove-and-relaunch lifecycle for effect445 changed")

sig = "bool CRagEffect::ProcessEffect445()"
local_start = local_text.rindex(sig)
opening = local_text.index("{", local_start)
depth = 0
for pos in range(opening, len(local_text)):
    if local_text[pos] == "{":
        depth += 1
    elif local_text[pos] == "}":
        depth -= 1
        if depth == 0:
            local = local_text[local_start:pos + 1]
            break
else:
    raise SystemExit("unterminated local effect445 handler")
if "bool ProcessEffect445();" not in header or "case 445: return ProcessEffect445();" not in dispatch:
    raise SystemExit("local effect445 declaration or generated route missing")
ordered = (
    "if (m_stateCnt == 30)", "m_effectPos = master->m_pos", '"effect\\\\T_"',
    "master->m_BodyLight & 0x10000", "m_stateCnt > 35", "master->m_BodyLight += 0x10000",
    "master->m_BodyTime = static_cast<u16>(m_stateCnt - 35)",
)
cursor = 0
for token in ordered:
    found = local.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local effect445 behavior missing or out of order: {token}")
    cursor = found + len(token)
if "rand(" in local or "LaunchEffectPrim" in local:
    raise SystemExit("effect445 must not consume RNG or emit a primitive")
print("effect445 parity: action19 lifecycle, inline state30 wave and one-shot light timing passed")
