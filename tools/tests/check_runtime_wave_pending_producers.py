#!/usr/bin/env python3
"""Compare remaining runtime-wave producers to canonical EXE bytes and Ref."""
from __future__ import annotations

import hashlib
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path: str, encoding: str = "utf-8") -> str:
    return (ROOT / path).read_text(encoding=encoding, errors="replace")


def body(text: str, signature: str, label: str) -> str:
    start = text.rindex(signature)
    opening = text.index("{", start)
    depth = 0
    for end in range(opening, len(text)):
        if text[end] == "{":
            depth += 1
        elif text[end] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1 : end]
    raise AssertionError(f"unterminated body: {label}")


def normalize(text: str) -> str:
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"\s+", " ", text).strip()


def va_bytes(va: int, length: int) -> bytes:
    """Map a PE32 virtual address to its raw file bytes."""
    image = (ROOT / "HighPriest.exe").read_bytes()
    assert image[:2] == b"MZ", "canonical executable is not an MZ image"
    nt = struct.unpack_from("<I", image, 0x3C)[0]
    assert image[nt : nt + 4] == b"PE\0\0", "canonical executable has no PE signature"
    section_count = struct.unpack_from("<H", image, nt + 6)[0]
    optional_size = struct.unpack_from("<H", image, nt + 20)[0]
    optional = nt + 24
    image_base = struct.unpack_from("<I", image, optional + 28)[0]
    rva = va - image_base
    section_table = optional + optional_size
    for index in range(section_count):
        section = section_table + index * 40
        virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from("<IIII", image, section + 8)
        if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
            offset = raw_offset + rva - virtual_address
            return image[offset : offset + length]
    raise AssertionError(f"VA 0x{va:X} is not backed by a PE section")


exe = read("HighPriest.exe.c")
current = read("src/world/RagEffect.cpp")
source_specs = [
    (
        "FireArrow",
        "Ref/RagEffect.cpp",
        "void __thiscall CRagEffect::FireArrow(CRagEffect *this)",
        "0x5F963C",
        "void CRagEffect::ApplyFireArrow()",
        0x71B714,
        b"effect\\EF_FireArrow%d.wav\0",
        ("if ( this->m_stateCnt == 12 )", "rand()", "PlayWave(buf", "this->m_count *= 10"),
        ("if (m_stateCnt == 12)", "rand() % 3 + 1", "PlayWave(waveName", "m_count *= 10"),
    ),
    (
        "EarthSpike",
        "Ref/RagEffect.cpp",
        "void __thiscall CRagEffect::EarthSpike(CRagEffect *this, char *F1)",
        "0x5FD22D",
        "void CRagEffect::ApplyEarthSpike(char variant)",
        0x71B5E0,
        b"effect\\EF_IceArrow%d.wav\0",
        ("if ( (_BYTE)F1 == 2 )", "rand()", "PlayWave(buf"),
        ("else {", "rand()%3+1", "PlayWave(waveName"),
    ),
    (
        "FireHit",
        "Ref/RagEffect.cpp",
        "bool __thiscall CRagEffect::FireHit(CRagEffect *this)",
        "0x606D01",
        "bool CRagEffect::ProcessFireHit()",
        0x723410,
        b"_enemy_hit_fire1.wav\0",
        ("if ( !this->m_stateCnt )", "GetAttrWaveName(&g_session, 3)", "PlayWave"),
        ("if (m_stateCnt == 0)", "GetAttrWaveName(3)", "PlayWave(wave"),
    ),
    (
        "Kouenka",
        "Ref/RagEffect2.cpp",
        "void __thiscall CRagEffect::Kouenka(CRagEffect *this)",
        "0x60A8EE",
        "void CRagEffect::ApplyKouenka()",
        0x71B714,
        b"effect\\EF_FireArrow%d.wav\0",
        ("m_stateCnt >= 12 && m_stateCnt <= 15", "rand()", "PlayWave(buf"),
        ("m_stateCnt >= 12 && m_stateCnt <= 15", "rand() % 3 + 1", "PlayWave(waveName"),
    ),
    (
        "Prim3DParticleGravity",
        "Ref/RagEffectPrim.cpp",
        "bool __thiscall CEffectPrim::Prim3DParticleGravity(CEffectPrim *this)",
        "0x642CAD",
        "bool UpdateReferenceGravityParticle(CEffectPrim* prim)",
        0x71D44C,
        b"effect\\EF_Coin%d.wav\0",
        ("this->m_playWaveType == 1", "rand()", "PlayWave(buf"),
        ("prim->m_playWaveType == 1", "rand() % 3 + 1", "PlayWave(waveName"),
    ),
]


def ordered(text: str, tokens: tuple[str, ...], label: str) -> None:
    positions = [text.find(token) for token in tokens]
    assert all(position >= 0 for position in positions), f"{label}: missing {[(t, p) for t, p in zip(tokens, positions) if p < 0]}"
    assert positions == sorted(positions), f"{label}: branch order differs: {list(zip(tokens, positions))}"


current_functions = {
    "FireArrow": body(current, "void CRagEffect::ApplyFireArrow()", "current FireArrow"),
    "EarthSpike": body(current, "void CRagEffect::ApplyEarthSpike(char variant)", "current EarthSpike"),
    "FireHit": body(current, "bool CRagEffect::ProcessFireHit()", "current FireHit"),
    "Kouenka": body(current, "void CRagEffect::ApplyKouenka()", "current Kouenka"),
    "Prim3DParticleGravity": body(current, "bool UpdateReferenceGravityParticle(CEffectPrim* prim)", "current gravity particle"),
}

for name, ref_path, signature, call_va, current_signature, string_va, exact_bytes, source_order, current_order in source_specs:
    ref_body = normalize(body(read(ref_path, "cp1252"), signature, f"Ref {name}"))
    exe_body = normalize(body(exe, signature, f"EXE {name}"))
    assert ref_body == exe_body, f"{name}: complete Ref/EXE bodies differ"
    actual_bytes = va_bytes(string_va, len(exact_bytes))
    assert actual_bytes == exact_bytes, f"{name}: canonical EXE bytes differ at string VA 0x{string_va:X}: {actual_bytes!r}"
    current_body = current_functions[name]
    if name != "FireHit":
        c_literal = exact_bytes[:-1].decode("ascii").replace("\\", "\\\\")
        assert c_literal in current_body, f"{name}: current producer format differs from canonical EXE bytes"
    ordered(exe_body, source_order, f"EXE {name} producer branch {call_va}")
    ordered(current_body, current_order, f"current {name} route {call_va}")
    assert f"Ref runtime-wave: {call_va}" in current_body, f"{name}: missing current source anchor {call_va}"
    digest = hashlib.sha256(ref_body.encode()).hexdigest()
    print(f"{name}: Ref/EXE equal, chars={len(ref_body)}, sha256={digest}; string VA=0x{string_va:X}, bytes={exact_bytes!r}, call={call_va}")

assert va_bytes(0x723428, len(b"_enemy_hit_fire2.wav\0")) == b"_enemy_hit_fire2.wav\0"
manifest = read("docs/skill/generated/runtime_wave_callsites.tsv")
for row in manifest.splitlines():
    if any(va in row for va in ("0x5F963C", "0x5FD22D", "0x606D01", "0x60A8EE", "0x642CAD")):
        assert "producer port pending" not in row, f"unresolved runtime producer remains in manifest: {row}"
print("All five in-scope runtime-name producers are source-anchored; no live audio playback acceptance is claimed.")
