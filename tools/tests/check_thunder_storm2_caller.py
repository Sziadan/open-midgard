from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def body(text: str, start_marker: str, end_marker: str) -> str:
    start = text.index(start_marker)
    return text[start:text.index(end_marker, start + len(start_marker))]


def ordered(text: str, fragments: list[str]) -> bool:
    cursor = 0
    for item in fragments:
        found = text.find(item, cursor)
        if found < 0:
            return False
        cursor = found + len(item)
    return True


readable_header = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="latin1")
enum_tail = body(readable_header, "EF_STIN4,", "};")
enum_names = [part.strip() for part in re.split(r"[,\n]", enum_tail) if part.strip()]
effect_id = 621
for name in enum_names:
    if name == "EF_THUNDERSTORM2":
        break
    effect_id += 1
else:
    raise AssertionError("EF_THUNDERSTORM2 missing after EF_STIN4")

readable_dispatch = body((ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="latin1"),
                         "EF_THUNDERSTORM2:", "EF_STIN4:")
canonical_text = (ROOT / "HighPriest.exe.c").read_text(encoding="latin1")
canonical_dispatch = body(canonical_text, "case 0x26E:", "case 0x26F:")
readable_emitter = body((ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="latin1"),
                        "void\tCRagEffect :: ThunderStorm2()", "void\tCRagEffect :: FireArrow()")
canonical_emitter = body(canonical_text,
                         "void __thiscall CRagEffect::ThunderStorm2(CRagEffect *this)\n{",
                         "void __thiscall CRagEffect::FireArrow(CRagEffect *this)\n{")
current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
current_dispatch = body(current, "if (m_type == 622) {", "if (m_type == 618) {")
current_emitter = body(current, "void CRagEffect::ApplyThunderStorm2()", "void CRagEffect::ApplyBottomMagnus(")
particle_routes = body(current, "bool CEffectPrim::UsesReferenceParticle() const", "CEffectPrim::~CEffectPrim()")
source_render_dispatch = body((ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="latin1"),
                              "PP_3DPARTICLE_NOMASTER:", "PP_3DQUADHORN:")

assert effect_id == 622, f"readable enum resolves EF_THUNDERSTORM2={effect_id}, expected 622"
assert "case 0x26E:" in canonical_text
assert ordered(readable_dispatch, ["ThunderStorm2();"])
assert ordered(canonical_dispatch, ["CRagEffect::ThunderStorm2(this);", "return 1;"])
assert ordered(current_dispatch, ["ApplyThunderStorm2();", "return true;"])
assert ordered(readable_emitter, ["m_pos = m_master->m_pos", "if (m_stateCnt == 0)",
                                  'PlayWave("effect\\\\EF_ThunderStorm.wav"',
                                  "LaunchEffectPrim(PP_3DPARTICLE)", "m_duration", "RF_NODEPTHCHECK",
                                  "PT_USEORGARGB", "m_deltaPos2.y = -4.5f", "m_animSpeed = 2",
                                  "m_size = 2.5f", 'SetSprName("misc\\\\thunder_storm.spr")',
                                  'SetActName("misc\\\\thunder_storm.act")', "InitSegment()"])
assert ordered(canonical_emitter, ["this->m_pos = this->m_master->m_pos", "if ( !this->m_stateCnt )",
                                    "PlayWave(aEffectEfThunde", "LaunchEffectPrim(this, PP_3DPARTICLE",
                                    "m_duration = this->m_duration", "m_renderFlag |= 0x200u",
                                    "m_pattern |= 0x200u", "m_deltaPos2.y = -4.5",
                                    "m_animSpeed = 2", "m_size = 2.5", "SetSprName(g_prim",
                                    "SetActName(g_prim", "InitSegment(g_prim)"])
assert ordered(current_emitter, ["m_effectPos = master->m_pos", "if (m_stateCnt != 0) return",
                                  'PlayWave("effect\\\\EF_ThunderStorm.wav"',
                                  "LaunchEffectPrim(PP_3DPARTICLE", "prim->m_duration = m_duration",
                                  "prim->m_renderFlag |= 0x200", "prim->m_pattern |= 0x200",
                                  "prim->m_deltaPos2.y = -4.5f", "prim->m_animSpeed = 2",
                                  "prim->m_size = 2.5f", "prim->m_numSegments = 1",
                                  'SetParticleResources("misc\\\\thunder_storm.spr", "misc\\\\thunder_storm.act")',
                                  "InitReferenceSegments(prim)"])
assert "622" in particle_routes, "effect 622 must use the existing animated sprite-particle path"
assert "PP_3DPARTICLE:" in (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="latin1")
assert "Render3DParticle(wtm);" in source_render_dispatch
assert current.count('"effect\\\\EF_ThunderStorm.wav"') >= 1
assert current.count('"misc\\\\thunder_storm.spr"') >= 1
assert current.count('"misc\\\\thunder_storm.act"') >= 1
assert "rand(" not in readable_emitter and "rand(" not in canonical_emitter and "rand(" not in current_emitter
assert (ROOT / "Ref/GRF-Content/data/wav/effect/ef_thunderstorm.wav").exists()

print("ThunderStorm2 source/canonical/current dispatcher, caller, resources, particle flags, and render-route comparisons passed (case 622 / 0x26E; no RNG).")
