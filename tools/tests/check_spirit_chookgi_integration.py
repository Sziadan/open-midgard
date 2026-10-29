from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
packet = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
effect = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
cmake = (ROOT / "tools/tests/CMakeLists.txt").read_text(encoding="utf-8")
fixture = (ROOT / "tools/tests/SpiritChookgiIntegrationTests.cpp").read_text(encoding="utf-8")

assert 'extract_body(spirit_handle_one "${packet_source}" "void HandleSpirits(CGameMode& mode, const PacketView& packet)"' in cmake
assert 'extract_body(spirit_chookgi_emitter "${spirit_chookgi_source}" "void CRagEffect::SpawnChookgi("' in cmake
assert "SpiritChookgiIntegrationTests.cpp" in cmake and "check_spirit_chookgi_integration.py" in cmake
assert "HandleSpirits(mode" in fixture and "effect.SpawnChookgi(variants[row])" in fixture

route_start = effect.index("if (m_type == 228 || m_type == 504 || m_type == 629)", effect.index("bool CRagEffect::ProcessEffectFrame("))
route = effect[route_start:effect.index("return true;", route_start)]
route = re.sub(r"\s+", "", route)
assert "SpawnChookgi(m_type==228?0:(m_type==504?1:3));" in route

spirit_start = packet.index("void HandleSpirits(CGameMode& mode, const PacketView& packet)")
spirit_end = packet.index("void HandleSpirits2(", spirit_start)
spirit = re.sub(r"\s+", "", packet[spirit_start:spirit_end])
assert "job==4038?504:(job==24?629:228)" in spirit
assert "effect->SendMsg(nullptr,14,reinterpret_cast<msgparam_t>(&position),0,0);" in spirit

for claim in ("effect.m_deltaPos.x==counts[row]", "rngDraws-drawStart==emitted*7",
              '"effect\\\\thunder_center.bmp"', "prim.m_renderFlag==5", "gi.height[10]"):
    assert claim in fixture, f"integration fixture missing assertion: {claim}"

print("Spirit-to-CHOOKGI integration source binding: PASS (packet extraction, job map, message 14, production type route, emitter fixture)")
