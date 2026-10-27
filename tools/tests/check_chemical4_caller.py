from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def body(text: str, start: str, end: str) -> str:
    start_pos = text.index(start)
    return text[start_pos:text.index(end, start_pos + len(start))]


def ordered(text: str, fragments: list[str]) -> bool:
    offset = 0
    for fragment in fragments:
        found = text.find(fragment, offset)
        if found < 0:
            return False
        offset = found + len(fragment)
    return True


header = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="latin1")
enum = body(header, "EF_THUNDERSTORM2,", "};")
names = [part.strip() for part in re.split(r"[,\n]", enum) if part.strip()]
effect_id = 622
for name in names:
    if name == "EF_CHEMICAL4":
        break
    effect_id += 1
else:
    raise AssertionError("EF_CHEMICAL4 missing after EF_THUNDERSTORM2")

readable = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="latin1")
readable_caller = body(readable, "EF_CHEMICAL4:", "EF_THUNDERSTORM2:")
readable_emitter = body((ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="latin1"),
                        "void\tCRagEffect :: CHEMICAL3(short time)", "void\tCRagEffect :: CALLZONE(")
canonical = (ROOT / "HighPriest.exe.c").read_text(encoding="latin1")
canonical_caller = body(canonical, "case 0x26F:", "case 0x270:")
canonical_emitter = body(canonical,
                         "void __thiscall CRagEffect::CHEMICAL3(CRagEffect *this, __int16 time)\n{",
                         "void __thiscall CRagEffect::CALLZONE(CRagEffect *this, char *tName)\n{")
current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
current_route = body(current, "} else if (chemicalSlash) {", "} else if (blastMine) {")
current_emitter = body(current, "void CRagEffect::ApplyChemical3(short time)", "void CRagEffect::ApplyChemicalProtection(")

assert effect_id == 623, f"readable enum resolves EF_CHEMICAL4={effect_id}, expected 623"
assert ordered(readable_caller, ["m_stateCnt == 28", "m_pos = m_master->m_pos",
                                  'PlayWave("effect\\\\chemical4.wav"',
                                  "for (int i=0;i<4;i++)", "CHEMICAL3(20);", "CHEMICAL3(21);"])
assert ordered(canonical_caller, ["this->m_stateCnt == 28", "this->m_pos = this->m_master->m_pos",
                                   "PlayWave(aEffect_140", "v33 = 4", "CHEMICAL3(this, 20)",
                                   "CHEMICAL3(this, 21)", "--v33", "while ( v33 )"])
assert ordered(current_route, ["m_type == 623 && m_stateCnt == 28", '"effect\\\\chemical4.wav"',
                                "m_type == 623 ? 20", "repeat < 4", "ApplyChemical3(slashTime)",
                                "ApplyChemical3(static_cast<short>(slashTime + 1))"])

readable_emitter_order = ["m_pos = m_master->m_pos", "if (m_stateCnt == 0)",
                                   "LaunchEffectPrim(PP_CHEMICAL3)", "RF_UI", "m_duration",
                                   "time<10", 'GetTexture("effect\\\\line3.tga")',
                                   'GetTexture("effect\\\\line3y.tga")',
                                   "for (ec=0;ec<4;ec++)", "life=0", "life=1", "process=0",
                                   "distance=100.0f", "time%10 == 0", "height[0]=-15.0f-random(11)",
                                   "time%10 == 1", "height[0]=15.0f+random(11)", "height[1]=random(200)",
                                   "max_height=0.3f+(random(4)*0.1f)", "time<20",
                                   "max_height=0.5f+(random(6)*0.1f)",
                                   "max_height=0.4f+(random(4)*0.1f)",
                                   "Get_Angle(Rx,Rz", "alphaB=0"]
assert ordered(readable_emitter, readable_emitter_order), [(part, readable_emitter.find(part)) for part in readable_emitter_order]
canonical_emitter_order = ["this->m_pos = this->m_master->m_pos", "if ( this->m_stateCnt )",
                                    "LaunchEffectPrim(this, PP_CHEMICAL3", "m_renderFlag = 513",
                                    "m_duration = this->m_duration", "time >= 10", "aEffectLine3yTg",
                                    "aEffectLine3Tga", "time % 10", "rand() % 11", "rand() % 200",
                                    "if ( v3 >= 10 )", "if ( v3 < 20 )", "rand() % 6", "rand() % 31",
                                    "rand() % 3 + 10", "rand() % 11", "goto LABEL_20", "v12 = rand() % 4",
                                    "rand() % 31", "rand() % 3 + 20", "rand() % 21", "v9 = rand() % 4",
                                    "rand() % 31", "rand() % 3;", "rand() % 21", "LABEL_20:",
                                    "height[3] = v11", 
                                    "this->m_deltaPos.z - this->m_pos.z", "this->m_deltaPos.x - this->m_pos.x",
                                    "if ( v7 < 4 )", "continue;"]
assert ordered(canonical_emitter, canonical_emitter_order), [(part, canonical_emitter.find(part)) for part in canonical_emitter_order]
assert ordered(current_emitter, ["m_effectPos = origin", "if (m_stateCnt != 0)",
                                  "LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeChemical3)",
                                  "prim->m_renderFlag = 513u", "prim->m_duration = m_duration",
                                  "time >= 10", '"effect\\\\line3y.tga"', '"effect\\\\line3.tga"',
                                  "Chemical2RiseAngle(m_deltaPos.x - origin.x, m_deltaPos.z - origin.z)",
                                  "band < 4", "gi.life = 1", "gi.process = 0", "gi.distance = 100.0f",
                                  "gi.alphaB = 0", "gi.riseAngle = rise", "digit == 0", "rand() % 11",
                                  "digit == 1", "rand() % 200", "time >= 10 && time < 20", "rand() % 6",
                                  "rand() % 31", "rand() % 3 + 10", "rand() % 11", "time >= 20",
                                  "rand() % 4", "rand() % 31", "rand() % 3 + 20", "rand() % 21",
                                  "rand() % 3)", "rand() % 21"]) 
print("EF_CHEMICAL4 caller and CHEMICAL3 emitter source/canonical/current branch comparisons passed (case 623 / 0x26F; exact audio path; 20/21 cadence).")
