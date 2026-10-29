from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def body(text: str, start_marker: str, end_marker: str) -> str:
    start = text.index(start_marker)
    return text[start:text.index(end_marker, start + len(start_marker))]


def ordered(text: str, parts: list[str]) -> bool:
    pos = 0
    for part in parts:
        found = text.find(part, pos)
        if found < 0:
            return False
        pos = found + len(part)
    return True


header = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="latin1")
enum_tail = body(header, "EF_THUNDERSTORM2,", "};")
names = [item.strip() for item in re.split(r"[,\n]", enum_tail) if item.strip()]
effect_id = 622
for name in names:
    if name == "EF_STIN5":
        break
    effect_id += 1
else:
    raise AssertionError("EF_STIN5 missing from readable enum")

readable_text = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="latin1")
readable_dispatch = body(readable_text, "EF_STIN5:", "EF_CHEMICAL4:")
canonical_text = (ROOT / "HighPriest.exe.c").read_text(encoding="latin1")
canonical_dispatch = body(canonical_text, "case 0x270:", "case 0x271:")
readable_emitter = body((ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="latin1"),
                        "void\tCRagEffect :: STIN5(char *tName)", "void\tCRagEffect :: STIN2(")
canonical_emitter = body(canonical_text,
                         "void __thiscall CRagEffect::STIN5(CRagEffect *this, char *tName)\n{",
                         "void __thiscall CRagEffect::STIN2(CRagEffect *this, char *tName, char F1)\n{")
current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
current_route = body(current, "if (m_type == 624) {", "if (m_type == 618) {")
current_caller = body(current, "void CRagEffect::ApplyStin5()", "void CRagEffect::SpawnStin5(")
current_emitter = body(current, "void CRagEffect::SpawnStin5(", "// TEST_BODY_END stin5_emitter_family")

assert effect_id == 624, f"readable enum resolves EF_STIN5={effect_id}, expected 624"
assert "if (m_stateCnt == 1)" in readable_dispatch
assert ordered(readable_dispatch, ['PlayWave("effect\\\\stin5.wav"', 'STIN5("effect\\\\wind.tga")'])
assert ordered(canonical_dispatch, ["if ( this->m_stateCnt == 1 )", "PlayWave(aEffectT_17",
                                    "CRagEffect::STIN5(this, aEffect270);", "return 1;"])
assert ordered(current_route, ["ApplyStin5();", "return true;"])
assert ordered(current_caller, ["m_effectPos = master->m_pos", "if (m_stateCnt == 1)",
                                'PlayWave("effect\\\\stin5.wav"',
                                'SpawnStin5("effect\\\\wind.tga")'])
assert ordered(readable_emitter, ["m_pos = m_master->m_pos", "if (m_stateCnt < 5)",
                                  "LaunchEffectPrim(PP_STIN5)", "RF_EFFECT", "m_duration",
                                  "m_totalTexture = 1", "GetTexture(tName)", "m_size=0",
                                  "m_deltaPos = m_deltaPos", "m_pos = m_pos",
                                  "for (ec=0;ec<4;ec++)", "life = 1", "process= -3+ec",
                                  "distance = 20.0f", "alphaB = 0", "if (m_stateCnt == 0)",
                                  "else if (m_stateCnt == 1)", "else if (m_stateCnt == 2)",
                                  "else if (m_stateCnt == 3)", "vecB_now.y = m_pos.y-10.0f+random(5)",
                                  "RotStart=random(360)", "rise_angle=ec", "height[1] = 1.5f",
                                  "flag1[0] = 0", "flag1[3] = 1"])
assert ordered(canonical_emitter, ["this->m_pos = this->m_master->m_pos", "if ( this->m_stateCnt < 5 )",
                                    "LaunchEffectPrim(this, PP_STIN5", "m_renderFlag = 5",
                                    "m_duration = this->m_duration", "m_totalTexture = 1",
                                    "GetTexture(&g_texMgr, tName, 0)", "m_size = 0.0",
                                    "m_deltaPos = this->m_deltaPos", "m_pos = this->m_pos",
                                    "while ( v3 < 4 )"])
for branch in ["life = 1", "process = v4 - 3", "distance = 20.0", "alphaB = 0",
               "m_stateCnt == 1", "m_stateCnt != 2", "m_stateCnt == 3",
               "rand() % 5", "rand() % 360", "m_rollAccel", "v4++", "m_radiusSpeed",
               "m_deltaPos2.z + v3 * 180) = 0", "m_deltaPos2.z + v3 * 180 + 3) = 1", "< 5"]:
    assert branch in canonical_emitter, f"canonical emitter branch missing: {branch}"
assert ordered(current_emitter, ["m_effectPos = master->m_pos", "if (m_stateCnt >= 5) return",
                                  "LaunchEffectPrim(PP_STIN5", "prim->m_renderFlag = 5",
                                  "prim->m_duration = m_duration", "prim->m_totalTexture = 1",
                                  "GetTexture(textureName", "prim->m_size = 0.0f",
                                  "prim->m_deltaPos = m_deltaPos", "prim->m_pos = m_effectPos",
                                  "lane < 4", "gi.life = 1", "gi.process = -3 + lane",
                                  "gi.distance = 20.0f", "gi.alphaB = 0", "m_stateCnt == 0",
                                  "m_stateCnt == 1", "m_stateCnt == 2", "m_stateCnt == 3",
                                  "rand() % 5", "rand() % 360", "gi.riseAngle", "gi.height[1] = 1.5f",
                                  "gi.flag[0] = 0", "gi.flag[3] = 1"])
assert current_emitter.count("rand() %") == 2, "STIN5 consumes two random draws per lane"

print("STIN5 source/canonical/current dispatcher, state branches, lane payload, cadence, resources, and RNG comparisons passed (case 624 / 0x270).")
