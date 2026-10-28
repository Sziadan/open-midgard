"""Compare effect501's inline OnProcess3 behavior with PE and primary Ref."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable

pe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
source = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
skill = (ROOT / "src/session/SkillActionInfo2.cpp").read_text(encoding="utf-8")

def case_body(text: str) -> str:
    start = text.index("case 0x1F5:")
    end = text.index("case 0x1F6:", start)
    return text[start:end]

pe_case = case_body(pe_text)
ref_case = case_body(ref)
if pe_case != ref_case:
    raise SystemExit("PE and primary Ref effect501 OnProcess3 case bodies differ")
if "case 501: return ProcessEffect501();" not in dispatch:
    raise SystemExit("OnProcess3 effect501 route missing")
if "bool ProcessEffect501();" not in header:
    raise SystemExit("effect501 handler declaration missing")
for skill_id in (389, 555):
    start = skill.index(f"case {skill_id}:")
    end = skill.find("case ", start + 5)
    if end < 0:
        end = len(skill)
    if "beginEffectId = 501;" not in skill[start:end]:
        raise SystemExit(f"SkillActionInfo2 skill {skill_id} no longer begins effect501")

wave = SymbolTable(ExeImage(ROOT / "HighPriest.exe")).resolve_name("aEffectEfBegins")
if wave is None or wave.va != 0x7193CC or wave.raw != b"effect\\EF_BeginSpell.wav":
    raise SystemExit(f"effect501 PE wave bytes changed: {wave!r}")

signature = "bool CRagEffect::ProcessEffect501()"
start = source.rindex(signature)
opening = source.index("{", start)
depth = 0
for pos in range(opening, len(source)):
    if source[pos] == "{":
        depth += 1
    elif source[pos] == "}":
        depth -= 1
        if depth == 0:
            local = source[start:pos + 1]
            break
else:
    raise SystemExit("unterminated effect501 local handler")
ordered = (
    "if (m_stateCnt == 0)", "m_effectPos = master->m_pos", '"effect\\\\EF_BeginSpell.wav"',
    "if (master->m_stateId != 0)", "if (m_stateCnt > 40)", "master->m_roty += 48.0f",
    "else if (m_stateCnt > 30)", "master->m_roty += 24.0f", "else if (m_stateCnt > 15)",
    "master->m_roty += 18.0f", "else", "master->m_roty += 12.0f",
    "if (master->m_roty > 360.0f) master->m_roty -= 360.0f",
)
cursor = 0
for token in ordered:
    found = local.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local effect501 behavior missing or out of order: {token}")
    cursor = found + len(token)
if "rand(" in local or "LaunchEffectPrim" in local:
    raise SystemExit("effect501 must not consume RNG or emit a primitive")
print("effect501 parity: exact PE/Ref case, skill-table callers, wave bytes and rotation tiers passed")
