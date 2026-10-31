"""Check login Falcon reconciliation and equipment-window release semantics."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def function(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(f"unterminated function: {signature}")

packet = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
ui = (ROOT / "src/ui/UIEquipWnd.cpp").read_text(encoding="utf-8")
mode = (ROOT / "src/gamemode/GameMode.cpp").read_text(encoding="utf-8")
send_profile = (ROOT / "src/network/MapSendProfile.cpp").read_text(encoding="utf-8")
ref_actor = (ROOT / "Ref/GameActor.cpp").read_text(encoding="latin-1")
server = (ROOT / "Ref/eAthena_src_2011/map/clif.c").read_text(encoding="latin-1")

refresh = function(packet, "void RefreshActorEffectStatePresentation(CGameActor* actor, int oldEffectState)")
for token in ("kEffectStateFalconMask = 0x0010", "hasFalcon && !suppressFalcon && !actor->m_birdEffect",
              "actor->SendMsg(actor, 101, 0, 0, 0)", "(!hasFalcon || suppressFalcon) && actor->m_birdEffect",
              "actor->SendMsg(actor, 102, 0, 0, 0)"):
    if token not in packet:
        raise SystemExit(f"runtime Falcon state reconciliation missing {token}")

if "case 101:" not in ref_actor or "case 102:" not in ref_actor:
    raise SystemExit("Ref actor Falcon create/remove messages missing")
if "pc_setoption(sd,sd->sc.option&~(OPTION_CART|OPTION_RIDING|OPTION_FALCON))" not in server:
    raise SystemExit("Reference CZ_REQ_CARTOFF behavior no longer clears Falcon option")

if 'constexpr int kButtonIdFalconOff = 138;' not in ui:
    raise SystemExit("equipment Falcon button id missing")
if 'outData->label = "Release Falcon"' not in ui or 'IsLocalPlayerFalconEquipped()' not in ui:
    raise SystemExit("equipment Falcon button visibility/label missing")
if "kButtonIdFalconOff:\n        return g_modeMgr.SendMsg(CGameMode::GameMsg_RequestCartOff, 0, 0, 0);" not in ui:
    raise SystemExit("Falcon button does not issue the shared reference off request")
if "packet.PacketType = profile.cartOff" not in send_profile or "case GameMsg_RequestCartOff:" not in mode:
    raise SystemExit("Falcon off request is not routed through the active packet profile")
if "outData->y = m_y + 186" not in ui:
    raise SystemExit("Release Falcon button is not placed beneath the preview sprite")
if packet.count("effectState = static_cast<int>(option | opt3);") != 5:
    raise SystemExit("actor spawn decoders must preserve option and opt3 flags together")

qml = (ROOT / "src/qtui/qml/GameOverlay.qml").read_text(encoding="utf-8")
if "parent.height - y - 49" not in qml:
    raise SystemExit("Qt equipment preview does not reserve the Falcon button strip")

bootstrap = function(mode, "void EnsureBootstrapSelfActor(CGameMode& mode)")
if "actor->m_effectState = info->effectstate;" not in bootstrap or "actor->SendMsg(actor, 101, 0, 0, 0);" not in bootstrap:
    raise SystemExit("selected-character Falcon state is not materialized during login bootstrap")

print("Falcon login lifecycle and equipment release request parity checks passed")
