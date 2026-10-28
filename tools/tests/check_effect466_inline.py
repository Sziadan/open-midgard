"""Compare effect466's inline PE behavior and launch paths with the local port."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
pe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref_sources = [(ROOT / name).read_text(encoding="latin-1") for name in ("Ref/RagEffect.cpp", "Ref/RagEffect2.cpp")]
source = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
skill_caller = (ROOT / "src/world/SkillSpecialEffect.cpp").read_text(encoding="utf-8")
use_skill = (ROOT / "src/gamemode/UseSkillEffect.generated.inc").read_text(encoding="utf-8")

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
case_start = pe.index("case 466:")
case_end = pe.index("case 467:", case_start)
case = pe[case_start:case_end]
for token in (
    "m_stateCnt == 5", "this->m_pos = (*p_m_master)->m_pos",
    "PlayWave(aEffectT_21, xbo, 0.0, zbo, 250, 40, 1.0)",
    "m_BodyLight & 0x40", "v100 >= 14", "v100 < 50",
    "m_BodyLight += 64", "m_BodyTime = LOWORD(this->m_stateCnt) - 14",
):
    if token not in case:
        raise SystemExit(f"canonical PE effect466 case missing {token!r}")
if 'char aEffectT_21[] = "effect\\\\T_";' not in pe_text:
    raise SystemExit("canonical effect466 wave literal changed")
if any("case 466:" in ref for ref in ref_sources):
    raise SystemExit("a Ref RagEffect source unexpectedly contains case466")
if "case 417:" not in skill_caller or "LaunchOn(target, 466)" not in skill_caller:
    raise SystemExit("skill417 target caller no longer launches effect466")
combo = use_skill[use_skill.index("case 0x1A9:"):use_skill.index("case 0x1AA:")]
for token in ("LaunchOn(source, 461)", "i = clampedLevel + 466", "LaunchOn(source, i)"):
    if token not in combo:
        raise SystemExit(f"skill combo caller no longer launches effect466 family: {token}")

sig = "bool CRagEffect::ProcessEffect466()"
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
    raise SystemExit("unterminated local effect466 handler")
if "bool ProcessEffect466();" not in header or "case 466: return ProcessEffect466();" not in dispatch:
    raise SystemExit("local effect466 declaration or generated route missing")
ordered = (
    "if (m_stateCnt == 5)", "m_effectPos = master->m_pos", '"effect\\\\T_"',
    "master->m_BodyLight & 0x40", "m_stateCnt >= 14", "m_stateCnt < 50",
    "master->m_BodyLight += 64", "master->m_BodyTime = static_cast<u16>(m_stateCnt - 14)",
)
cursor = 0
for token in ordered:
    found = local.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local effect466 behavior missing or out of order: {token}")
    cursor = found + len(token)
if "rand(" in local or "LaunchEffectPrim" in local:
    raise SystemExit("effect466 must not consume RNG or emit a primitive")
print("effect466 parity: target/combo callers, state5 audio and states14-49 body-light window passed")
