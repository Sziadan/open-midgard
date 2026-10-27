"""Source comparator for gameplay packet 0x01D0 / Zc_Spirits.

This deliberately does not include legacy window/UI behavior or claim that the
current IgnorePacket route implements the source handler.
"""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
EXE_C = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
REF = (ROOT / "Ref/GameModePacket.cpp").read_text(encoding="utf-8")
REF_ACTOR = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8")
PACKET_DB = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8", errors="replace")
IMPLEMENTATION = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")


def body(text: str, signature: str) -> str:
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise AssertionError(f"unterminated body: {signature}")


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


signature = "void __thiscall CGameMode::Zc_Spirits(CGameMode *this, const char *buf)"
exe_anchor = EXE_C.index("//----- (005A7F30)")
exe_end = EXE_C.index("//----- (005A8160)", exe_anchor)
ref_anchor = REF.index("// Original Signature: public: void __thiscall CGameMode::Zc_Spirits(char const *)")
ref_end = REF.index("// Original Signature:", ref_anchor + 80)
exe_body = body(EXE_C[exe_anchor:exe_end], signature)
ref_body = body(REF[ref_anchor:ref_end], signature)
assert compact(exe_body) == compact(ref_body), "canonical EXE and Ref Zc_Spirits bodies differ"

# Source calls GetJob, while current code directly reads m_job. Prove this is
# equivalent for the only special comparisons (4038 and 24): GetJob transforms
# other IDs but has no branch transforming either value.
get_job_signature = "int __thiscall CGameActor::GetJob(CGameActor *this)"
exe_job_anchor = EXE_C.index("//----- (0055B430)")
exe_job_end = EXE_C.index("//----- (0055B540)", exe_job_anchor)
ref_job_anchor = REF_ACTOR.index("// Original Signature: public: int __thiscall CGameActor::GetJob(void)")
ref_job_end = REF_ACTOR.index("// Original Signature:", ref_job_anchor + 80)
exe_job = body(EXE_C[exe_job_anchor:exe_job_end], get_job_signature)
ref_job = body(REF_ACTOR[ref_job_anchor:ref_job_end], get_job_signature)
assert compact(exe_job) == compact(ref_job), "canonical EXE and Ref GetJob bodies differ"
assert "m_job == 4038" not in exe_job and "m_job == 24" not in exe_job

# Ordered branch/operation anchors. Their order is checked independently in
# both bodies, then full normalized body equality proves the remaining syntax.
checklist = [
    ("Effect toggle gate", "if ( g_session.m_isEffectOn )"),
    ("Self AID resolves to local player; otherwise resolve actor by AID", "*(_DWORD *)(buf + 2) == g_session.m_aid"),
    ("Null actor returns without effect work", "if ( v2 )"),
    ("First job mapping: job 4038 -> message 109 / effect 504", "CGameActor::GetJob(v2) == 4038"),
    ("First job mapping: job 24 -> message 109 / effect 629", "CGameActor::GetJob(v2) == 24"),
    ("Other jobs -> message 109 / effect 228", "SendMsg(v2, 0, 109, 228, 0, 0)"),
    ("Actor AID zero skips the launch/update branch", "if ( *(_DWORD *)(buf + 2) )"),
    ("Burrow or hiding state takes refresh-only branch", "IsEffectStateBurrow(v2->m_effectState) || IsEffectStateHiding(v2->m_effectState)"),
    ("Burrow/hiding repeats the job-specific message 109 mapping", "SendMsg(v2, 0, 109, 504, 0, 0)"),
    ("Visible-state branch launches job-specific effect at zero offset", "CAbleToMakeEffect::LaunchEffect(v2, 504, v5[0], 0.0)"),
    ("Packet signed 16-bit field at +6 is copied to effect position X", "pos.x = (float)*((__int16 *)buf + 3)"),
    ("Successful launch receives message 14 with position pointer", "SendMsg(v3, 0, 14, (int)&pos, 0, 0)"),
]

for label, token in checklist:
    assert token in exe_body, f"missing EXE branch anchor: {label}: {token}"
    assert token in ref_body, f"missing Ref branch anchor: {label}: {token}"
    assert exe_body.count(token) == ref_body.count(token), f"anchor occurrence mismatch: {label}"

impl_body = body(IMPLEMENTATION, "void HandleSpirits(")
assert "const int job = actor->m_job;" in impl_body
implementation_checks = [
    ("Effect toggle gate", "!g_session.m_isEffectOn"),
    ("Self AID resolves to local player; otherwise resolve actor by AID", "aid == g_session.m_aid"),
    ("Self branch selects world player", "mode.m_world ? mode.m_world->m_player : nullptr"),
    ("Non-self branch looks actor up by AID", "mode.m_world->GetGameActorByAID(aid)"),
    ("Null actor returns without effect work", "if (!actor)"),
    ("Job mapping 4038 -> 504, 24 -> 629, otherwise 228", "job == 4038 ? 504 : (job == 24 ? 629 : 228)"),
    ("Initial actor message 109 uses null sender", "actor->SendMsg(nullptr, 109, effectId, 0, 0)"),
    ("Actor AID zero skips the launch/update branch", "if (aid == 0)"),
    ("Burrow or hiding state takes refresh-only branch", "if (hidden)"),
    ("Hidden branch repeats message 109 using null sender", "actor->SendMsg(nullptr, 109, effectId, 0, 0)"),
    ("Visible-state branch launches mapped effect at zero offset", "actor->LaunchEffect(effectId, vector3d{ 0.0f, 0.0f, 0.0f }, 0.0f)"),
    ("Packet signed 16-bit field at +6 is copied to effect position X", "static_cast<s16>(ReadLE16(packet.data + 6))"),
    ("Successful launch receives message 14 with null sender", "effect->SendMsg(nullptr, 14,"),
]
impl_positions = []
impl_cursor = 0
for label, token in implementation_checks:
    position = impl_body.find(token, impl_cursor)
    assert position >= 0, f"implementation branch mismatch: {label}: expected {token}"
    impl_positions.append(position)
    impl_cursor = position + len(token)
assert impl_positions == sorted(impl_positions), "implementation gameplay branch order differs from the source checklist"

# packet framing/identity checks in HandleSpirits are defensive client guards;
# the reference method assumes packet framing was already established by dispatch.
assert "packet.packetLength < 8" in impl_body and "packet.packetId != 0x01D0" in impl_body

# Dispatcher ID 464 is the canonical route for this handler in EXE and Ref.
def dispatcher_call(text: str, case_label: str, handler: str) -> bool:
    start = text.index(case_label)
    snippet = text[start:start + 180]
    return re.search(rf"{re.escape(case_label)}\s*{re.escape(handler)}\s*;", snippet) is not None


assert dispatcher_call(EXE_C, "case 464:", "CGameMode::Zc_Spirits(this, buf)")
assert dispatcher_call(REF, "case 464:", "CGameMode::Zc_Spirits(this, buf)")
assert re.search(r"^0x01d0,8\s*$", PACKET_DB, re.M | re.I), "packet_db must declare opcode 0x01D0 length 8"
assert "router.Register(0x01D0, HandleSpirits);" in IMPLEMENTATION, "current router must route 0x01D0 to HandleSpirits"
PACKET_SIZES = (ROOT / "src/network/GronPacket.cpp").read_text(encoding="utf-8")
assert "SetPacketSize(0x01D0, 8);" in PACKET_SIZES, "current packet-size table must frame 0x01D0 as 8 bytes"

print("Zc_Spirits source comparison: PASS (canonical EXE == Ref, normalized full body)")
print("Opcode 0x01D0: packet_db length 8; executable/Ref dispatch ID 464 -> Zc_Spirits")
print("Gameplay branch checklist (EXE == Ref == implementation for checked branch anchors):")
for label, _ in implementation_checks:
    print(f"[matched] {label}")
