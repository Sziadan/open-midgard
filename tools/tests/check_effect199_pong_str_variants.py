"""Verify effect199's random Pong STR selection against Ref and PE."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SOURCE = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")
STR_TABLE = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


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
expected_mapping = r"case\s+199\s*:\s*v12\s*=\s*rand\(\);\s*sprintf\(StrName,\s*\"Pong%d\.str\",\s*v12\s*%\s*3\s*\+\s*1\);\s*break;"
for label, text in (("primary Ref", REF), ("canonical PE", PE)):
    init = body(text, signature)
    if not re.search(expected_mapping, init):
        raise SystemExit(f"{label} Init changed effect199's one-draw Pong1-3 STR selection")

for table, label in ((CATALOG, "catalog"), (STR_TABLE, "STR table")):
    if not re.search(r'\{\s*199,\s*nullptr,\s*nullptr,\s*"Pong",\s*1,\s*3\s*\}', table):
        raise SystemExit(f"local {label} does not encode Pong variants 1-3")

catalog_resolver = body(SOURCE, "std::string ResolveCatalogStrName(const RagEffectCatalogEntry& entry)")
if not re.search(r"const int offset\s*=\s*rand\(\)\s*%\s*entry\.variantCount;", catalog_resolver):
    raise SystemExit("local catalog resolver changed its one rand()%variantCount selection")
if not re.search(r'"%s%d\.str"\s*,\s*entry\.variantPrefix\s*,\s*entry\.variantFirst\s*\+\s*offset', catalog_resolver):
    raise SystemExit("local catalog resolver changed variant filename formatting")
resolved = body(SOURCE, "std::string ResolveStrName(int effectId)")
if "ResolveCatalogStrName(*entry)" not in resolved:
    raise SystemExit("effect initialization no longer uses the catalog string resolver")
if "case 199: return ProcessEZ2STR();" not in DISPATCH:
    raise SystemExit("local effect199 route is missing")

pong = ExeImage(ROOT / "HighPriest.exe").read_cstring_at(0x71AB7C)
if pong is None or pong.raw != b"Pong%d.str":
    raise SystemExit("canonical Pong STR format bytes differ at VA 0x71AB7C")
print("effect199 parity: Ref/PE one-draw Pong mapping, local variants, resolver, route and exact format bytes passed")
