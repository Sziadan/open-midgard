"""Branch comparator for gameplay packet 0x01E1 / Zc_Spirits2."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
REF = (ROOT / "Ref/GameModePacket.cpp").read_text(encoding="utf-8")
REF_ACTOR = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8")
PACKET_DB = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8", errors="replace")
IMPLEMENTATION = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
PACKET_SIZES = (ROOT / "src/network/GronPacket.cpp").read_text(encoding="utf-8")
EFFECT_HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")


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


signature = "void __thiscall CGameMode::Zc_Spirits2(CGameMode *this, const char *buf)"
exe_anchor = EXE.index("//----- (005A8160)")
exe_end = EXE.index("//----- (005A8400)", exe_anchor)
ref_anchor = REF.index("// Original Signature: public: void __thiscall CGameMode::Zc_Spirits2(char const *)")
ref_end = REF.index("// Original Signature:", ref_anchor + 80)
exe_body = body(EXE[exe_anchor:exe_end], signature)
ref_body = body(REF[ref_anchor:ref_end], signature)
assert compact(exe_body) == compact(ref_body), "canonical EXE and Ref Zc_Spirits2 bodies differ"

# GetJob's only special outputs do not include either spirit-specific job ID,
# so the current direct m_job comparisons preserve the source job split.
get_job_signature = "int __thiscall CGameActor::GetJob(CGameActor *this)"
exe_job_anchor = EXE.index("//----- (0055B430)")
exe_job_end = EXE.index("//----- (0055B540)", exe_job_anchor)
ref_job_anchor = REF_ACTOR.index("// Original Signature: public: int __thiscall CGameActor::GetJob(void)")
ref_job_end = REF_ACTOR.index("// Original Signature:", ref_job_anchor + 80)
exe_job = body(EXE[exe_job_anchor:exe_job_end], get_job_signature)
ref_job = body(REF_ACTOR[ref_job_anchor:ref_job_end], get_job_signature)
assert compact(exe_job) == compact(ref_job), "canonical EXE and Ref GetJob bodies differ"
assert "m_job == 4038" not in exe_job and "m_job == 24" not in exe_job

source_anchors = [
    "if ( g_session.m_isEffectOn )",
    "*(_DWORD *)(buf + 2) == g_session.m_aid",
    "if ( v2 )",
    "IsEffectStateBurrow(v2->m_effectState) || IsEffectStateHiding(v2->m_effectState)",
    "if ( !CAbleToMakeEffect::IsEffectExist(v2, 504) )",
    "if ( !CAbleToMakeEffect::IsEffectExist(v2, 629) )",
    "!CAbleToMakeEffect::IsEffectExist(v2, 228)",
    "if ( *(_DWORD *)(buf + 2) )",
    "memset(&pos, 0, sizeof(pos))",
    "CAbleToMakeEffect::LaunchEffect(v2, 504, v4[0], 0.0)",
    "pos.x = (float)*((__int16 *)buf + 3)",
    "SendMsg(v3, 0, 14, (int)&pos, 0, 0)",
]
for anchor in source_anchors:
    assert anchor in exe_body and anchor in ref_body, f"missing source branch anchor: {anchor}"

impl_body = body(IMPLEMENTATION, "void HandleSpirits2(")
assert "const int job = actor->m_job;" in impl_body
implementation_anchors = [
    "!g_session.m_isEffectOn",
    "ResolveSkillPacketActor(mode, aid)",
    "if (!actor)",
    "job == 4038 ? 504 : (job == 24 ? 629 : 228)",
    "if (hidden)",
    "actor->SendMsg(nullptr, 109, effectId, 0, 0)",
    "if (!EffectExists(actor, effectId) && aid != 0)",
    "vector3d position{}",
    "CRagEffect* effect = actor->LaunchEffect(effectId, offset, 0.0f)",
    "static_cast<s16>(ReadLE16(packet.data + 6))",
    "effect->SendMsg(nullptr, 14, reinterpret_cast<msgparam_t>(&position), 0, 0)",
]
cursor = 0
for anchor in implementation_anchors:
    found = impl_body.find(anchor, cursor)
    assert found >= 0, f"implementation branch/order mismatch: {anchor}"
    cursor = found + len(anchor)
assert "packet.packetLength < 8" in impl_body and "packet.packetId != 0x01E1" in impl_body

# IsEffectExist's source is a null-safe linked-list search comparing each
# attached effect's type. EffectExists uses the same list and getter.
exist_signature = "int __thiscall CAbleToMakeEffect::IsEffectExist(CAbleToMakeEffect *this, int EffectId)"
exe_exist_anchor = EXE.index("//----- (00559FD0)")
exe_exist_end = EXE.index("//----- (0055A010)", exe_exist_anchor)
ref_exist_anchor = REF_ACTOR.index("// Original Signature: public: int __thiscall CAbleToMakeEffect::IsEffectExist(int)")
ref_exist_end = REF_ACTOR.index("// Original Signature:", ref_exist_anchor + 80)
exe_exist = body(EXE[exe_exist_anchor:exe_exist_end], exist_signature)
ref_exist = body(REF_ACTOR[ref_exist_anchor:ref_exist_end], exist_signature)
assert compact(exe_exist) == compact(ref_exist), "canonical EXE and Ref IsEffectExist bodies differ"
exist_current_signature = "bool EffectExists(CAbleToMakeEffect* actor, int effectId)"
current_exist = body(IMPLEMENTATION[IMPLEMENTATION.rindex(exist_current_signature):], exist_current_signature)
for token in ("if (actor == nullptr)", "for (const CRagEffect* effect : actor->m_effectList)",
              "effect != nullptr && effect->GetEffectType() == effectId", "return true;", "return false;"):
    assert token in current_exist, f"current effect-existence helper is missing source behavior: {token}"
assert "int GetEffectType() const { return m_type; }" in EFFECT_HEADER

def dispatches(text: str, handler: str) -> bool:
    return re.search(r"case\s+481:\s*" + re.escape(handler), text) is not None


assert dispatches(EXE, "CGameMode::Zc_Spirits2(this, buf)")
assert dispatches(REF, "CGameMode::Zc_Spirits2(this, buf)")
assert re.search(r"^0x01e1,8\s*$", PACKET_DB, re.M | re.I)
assert "SetPacketSize(0x01E1, 8);" in PACKET_SIZES
assert "router.Register(0x01E1, HandleSpirits2);" in IMPLEMENTATION

print("Zc_Spirits2 source comparison: PASS (canonical EXE == Ref, normalized full body)")
print("IsEffectExist comparison: PASS (null-safe effect-list search by effect type)")
print("Opcode 0x01E1: dispatch 481 and 8-byte packet framing agree across source/current")
print(f"Gameplay branch comparison: PASS ({len(implementation_anchors)} ordered anchors)")
