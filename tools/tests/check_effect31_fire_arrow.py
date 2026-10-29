"""Compare effect31 FireArrow PE/Ref body, local route, resources and fixture."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage


PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def body(text: str, signature: str, start: int = 0) -> str:
    begin = text.index(signature, start)
    opening = text.index("{", begin)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[begin : pos + 1]
    raise AssertionError(f"unterminated body: {signature}")


signature = "void __thiscall CRagEffect::FireArrow(CRagEffect *this)"
pe_handler = body(PE, signature, PE.index(signature + "; // idb") + 1)
ref_handler = body(REF, signature)
local_handler = body(SRC, "void CRagEffect::ApplyFireArrow()")
pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1,
)
legacy_process = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")

if re.sub(r"\s+", " ", pe_handler).strip() != re.sub(r"\s+", " ", ref_handler).strip():
    raise SystemExit("canonical PE and primary Ref FireArrow bodies differ")
if not re.search(r"case\s+31\s*:\s*CRagEffect::FireArrow\(this\)", pe_process):
    raise SystemExit("canonical PE effect31 route to FireArrow is missing")
if "case 31: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect31 route is missing")
if not re.search(r"if\s*\(m_type\s*==\s*31\)\s*\{\s*ApplyFireArrow\(\);\s*return\s+true;\s*\}", legacy_process):
    raise SystemExit("local legacy route no longer dispatches effect31 to ApplyFireArrow")

ordered = (
    "m_effectPos = master->m_pos", "if (m_stateCnt == 12)", '"effect\\\\EF_FireArrow%d.wav"',
    "m_count *= 10", "const int flight = m_stateCnt - 12", "flight % 10 == 0 && flight < m_count",
    "LaunchEffectPrim(PP_3DCROSSTEXTURE", "const int spawnZ = rand() % 10", "const int spawnX = rand() % 10",
    "const int aimZ = rand() % 4", "const int aimX = rand() % 4 - 2", "prim->m_speed = -2.75f",
    "prim->m_totalTexture = 6", "for (int i = 1; i <= 6; ++i)",
    '"effect\\\\\\xBA\\xD2\\xC8\\xAD\\xBB\\xEC%d.tga"',
    "if (m_stateCnt > 0 && (m_stateCnt & 3) == 0)", "LaunchEffectPrim(PP_3DPARTICLE",
    "SetParticleResources", "InitReferenceSegments(prim)", "if (m_flag != 0)", "m_flag = 0",
    "LaunchEffectPrim(PP_3DCIRCLE", '"effect\\\\ring_yellow.tga"',
)
cursor = 0
for token in ordered:
    found = local_handler.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local FireArrow branch or operation order missing: {token}")
    cursor = found + len(token)
if len(re.findall(r"\brand\s*\(", pe_handler)) != 10:
    raise SystemExit("FireArrow PE random-draw count changed")

image = ExeImage(ROOT / "HighPriest.exe")
wave = image.read_cstring_at(0x71B714)
if wave is None or wave.raw != b"effect\\EF_FireArrow%d.wav":
    raise SystemExit(f"FireArrow PE wave format bytes changed: {wave!r}")

for phrase in (
    "FireArrow frame twelve orders one wave draw four projectile draws then five trail draws",
    "FireArrow selects exact sound and horizontal actor listener delta before launch draws",
    "FireArrow projectile uses source Z-before-X spawn draws and reference cross path",
    "FireArrow trail preserves ordered speed size source drag and shrink arithmetic",
    "FireArrow consumes projectile hit flag after updating current actor origin",
    "FireArrow zero count preserves wave and trail without inventing a minimum projectile burst",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"FireArrow lifecycle fixture missing {phrase!r}")

print("effect31 FireArrow PE/Ref body, local route, resources and lifecycle coverage passed")
