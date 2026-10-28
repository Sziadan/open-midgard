"""Compare effect704's inline OnProcess3 behavior with PE and primary Ref."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
source = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
pe_caller = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
local_caller = (ROOT / "src/world/GameActorMsgHandler.cpp").read_text(encoding="utf-8")

def case_body(text: str) -> str:
    start = text.index("case 0x2C0:")
    end = text.index("default:", start)
    return text[start:end]

pe_case = case_body(pe)
ref_case = case_body(ref)
if pe_case != ref_case:
    raise SystemExit("PE and primary Ref effect704 OnProcess3 case bodies differ")
for token in ("v2 < 10", "v2 >= 34", "m_roty = this->m_master->m_roty + 30.0", "m_roty - 360.0"):
    if token not in pe_case:
        raise SystemExit(f"canonical effect704 branch missing {token!r}")
if "case 704: return ProcessEffect704();" not in dispatch or "bool ProcessEffect704();" not in header:
    raise SystemExit("local effect704 declaration or OnProcess3 route missing")
if "LaunchEffect(GameActorByAID, 704" not in pe_caller:
    raise SystemExit("canonical skill273 caller no longer launches effect704")
if "target->LaunchEffect(704" not in local_caller:
    raise SystemExit("local skill273 caller no longer launches effect704")

sig = "bool CRagEffect::ProcessEffect704()"
start = source.rindex(sig)
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
    raise SystemExit("unterminated local effect704 handler")
ordered = (
    "if (m_stateCnt < 10 || m_stateCnt >= 34)", "master->m_roty += 30.0f",
    "if (master->m_roty > 360.0f) master->m_roty -= 360.0f",
)
cursor = 0
for token in ordered:
    found = local.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local effect704 behavior missing or out of order: {token}")
    cursor = found + len(token)
if "rand(" in local or "PlayWave(" in local or "LaunchEffectPrim" in local:
    raise SystemExit("effect704 must not consume RNG, play audio or emit a primitive")
print("effect704 parity: PE/Ref case equality, skill273 caller and states10-33 rotation passed")
