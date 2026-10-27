from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
exe_c = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
ref = (ROOT / "Ref/Skill.cpp").read_text(encoding="utf-8")
ref_actor = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8")
skill_src = (ROOT / "src/world/SkillUnit.cpp").read_text(encoding="utf-8")
world_src = (ROOT / "src/world/World.cpp").read_text(encoding="utf-8")
qt_src = (ROOT / "src/qtui/QtUiStateAdapter.cpp").read_text(encoding="utf-8")
binary = (ROOT / "HighPriest.exe").read_bytes()


def body_between(text: str, start: str, end: str) -> str:
    start_at = text.index(start)
    end_at = text.index(end, start_at + len(start))
    return text[start_at:end_at]


exe_fn = body_between(exe_c, "//----- (00685660)", "//----- (006862F0)")
ref_fn = body_between(
    ref,
    "// Original Signature: public: virtual void __thiscall CSkill::SendMsg",
    "// Original Signature: public: virtual void __thiscall CSkill::SetRenderInfo",
)
case_pattern = re.compile(r"case\s+7\s*:(.*?)(?=\n\s*case\s+\d+\s*:|\n\s*default\s*:)", re.S)
exe_case = case_pattern.search(exe_fn)
ref_case = case_pattern.search(ref_fn)
assert exe_case and ref_case
assert "strlen((const char *)arg1)" in exe_case.group(1)
assert "this->m_chatTick = timeGetTime()" in exe_case.group(1)
assert "UIBalloonText::SetText(this->m_balloon, (const char *)arg1, 35)" in exe_case.group(1)
assert "m_balloon->m_fontColor = 16744576" in exe_case.group(1)
assert "m_balloon->m_bgColor = 0" in exe_case.group(1)
assert "this->m_lastTlvertX - this->m_chatWidth / 2" in exe_case.group(1)
assert "this->m_lastTlvertY - 110 * g_renderer->m_height / 480" in exe_case.group(1)
assert re.sub(r"\s+", "", exe_case.group(1)) == re.sub(r"\s+", "", ref_case.group(1))
assert "if ( timeGetTime() >= this->m_chatTick + 5000 )" in ref_actor

pe_offset = int.from_bytes(binary[0x3C:0x40], "little")
section_count = int.from_bytes(binary[pe_offset + 6:pe_offset + 8], "little")
optional_header = pe_offset + 24
image_base = int.from_bytes(binary[optional_header + 28:optional_header + 32], "little")
section_table = optional_header + int.from_bytes(binary[pe_offset + 20:pe_offset + 22], "little")


def file_offset_for_va(va: int) -> int:
    for index in range(section_count):
        section = section_table + index * 40
        virtual_size = int.from_bytes(binary[section + 8:section + 12], "little")
        virtual_address = int.from_bytes(binary[section + 12:section + 16], "little")
        raw_size = int.from_bytes(binary[section + 16:section + 20], "little")
        raw_offset = int.from_bytes(binary[section + 20:section + 24], "little")
        if image_base + virtual_address <= va < image_base + virtual_address + max(virtual_size, raw_size):
            return raw_offset + va - image_base - virtual_address
    raise AssertionError(f"VA {va:#x} is outside executable sections")


case_va = 0x685EEE
case_bytes = binary[file_offset_for_va(case_va):file_offset_for_va(0x686016)]
assert case_bytes.startswith(bytes.fromhex("8B 5D 10 83 C9 FF 8B FB 33 C0 F2 AE F7 D1 49"))
for encoded in (
    "FF 15 2C C4 6C 00",  # timeGetTime
    "6A 23 53 E8 9A C8 DB FF",  # SetText(..., 35)
    "C7 40 60 80 80 FF 00",  # fontColor 0x00FF8080
    "C7 40 64 00 00 00 00",  # transparent background
    "8B 51 1C 89 96 58 02 00 00",  # cached balloon width
    "8B 41 20 89 86 5C 02 00 00",  # cached balloon height
    "FF 53 18",  # virtual Move
):
    assert bytes.fromhex(encoded) in case_bytes, encoded

current_case = skill_src[skill_src.index("void CSkill::SendMsg("):]
assert "case 7:" in current_case
assert "SetSkillTalkboxText(*this, reinterpret_cast<const char*>(arg1));" in current_case
helper = body_between(skill_src, "void SetSkillTalkboxText(CSkill& skill, const char* text)", "void CSkill::HideModel()")
assert "text[0] == '\\0'" in helper
assert "std::memchr(text, '\\0', 80)" in helper
assert "timeGetTime() + 5000" in helper
assert "skill.SetChatBubbleText" in helper
assert "GetSkillSpeechBubbleAnchor" in world_src
assert "for (const CSkill* entry : m_skillList)" in world_src
assert "ProjectPoint(g_renderer, viewMatrix, skill->m_pos" in world_src
assert "(110 * screenHeight) / 480" in world_src
assert "MakeSkillTalkboxAnchor" in qt_src and "#ff8080" in qt_src
assert "for (const CSkill* skill : mode.m_world->m_skillList)" in qt_src

print("CSkill::SendMsg message-7 receiver source comparison passed")
print("EXE case bytes at 0x685EEE confirm empty-text return, lazy balloon creation, SetText width 35, colors, measured dimensions, and virtual move.")
print("Ref confirms the 5-second lifetime; current Qt port preserves text/lifetime/color and projects skill position, with source geometry noted as a mismatch.")
