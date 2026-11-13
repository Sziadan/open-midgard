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

if 'constexpr int kButtonIdRemoveOption = 138;' not in ui:
    raise SystemExit("equipment remove-option control id missing")
if 'if (player && (player->m_effectState & kFalconOption) != 0) return "Release Falcon";' not in ui:
    raise SystemExit("equipment remove-option label does not reflect Falcon state")
if 'if (player && (player->m_effectState & kRidingOption) != 0) return "Unmount";' not in ui:
    raise SystemExit("equipment shared remove-option riding precedence missing")
if 'if (IsLocalPlayerPushCartEquipped()) return "Remove cart";' not in ui:
    raise SystemExit("equipment shared remove-option cart fallback missing")
if "case kButtonIdRemoveOption:\n        return g_modeMgr.SendMsg(CGameMode::GameMsg_RequestCartOff, 0, 0, 0);" not in ui:
    raise SystemExit("remove-option control does not issue the shared reference off request")
if "packet.PacketType = profile.cartOff" not in send_profile or "case GameMsg_RequestCartOff:" not in mode:
    raise SystemExit("Falcon off request is not routed through the active packet profile")
if "m_layout.removeOption.x, m_layout.removeOption.y, m_layout.removeOption.width, m_layout.removeOption.height" not in ui:
    raise SystemExit("shared remove-option control is not placed through equipment layout")
if "outData->visible = m_h > kMiniHeight && label;" not in ui:
    raise SystemExit("shared remove-option visibility is not limited to expanded equipment with an active option")
if "outData->label = label ? label : \"\";" not in ui:
    raise SystemExit("Qt equipment control does not publish its dynamic option label")
qt_equipment = (ROOT / "src/qtui/qml/RoEquipment.qml").read_text(encoding="utf-8")
if 'objectName: modelData.id === 138 ? "equipmentRemoveOption"' not in qt_equipment or "visible: modelData.visible; label: modelData.label" not in qt_equipment:
    raise SystemExit("Qt equipment view does not render the shared dynamic remove-option control")
if packet.count("outState.effectState = static_cast<int>(option);") != 4 or packet.count("outState.showEfst = opt3;") != 4:
    raise SystemExit("actor spawn decoders must retain option state and opt3 visibility in their separate fields")

adapter = (ROOT / "src/qtui/QtUiStateAdapter.cpp").read_text(encoding="utf-8")
if 'objectName: "equipmentPreview"' not in qt_equipment or "data.insert(QStringLiteral(\"previewHeight\"), display.preview.height);" not in adapter:
    raise SystemExit("Qt equipment preview does not use the shared equipment layout dimensions")

bootstrap = function(mode, "void EnsureBootstrapSelfActor(CGameMode& mode)")
if "actor->m_effectState = info->effectstate;" not in bootstrap or "actor->SendMsg(actor, 101, 0, 0, 0);" not in bootstrap:
    raise SystemExit("selected-character Falcon state is not materialized during login bootstrap")

print("Falcon login lifecycle and equipment release request parity checks passed")
