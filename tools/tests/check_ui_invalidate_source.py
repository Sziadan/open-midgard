from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
binary = (ROOT / "HighPriest.exe").read_bytes()
exe_c = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
ref_uisys = (ROOT / "Ref/UISys.cpp").read_text(encoding="utf-8")
ref_mgr = (ROOT / "Ref/UIWindowMgr.cpp").read_text(encoding="utf-8")
ref_session = (ROOT / "Ref/Session.cpp").read_text(encoding="utf-8")
impl_ui = (ROOT / "src/ui/UIWindow.cpp").read_text(encoding="utf-8")
impl_mgr = (ROOT / "src/ui/UIWindowMgr.cpp").read_text(encoding="utf-8")
impl_session = (ROOT / "src/session/Session.cpp").read_text(encoding="utf-8")


def function_body(text: str, signature: str) -> str:
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
    raise AssertionError(f"unterminated function: {signature}")


def normalized(body: str) -> str:
    return re.sub(r"\s+", "", body)


pe = int.from_bytes(binary[0x3C:0x40], "little")
section_count = int.from_bytes(binary[pe + 6:pe + 8], "little")
optional = pe + 24
image_base = int.from_bytes(binary[optional + 28:optional + 32], "little")
section_table = optional + int.from_bytes(binary[pe + 20:pe + 22], "little")


def offset_for_va(va: int) -> int:
    for index in range(section_count):
        section = section_table + index * 40
        virtual_size = int.from_bytes(binary[section + 8:section + 12], "little")
        virtual_address = int.from_bytes(binary[section + 12:section + 16], "little")
        raw_size = int.from_bytes(binary[section + 16:section + 20], "little")
        raw_offset = int.from_bytes(binary[section + 20:section + 24], "little")
        if image_base + virtual_address <= va < image_base + virtual_address + max(virtual_size, raw_size):
            return raw_offset + va - image_base - virtual_address
    raise AssertionError(f"VA {va:#x} is outside executable sections")


functions = (
    (
        "UIWindow::InvalidateChildren",
        0x4D7660,
        bytes.fromhex("56 57 8B F9 8B 07 C7 47 24 01 00 00 00 FF 50 08 8B 47 0C 8B 30 3B F0 74 11 8B 4E 08 E8 DF FF FF FF 8B 36 8B 47 0C 3B F0 75 EF 5F 5E C3 90 90 90"),
        "void __thiscall UIWindow::InvalidateChildren(UIWindow *this)",
        ref_uisys,
        "void UIWindow::InvalidateChildren()",
        impl_ui,
    ),
    (
        "UIWindowMgr::InvalidateUpdateNeededUI",
        0x4DD900,
        bytes.fromhex("56 57 8B F9 8B 87 C8 02 00 00 85 C0 75 30 C7 87 C8 02 00 00 01 00 00 00 8B 47 38 8B 30 3B F0 74 1D 8B 4E 08 8B 01 FF 50 14 84 C0 74 08 8B 4E 08 E8 2B 9D FF FF 8B 36 8B 47 38 3B F0 75 E3 5F 5E C3 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90"),
        "void __thiscall UIWindowMgr::InvalidateUpdateNeededUI(UIWindowMgr *this)",
        ref_mgr,
        "void UIWindowMgr::InvalidateUpdateNeededUI()",
        impl_mgr,
    ),
    (
        "CSession::InvalidateUI",
        0x661470,
        bytes.fromhex("B9 58 87 76 00 E9 86 C4 E7 FF 90 90 90 90 90 90"),
        "void __thiscall CSession::InvalidateUI(CSession *this)",
        ref_session,
        "void CSession::InvalidateUI()",
        impl_session,
    ),
)

for name, va, expected_bytes, ref_signature, ref_source, impl_signature, impl_source in functions:
    actual_bytes = binary[offset_for_va(va):offset_for_va(va) + len(expected_bytes)]
    assert actual_bytes == expected_bytes, f"{name}: executable bytes differ: {actual_bytes.hex(' ')}"
    ref_body = function_body(ref_source, ref_signature)
    impl_body = function_body(impl_source, impl_signature)
    assert ref_body, f"{name}: missing Ref body"
    assert impl_body, f"{name}: missing implementation body"

# The EXE decompilation for these three functions is also compared directly
# with Ref; exact PE bytes above anchor those bodies to the canonical binary.
for name, va, expected_bytes, ref_signature, ref_source, impl_signature, impl_source in functions:
    marker = f"//----- ({va:08X})"
    exe_start = exe_c.index(marker)
    exe_end = exe_c.find("\n//----- (", exe_start + len(marker))
    if exe_end < 0:
        exe_end = len(exe_c)
    exe_body = function_body(exe_c[exe_start:exe_end], ref_signature)
    ref_body = function_body(ref_source, ref_signature)
    assert normalized(exe_body) == normalized(ref_body), f"{name}: EXE decompilation differs from Ref"

# Compare control-flow effects from the executable/Ref and implementation.
checks = (
    (ref_uisys, impl_ui, "InvalidateChildren", ("m_isDirty=1", "InvalidateWallPaper()", "InvalidateChildren(Next->_Value)", "child->InvalidateChildren()")),
    (ref_mgr, impl_mgr, "InvalidateUpdateNeededUI", ("IsUpdateNeed", "IsUpdateNeed", "InvalidateChildren", "InvalidateChildren")),
    (ref_session, impl_session, "InvalidateUI", ("InvalidateUpdateNeededUI(&g_windowMgr)", "g_windowMgr.InvalidateUpdateNeededUI()")),
)
for ref_source, impl_source, name, tokens in checks:
    full_name = next(f[0] for f in functions if f[0].endswith(name))
    ref_body = function_body(ref_source, next(f[3] for f in functions if f[0] == full_name))
    impl_body = function_body(impl_source, next(f[5] for f in functions if f[0] == full_name))
    # Each tuple contains the corresponding token in both source spellings.
    assert len(tokens) % 2 == 0
    for ref_token, impl_token in zip(tokens[::2], tokens[1::2]):
        assert normalized(ref_token) in normalized(ref_body), f"{name}: missing Ref branch/effect {ref_token}"
        assert normalized(impl_token) in normalized(impl_body), f"{name}: missing implementation branch/effect {impl_token}"

manager_ref = function_body(ref_mgr, "void __thiscall UIWindowMgr::InvalidateUpdateNeededUI(UIWindowMgr *this)")
manager_impl = function_body(impl_mgr, "void UIWindowMgr::InvalidateUpdateNeededUI()")
assert "!this->m_isInvalidatedByForce" in manager_ref
assert "if (m_isInvalidatedByForce)" in manager_impl and "return;" in manager_impl
assert "this->m_isInvalidatedByForce = 1" in manager_ref
assert "m_isInvalidatedByForce = true" in manager_impl

print("EXE bytes, Ref bodies, and implementation effects match for UI invalidation functions")
