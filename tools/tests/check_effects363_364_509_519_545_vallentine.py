#!/usr/bin/env python3
"""Compare Vallentine effects against the executable and Ref callers/helper."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
REF2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH2 = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")
DISPATCH3 = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
COVERAGE = (ROOT / "docs/skill/generated/EFFECT_COVERAGE.tsv").read_text(encoding="utf-8")
PARITY = (ROOT / "docs/skill/PARITY_LOG.md").read_text(encoding="utf-8")
symbols = SymbolTable(ExeImage())


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


variants = ((363, 0, "0x16B", 10526), (364, 1, "0x16C", 10529),
            (509, 2, "0x1FD", 11516), (519, 3, "0x207", 11596),
            (545, 4, "0x221", 11731))
for effect_id, mode, case_id, ref_line in variants:
    caller = f"case {case_id}:\n      CRagEffect::Vallentine(this, {mode});"
    require(caller in PE and caller in REF, f"effect {effect_id} PE/primary Ref caller changed")
    require(f"CRagEffect::Vallentine(this, {mode});" in REF[REF.find(caller):REF.find(caller) + 160]
            and f"CRagEffect::Vallentine(this, {mode});" in PE[PE.find(caller):PE.find(caller) + 160],
            f"effect {effect_id} call mapping changed")
    dispatch = DISPATCH2 if effect_id in (363, 364) else DISPATCH3
    route = f"case {effect_id}: return ProcessVallentine({mode});"
    require(route in dispatch and f'{effect_id}: "return ProcessVallentine({mode});"' in GENERATOR,
            f"effect {effect_id} runtime/generator route changed")
    row = next((line.split("\t") for line in COVERAGE.splitlines()
                if line.startswith(f"{effect_id}\t")), None)
    require(row is not None and row[3] == "routed", f"effect {effect_id} coverage row is not routed")
    lines = PARITY.splitlines()
    evidence_line = int(row[6])
    require(0 < evidence_line <= len(lines) and "Case list:" in lines[evidence_line - 1]
            and re.search(rf"(?<!\d){effect_id}(?!\d)", lines[evidence_line - 1]),
            f"effect {effect_id} coverage does not point at its case-list evidence")

pe = re.search(r"void __thiscall CRagEffect::Vallentine\([^)]*\)\s*\{(.*?)\n\}", PE, re.S)
ref2 = re.search(r"void\s+CRagEffect\s*::\s*Vallentine\([^)]*\)\s*\{(.*?)\n\}", REF2, re.S)
require(pe is not None and ref2 is not None, "PE/secondary Ref Vallentine helper missing")
for label, body in (("PE", pe.group(1)), ("secondary Ref", ref2.group(1))):
    for token in ("m_master->m_pos", "m_stateCnt", "PlayWave", "PP_3DPARTICLE",
                  "m_duration", "m_deltaPos2.y", "m_animSpeed = 2",
                  "SetAction", "InitSegment"):
        require(token in body, f"{label} Vallentine helper lacks {token}")
    require(("case 2:" in body or "F1 == 2" in body)
            and ("case 3:" in body or "F1 == 3" in body)
            and ("case 4:" in body or "F1 == 4" in body),
            f"{label} Vallentine mode branches missing")
    require(("0x200u" in body or "PT_USEORGARGB" in body)
            and ("m_renderFlag |= 0x200u" in body or "RF_NODEPTHCHECK" in body),
            f"{label} Vallentine pattern/render bits missing")
    require("m_repeatAnim = false" in body or "m_repeatAnim = 0" in body,
            f"{label} Vallentine repeat animation setting missing")
    require(body.index("m_master->m_pos") < body.index("m_stateCnt") < body.index("PP_3DPARTICLE"),
            f"{label} Vallentine position/state/emission order changed")

assets = {
    "aEffectVallenti": b"effect\\vallentine.wav",
    "byte_71D380": b"\xC0\xCC\xC6\xD1\xC6\xAE\\vallentine.spr",
    "byte_71D368": b"\xC0\xCC\xC6\xD1\xC6\xAE\\vallentine.act",
    "byte_71D400": b"\xC0\xCC\xC6\xD1\xC6\xAE\\05vallentine.spr",
    "byte_71D3E8": b"\xC0\xCC\xC6\xD1\xC6\xAE\\05vallentine.act",
    "byte_71D3D8": b"\xC0\xCC\xC6\xD1\xC6\xAE\\fast.spr",
    "byte_71D3C8": b"\xC0\xCC\xC6\xD1\xC6\xAE\\fast.act",
    "byte_71D3B0": b"\xC0\xCC\xC6\xD1\xC6\xAE\\ro2effect.spr",
    "byte_71D398": b"\xC0\xCC\xC6\xD1\xC6\xAE\\ro2effect.act",
}
for name, expected in assets.items():
    resolved = symbols.resolve_name(name)
    require(resolved is not None and resolved.raw == expected, f"{name} executable bytes changed")

local = re.search(r"bool CRagEffect::ProcessVallentine\(int variant\)\s*\{(.*?)\n\}", LOCAL, re.S)
require(local is not None, "local ProcessVallentine handler missing")
body = local.group(1)
for token in ("m_effectPos = master->m_pos", "m_stateCnt != 0", "PlayWave(\"effect\\\\vallentine.wav\"",
              "PP_3DPARTICLE", "prim->m_pattern |= 0x200u", "prim->m_duration = m_duration",
              "prim->m_deltaPos2.y = 0.0f", "prim->m_animSpeed = 2", "prim->m_animSpeed = 4",
              "prim->m_renderFlag |= 0x200u", "prim->m_spriteMotionBase = 16",
              "prim->m_numSegments = 1", "InitReferenceSegments(prim)"):
    require(token in body, f"local Vallentine helper lacks {token}")
require(body.index("m_effectPos = master->m_pos") < body.index("m_stateCnt != 0")
        < body.index("PP_3DPARTICLE"), "local Vallentine position/state/emission order changed")
for raw, suffix in (("05vallentine", "2"), ("fast", "3"), ("ro2effect", "4")):
    require(f"\\\\{raw}.spr" in body and f"\\\\{raw}.act" in body,
            f"local Vallentine mode {suffix} resource mapping changed")
require("\\\\vallentine.spr" in body and "\\\\vallentine.act" in body,
        "local Vallentine default resource mapping changed")
print("effects 363/364/509/519/545 Vallentine source and asset checks passed")
