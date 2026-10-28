"""Verify effect185 Suicide STR mapping and target-position caller parity."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SOURCE = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")
STR_TABLE = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")


def body(text: str, signature: str) -> str:
    start = text.rindex(signature)
    opening = text.index("{", start + len(signature))
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{": depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening:pos + 1]
    raise AssertionError(f"unterminated function {signature}")


signature = "void __thiscall CRagEffect::Init(CRagEffect *this, CRenderObject *master, int effectId, vector3d fDeltaPos)"
ref_init = body(REF, signature)
pe_init = body(PE, signature)
for label, text in (("primary Ref", ref_init), ("canonical PE", pe_init)):
    match = re.search(r"case\s+185\s*:\s*sprintf\(StrName,\s*(\w+)\);\s*break;", text)
    if not match or match.group(1) != "aSuicideStr":
        raise SystemExit(f"{label} Init no longer maps effect185 to aSuicideStr")

resolved = SymbolTable(ExeImage()).resolve_name("aSuicideStr")
if resolved is None or (resolved.va, resolved.text) != (0x71AC2C, "suicide.str"):
    raise SystemExit(f"aSuicideStr resolved as {resolved!r}, expected 0x71AC2C suicide.str")
for table, label in ((CATALOG, "catalog"), (STR_TABLE, "STR table")):
    if not re.search(r'\{\s*185,\s*"suicide\.str"', table):
        raise SystemExit(f"local {label} does not map effect185 to suicide.str")
if "case 185: ProcessTargetPositionStr(); return FinishEffectFrame();" not in DISPATCH:
    raise SystemExit("local effect185 route is missing")

main = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
start = main.index("          case 185:")
end = main.index("          case 196:", start)
caller = main[start:end]
for token in (
    "this->m_deltaPos2 = (*p_m_master)->m_pos;",
    "(*p_m_master)->m_pos = this->m_deltaPos;",
    "CRagEffect::ProcessEZ2STR(this, 0.0, -80.0, 0.0);",
    "if ( !v153 )", "return 0;", "v156->z = this->m_deltaPos2.z;",
):
    if token not in caller:
        raise SystemExit(f"canonical effect185 caller is missing {token}")

local = body(SOURCE, "bool CRagEffect::ProcessTargetPositionStr()")
ordered = (
    "ResolveCurrentMaster()", "m_deltaPos2 = master->m_pos;", "master->m_pos = m_deltaPos;",
    "const bool active = ProcessEZ2STR();", "master->m_pos = m_deltaPos2;", "return active;",
)
cursor = 0
for token in ordered:
    found = local.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local effect185 behavior is missing or out of order: {token}")
    cursor = found + len(token)

print("effect185 parity: Ref/PE STR mapping, PE resource bytes, caller restoration, local route and helper passed")
