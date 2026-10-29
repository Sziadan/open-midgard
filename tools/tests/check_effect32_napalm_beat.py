"""Compare effect32 NapalmBeat PE/Ref body, local route, assets and fixture."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable


PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def body(text: str, signature: str, start: int | None = None) -> str:
    begin = text.index(signature, start or 0)
    opening = text.index("{", begin)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[begin:pos + 1]
    raise AssertionError(f"unterminated body: {signature}")


signature = "void __thiscall CRagEffect::NapalmBeat(CRagEffect *this)"
pe_handler = body(PE, signature, PE.index(signature + "; // idb") + 1)
ref_handler = body(REF, signature)
local_handler = body(SRC, "void CRagEffect::SpawnNapalmBeat()")
pe_process = body(
    PE,
    "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
    PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1,
)
legacy_process = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")

if re.sub(r"\s+", " ", pe_handler).strip() != re.sub(r"\s+", " ", ref_handler).strip():
    raise SystemExit("canonical PE and primary Ref NapalmBeat bodies differ")
if not re.search(r"case\s+32\s*:\s*CRagEffect::NapalmBeat\(this\)", pe_process):
    raise SystemExit("canonical PE effect32 route to NapalmBeat is missing")
if "case 32: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect32 route is missing")
if not re.search(r"if\s*\(m_type\s*==\s*32\)\s*\{\s*SpawnNapalmBeat\(\);\s*return\s+true;\s*\}", legacy_process):
    raise SystemExit("local legacy route no longer dispatches effect32 to SpawnNapalmBeat")

ordered = (
    "ProjectReferenceEffectVertex(master->m_pos", "m_tlvertX = projected.x", "m_tlvertY = projected.y",
    "if (!(m_stateCnt < 15 && m_stateCnt >= m_stateCnt % 5)) return",
    "if (m_stateCnt == 0", 'PlayWave("effect\\\\EF_NapalmBeat.wav"',
    "LaunchEffectPrim(PP_2DTEXTURE", "prim->m_duration = 30",
    "rand() % 50 + 10", "rand() % 360", "rand() % 40 + 20",
    "prim->m_alpha = 0.0f", "prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3",
    "prim->m_repeatAnim = false", "prim->m_animSpeed = 2", "prim->m_totalTexture = 8",
    'effect\\\\\\xC6\\xF8\\xB9\\xDF%d.TGA', "prim->m_gi[0].life = 0",
)
cursor = 0
for token in ordered:
    found = local_handler.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local NapalmBeat branch or operation order missing: {token}")
    cursor = found + len(token)
if len(re.findall(r"\brand\s*\(", pe_handler)) != 3 or len(re.findall(r"\brand\s*\(", local_handler)) != 3:
    raise SystemExit("NapalmBeat must preserve exactly three ordered random draws per emitted texture primitive")

symbols = SymbolTable(ExeImage(ROOT / "HighPriest.exe"))
wave = symbols.resolve_name("aEffectEfNapalm")
if wave is None or wave.va != 0x71A790 or wave.raw != b"effect\\EF_NapalmBeat.wav":
    raise SystemExit(f"NapalmBeat PE wave bytes changed: {wave!r}")
image = ExeImage(ROOT / "HighPriest.exe")
texture_format = image.read_cstring_at(0x71B730)
if texture_format is None or texture_format.raw != bytes.fromhex("6566666563745cc6f8b9df25642e544741"):
    raise SystemExit(f"NapalmBeat PE texture format bytes changed: {texture_format!r}")

for phrase in (
    "Napalm emits one reference screen primitive with three ordered draws despite reduced effects",
    "Napalm projects actor x/y while retaining stored origin and projected z",
    "Napalm zero-frame sound uses stored origin before RNG",
    "Napalm draws radius then angle then shared square size",
    "Napalm preserves duration centered pivot alpha rise constructor maximum and duration-minus-third fade",
    "Napalm uses eight nonrepeating textures with divisor two and inactive GI slot",
    "Napalm requests exact recovered CP949 uppercase texture sequence one through eight",
    "Napalm remainder comparison emits every frame one through fourteen with no repeated sound",
    "Napalm frame fifteen closes emission but still refreshes actor projection",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"NapalmBeat lifecycle fixture missing {phrase!r}")

print("effect32 NapalmBeat PE/Ref body, local route, resources and lifecycle coverage passed")
