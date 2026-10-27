"""Source and ordered-branch comparator for gameplay packet 0x01CF."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
REF = (ROOT / "Ref/GameModePacket.cpp").read_text(encoding="utf-8")
PACKET_DB = (ROOT / "Ref/RunningServer/packet_db.txt").read_text(encoding="utf-8", errors="replace")
CLIF = (ROOT / "Ref/eAthena_src_2011/map/clif.c").read_text(encoding="utf-8", errors="replace")
IMPLEMENTATION = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
PACKET_SIZES = (ROOT / "src/network/GronPacket.cpp").read_text(encoding="utf-8")


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


signature = "void __thiscall CGameMode::Zc_DevotionList(CGameMode *this, const char *buf)"
exe_start = EXE.index("//----- (005A7E30)")
exe_end = EXE.index("//----- (005A7F30)", exe_start)
ref_start = REF.index("// Original Signature: public: void __thiscall CGameMode::Zc_DevotionList(char const *)")
ref_end = REF.index("// Original Signature:", ref_start + 80)
exe_body = body(EXE[exe_start:exe_end], signature)
ref_body = body(REF[ref_start:ref_end], signature)
assert compact(exe_body) == compact(ref_body), "canonical EXE and Ref Zc_DevotionList bodies differ"

source_anchors = [
    "if ( g_session.m_isEffectOn )",
    "*(_DWORD *)(buf + 2) == g_session.m_aid",
    "if ( m_player )",
    "v3 = 5;",
    "SendMsg(m_player, 0, 109, 232, 0, 0)",
    "v14 = 5",
    "v4 = buf + 6",
    "v5 = *(_DWORD *)v4",
    "if ( *(_DWORD *)v4 )",
    "pos.x = (float)v13",
    "pos.y = (float)v5",
    "if ( m_player )",
    "LaunchEffect(m_player, 232, v7[0], 0.0)",
    "SendMsg(v6, 0, 14, (int)&pos, 0, 0)",
    "v4 += 4",
    "while ( v14 )",
]
for token in source_anchors:
    assert token in exe_body and token in ref_body, f"missing source branch: {token}"

impl_body = body(IMPLEMENTATION, "void HandleDevotionList(")
implementation_anchors = [
    "!g_session.m_isEffectOn",
    "devoterAid == g_session.m_aid",
    "mode.m_world ? mode.m_world->m_player : nullptr",
    "mode.m_world->GetGameActorByAID(devoterAid)",
    "if (devoter)",
    "for (int index = 0; index < 5; ++index)",
    "devoter->SendMsg(nullptr, 109, 232, 0, 0)",
    "const u32 devoteeAid = ReadLE32(packet.data + 6 + index * 4)",
    "if (devoteeAid == 0)",
    "static_cast<float>(devoterAid), static_cast<float>(devoteeAid), 0.0f",
    "if (devoter)",
    "devoter->LaunchEffect(232, offset, 0.0f)",
    "effect->SendMsg(nullptr, 14, reinterpret_cast<msgparam_t>(&position), 0, 0)",
]
cursor = 0
for token in implementation_anchors:
    found = impl_body.find(token, cursor)
    assert found >= 0, f"implementation branch/order mismatch: {token}"
    cursor = found + len(token)
assert "packet.packetLength < 28" in impl_body and "packet.packetId != 0x01CF" in impl_body
assert "packet.data + 26" not in impl_body, "source ignores max distance at packet +26"

assert re.search(r"case\s+463:\s*CGameMode::Zc_DevotionList\(this, buf\)", EXE)
assert re.search(r"case\s+463:\s*CGameMode::Zc_DevotionList\(this, buf\)", REF)
assert re.search(r"^0x01cf,28\s*$", PACKET_DB, re.M | re.I)
assert "01cf <devoter id>.L { <devotee id>.L }*5 <max distance>.W" in CLIF
assert "SetPacketSize(0x01CF, 28);" in PACKET_SIZES
assert "router.Register(0x01CF, HandleDevotionList);" in IMPLEMENTATION

print("Zc_DevotionList source comparison: PASS (canonical EXE == Ref, normalized full body)")
print("Opcode 0x01CF: dispatch 463; packet length 28; devoter +2, five devotee IDs +6..+22; max distance +26 unused")
print(f"Gameplay branch comparison: PASS ({len(implementation_anchors)} ordered anchors)")
