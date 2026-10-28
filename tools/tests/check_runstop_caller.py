"""Compare RunStop helper, effect444 caller order, and exact PE wave bytes."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ro_exe_resolve as resolver


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(signature)


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref_helper = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
ref_caller = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

signature = "unsigned __int8 __thiscall CRagEffect::RunStop(CRagEffect *this, __int16 time)"
if compact(function(pe, signature)) != compact(function(ref_helper, signature)):
    raise SystemExit("Ref/PE RunStop helper mismatch")
main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
caller = main[main.index("case 444:"):main.index("case 445:")]
for token in ("PlayWave(aEffectT", "RunStop(this, 0)", "RunStop(this, 45)"):
    if token not in caller:
        raise SystemExit(f"canonical effect444 caller missing {token}")
if not caller.index("PlayWave(aEffectT") < caller.index("RunStop(this, 0)") < caller.index("RunStop(this, 45)"):
    raise SystemExit("canonical effect444 call order changed")
if "case 444:" in ref_caller:
    raise SystemExit("Ref caller omission changed; review PE-only caller evidence")

wave = resolver.ExeImage().read_cstring_at(0x719F40).raw
if wave != b"effect\\T_\xC8\xBF\xB0\xFA\xC0\xBD1.wav":
    raise SystemExit(f"effect444 wave bytes changed: {wave!r}")
if "bool ProcessRunStop444();" not in header or "void SpawnRunStop(short time);" not in header:
    raise SystemExit("RunStop caller/helper declarations missing")
if "case 444: return ProcessRunStop444();" not in dispatch:
    raise SystemExit("effect444 route missing")
process = function(local, "bool CRagEffect::ProcessRunStop444()")
if not process.index("TryPlayEffectWaveAt") < process.index("SpawnRunStop(0)") < process.index("SpawnRunStop(45)"):
    raise SystemExit("local effect444 call order changed")
for token in ("m_stateCnt == 0", '"effect\\\\T_\\xC8\\xBF\\xB0\\xFA\\xC0\\xBD" "1.wav"'):
    if token not in process:
        raise SystemExit(f"local effect444 wave branch missing {token}")
emitter = function(local, "void CRagEffect::SpawnRunStop(short time)")
for token in ("m_effectPos = master->m_pos", "m_stateCnt != 0", "PP_SLASH1", "m_renderFlag = 5",
              '"effect\\\\magic_blue.tga"', "m_size = 4.0f", "lane < 4", "gi.life = 1",
              "gi.distance = 2.0f", "gi.height[0] = 1.0f", "gi.rotStart = static_cast<short>(time + 90 * lane)",
              "gi.flag[0] = 1"):
    if token not in emitter:
        raise SystemExit(f"RunStop GI setup missing {token}")
print("RunStop444 parity: Ref/PE helper, PE caller order, exact wave bytes, route and four-lane payload passed")
