"""Compare BlitzBeat helper and verify effect115 ordering, emitters, RNG, and family routes."""
from __future__ import annotations
import re
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ro_exe_resolve as resolver

def function(source: str, signature: str) -> str:
    start = source.rindex(signature); opening = source.index("{", start); depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{": depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0: return source[start:pos + 1]
    raise AssertionError(signature)
def compact(text: str) -> str: return re.sub(r"\s+", "", text)
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
msg_effect = (ROOT / "src/world/MsgEffect.cpp").read_text(encoding="utf-8")
ref_msg_effect = (ROOT / "Ref/Skill.cpp").read_text(encoding="latin-1")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
sig = "void __thiscall CRagEffect::BlitzBeat(CRagEffect *this)"
if compact(function(pe, sig)) != compact(function(ref, sig)): raise SystemExit("Ref/PE BlitzBeat helper mismatch")
main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case = main[main.index("case 115:"):main.index("case 116:")]
if "BlitzBeat(this)" not in case: raise SystemExit("canonical effect115 caller changed")
if "BlitzBeat(this" in ref: raise SystemExit("Ref caller omission changed; review PE-only caller evidence")
if "bool ProcessBlitzBeat115();" not in header or "case 115: return ProcessBlitzBeat115();" not in dispatch:
    raise SystemExit("effect115 route/declaration missing")
handler = function(local, "bool CRagEffect::ProcessBlitzBeat115()")
for token in ('"effect\\\\hunter_blitzbeat_1st.wav"', '"effect\\\\hunter_blitzbeat.wav"',
              "m_stateCnt == 5 || m_stateCnt == 30", "m_effectPos = master->m_pos", "m_stateCnt == 0",
              "g_blitzBeatEmissionCount = 0", "index < 10", "PP_3DCROSSTEXTURE", "rand() % 360",
              "rand() % 40 + 20", "effect\\\\ac_center2.tga", "m_stateCnt > 35", "m_stateCnt % 3 == 0",
              "g_blitzBeatEmissionCount < m_count", "rand() % 10", "rand() % 20", "PP_3DCYLINDER",
              "effect\\\\magic_green.tga", "++g_blitzBeatEmissionCount"):
    if token not in handler: raise SystemExit(f"BlitzBeat payload missing {token}")
if handler.count("rand()") != 5: raise SystemExit("expected 20 state-zero RNG draws plus 3 later draws")
on_process = function(msg_effect, "u8 CMsgEffect::OnProcess()")
falcon = on_process[on_process.index("case 18:"):on_process.index("case 5:", on_process.index("case 18:"))]
for token in ("m_falconIsAttackTarget", "m_falconStateCnt <= 10 || m_falconStateCnt >= 25",
              "m_pos.x += m_destPos2.x", "m_falconStateCnt >= 25", "m_falconMotionSpeed",
              "m_masterActor->m_pos", "GetHeight(m_pos.x, m_pos.z) - 25.0f",
              "std::atan2(dx, -dz)", "Get8Dir(m_falconRotation - 22.5f)",
              "m_falconLastMotionTick"):
    if token not in falcon: raise SystemExit(f"Falcon type18 processing missing {token}")
if "m_falconMotionSpeed(4.0f)" not in msg_effect or "m_falconIsAnimation(1)" not in msg_effect:
    raise SystemExit("Falcon source SetAction speed 4 was not preserved")
if "constexpr u32 kFalconAnimationStepMs = 24;" not in msg_effect:
    raise SystemExit("Falcon motion cadence must use the 24ms world simulation tick")
if "if (m_falconStateCnt > 35)" not in on_process or "m_falconIsAttackTarget = 0;" not in on_process:
    raise SystemExit("Falcon attack recovery does not resume owner follow")
if "Get8Dir2(this, this->m_roty, 1)" not in ref_msg_effect:
    raise SystemExit("Ref Falcon direction no longer uses rounded real-eight-direction mapping")
if "m_falconRotation - 202.5f" not in falcon:
    raise SystemExit("Falcon attack direction is not rounded after its 180-degree turn")
if "case 29: {" not in msg_effect or "m_falconIsAttackTarget = 1" not in msg_effect:
    raise SystemExit("Blitz Beat message29 target transition missing")
if "std::sin(radians) * 5.0f" not in msg_effect or "std::cos(radians) * 5.0f" not in msg_effect:
    raise SystemExit("Falcon owner-follow start offset missing")
for name, signature, token in (("cross-texture", "bool CEffectPrim::UsesReferenceCrossTexture() const", "115"),
                               ("cylinder", "bool CEffectPrim::UsesReferenceCylinder() const", "case 115")):
    family = function(local, signature)
    if token not in family: raise SystemExit(f"effect115 {name} reference family route missing")
for symbol, value in (("aEffectHunterBl_0", "effect\\\\hunter_blitzbeat_1st.wav"),
                      ("aEffectHunterBl_1", "effect\\\\hunter_blitzbeat.wav"),
                      ("aEffectAcCenter", "effect\\\\ac_center2.tga"),
                      ("aEffectMagicGre", "effect\\\\magic_green.tga")):
    if f'{symbol}[] = "{value}"' not in pe: raise SystemExit(f"canonical PE resource {symbol} changed")
image = resolver.ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x71BA98, b"effect\\hunter_blitzbeat_1st.wav"),
                     (0x71BA7C, b"effect\\hunter_blitzbeat.wav"),
                     (0x71B2D4, b"effect\\ac_center2.tga"),
                     (0x71A730, b"effect\\magic_green.tga")):
    actual = image.read_cstring_at(va)
    if actual is None or actual.raw != expected:
        raise SystemExit(f"canonical BlitzBeat resource bytes differ at VA 0x{va:X}")
print("BlitzBeat115/Falcon parity: Ref/PE helper, target message, owner follow, speed, route, resources, RNG/cadence, and render families passed")
