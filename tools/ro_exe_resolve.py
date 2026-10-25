"""Resolve HighPriest.exe virtual addresses, IDA string symbols, and PlayWave call sites.

The decompiled sources under Ref/ and HighPriest.exe.c truncate EUC-KR literals
(aEffectT_24 = "effect\\\\T_"). This module reads the bytes from HighPriest.exe
instead of guessing names. Nothing in here maps a symbol by suffix order or by
fuzzy asset-name search.
"""

from __future__ import annotations

import pathlib
import re
import struct
from dataclasses import dataclass


ROOT = pathlib.Path(__file__).resolve().parents[1]
EXE_PATH = ROOT / "HighPriest.exe"
DECOMPILE_PATH = ROOT / "HighPriest.exe.c"
PLAYWAVE_VA = 0x43A970

C_STRING_RE = re.compile(
    r'^char (?P<name>a[A-Za-z0-9_]+)\[(?:\d+)?\] = "(?P<body>(?:\\.|[^"\\])*)";',
    re.M,
)
SIGNATURE_RE = re.compile(
    r"Original Signature: .*?(?P<qual>(?:[A-Za-z_][A-Za-z0-9_]*::)*)(?P<name>~?[A-Za-z_][A-Za-z0-9_]*)\("
)
ADDRESS_RE = re.compile(r"Decompiled from address: (?P<addr>0x[0-9A-Fa-f]+)")


@dataclass(frozen=True)
class Section:
    name: str
    va: int
    virtual_size: int
    raw: int
    raw_size: int


@dataclass(frozen=True)
class ResolvedString:
    va: int
    text: str
    raw: bytes


class ExeImage:
    def __init__(self, path: pathlib.Path = EXE_PATH) -> None:
        self.data = path.read_bytes()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[pe:pe + 4] != b"PE\0\0":
            raise ValueError(f"{path} is not a PE image")
        coff = pe + 4
        section_count = struct.unpack_from("<H", self.data, coff + 2)[0]
        optional_size = struct.unpack_from("<H", self.data, coff + 16)[0]
        optional = coff + 20
        self.image_base = struct.unpack_from("<I", self.data, optional + 28)[0]
        section_table = optional + optional_size
        self.sections: list[Section] = []
        for index in range(section_count):
            off = section_table + index * 40
            name = self.data[off:off + 8].split(b"\0", 1)[0].decode("ascii", "replace")
            virtual_size, va, raw_size, raw = struct.unpack_from("<IIII", self.data, off + 8)
            self.sections.append(Section(name, self.image_base + va, virtual_size, raw, raw_size))

    def offset_of(self, va: int) -> int | None:
        for section in self.sections:
            span = max(section.virtual_size, section.raw_size)
            if section.va <= va < section.va + span:
                delta = va - section.va
                if delta >= section.raw_size:
                    return None
                return section.raw + delta
        return None

    def va_of(self, offset: int) -> int | None:
        for section in self.sections:
            if section.raw <= offset < section.raw + section.raw_size:
                return section.va + (offset - section.raw)
        return None

    def read_cstring_at(self, va: int, limit: int = 512) -> ResolvedString | None:
        offset = self.offset_of(va)
        if offset is None:
            return None
        end = self.data.find(b"\0", offset, offset + limit)
        if end < 0:
            return None
        raw = self.data[offset:end]
        if not raw:
            return ResolvedString(va, "", raw)
        try:
            text = raw.decode("cp949")
        except UnicodeDecodeError:
            text = raw.decode("latin-1")
        return ResolvedString(va, text, raw)

    def read_i32_table(self, va: int, count: int) -> list[int]:
        offset = self.offset_of(va)
        if offset is None:
            raise ValueError(f"VA 0x{va:X} is not in a section")
        need = count * 4
        if offset + need > len(self.data):
            raise ValueError(f"table at 0x{va:X} overruns the image")
        return list(struct.unpack_from("<" + "i" * count, self.data, offset))

    def iter_cstrings(self, sections: tuple[str, ...] = (".rdata", ".data")) -> list[ResolvedString]:
        found: list[ResolvedString] = []
        wanted = set(sections)
        for section in self.sections:
            if section.name not in wanted:
                continue
            blob = self.data[section.raw:section.raw + section.raw_size]
            start = 0
            while start < len(blob):
                end = blob.find(b"\0", start)
                if end < 0:
                    break
                raw = blob[start:end]
                if raw and all(32 <= byte < 127 or byte >= 0x80 for byte in raw):
                    va = section.va + start
                    try:
                        text = raw.decode("cp949")
                    except UnicodeDecodeError:
                        text = raw.decode("latin-1")
                    found.append(ResolvedString(va, text, raw))
                start = end + 1
        return found


def unescape_c_string(body: str) -> bytes:
    out = bytearray()
    index = 0
    while index < len(body):
        char = body[index]
        if char != "\\":
            out.append(ord(char) & 0xFF)
            index += 1
            continue
        index += 1
        if index >= len(body):
            break
        esc = body[index]
        index += 1
        simple = {"n": b"\n", "r": b"\r", "t": b"\t", "\\": b"\\", '"': b'"', "'": b"'", "0": b"\0", "a": b"\a"}
        if esc in simple:
            out.extend(simple[esc])
            continue
        if esc == "x":
            hex_digits = ""
            while index < len(body) and body[index] in "0123456789abcdefABCDEF" and len(hex_digits) < 2:
                hex_digits += body[index]
                index += 1
            out.append(int(hex_digits or "0", 16))
            continue
        if esc in "01234567":
            octal = esc
            while index < len(body) and body[index] in "01234567" and len(octal) < 3:
                octal += body[index]
                index += 1
            out.append(int(octal, 8) & 0xFF)
            continue
        out.append(ord(esc) & 0xFF)
    return bytes(out)


class SymbolTable:
    """IDA `char aName[] = "..."` declarations, resolved against the exe bytes."""

    def __init__(self, image: ExeImage, decompile: pathlib.Path = DECOMPILE_PATH) -> None:
        self.image = image
        text = decompile.read_text(encoding="latin-1")
        declared: dict[str, bytes] = {}
        for match in C_STRING_RE.finditer(text):
            declared[match.group("name")] = unescape_c_string(match.group("body"))
        self.declared = declared
        strings = image.iter_cstrings()
        self.by_exact: dict[bytes, list[ResolvedString]] = {}
        for item in strings:
            self.by_exact.setdefault(item.raw, []).append(item)
        self._cache: dict[str, ResolvedString | None] = {}

    def resolve_name(self, name: str) -> ResolvedString | None:
        if name in self._cache:
            return self._cache[name]
        resolved = self._resolve_uncached(name)
        self._cache[name] = resolved
        return resolved

    def _resolve_uncached(self, name: str) -> ResolvedString | None:
        if name.startswith("byte_") or name.startswith("dword_") or name.startswith("unk_"):
            va = int(name.split("_", 1)[1], 16)
            return self.image.read_cstring_at(va)
        prefix = self.declared.get(name)
        if prefix is None:
            return None
        exact = self.by_exact.get(prefix, [])
        if exact:
            return exact[0]
        extensions: list[ResolvedString] = []
        seen: set[bytes] = set()
        for raw, items in self.by_exact.items():
            if raw.startswith(prefix) and raw != prefix and raw not in seen:
                seen.add(raw)
                extensions.append(items[0])
        if len(extensions) == 1:
            return extensions[0]
        return None


def cpp_string_literal(raw: bytes) -> str:
    """C string literal whose bytes are the original cp949 (or ASCII) name."""
    pieces: list[str] = []
    for byte in raw:
        if byte == ord("\\"):
            pieces.append("\\\\")
        elif byte == ord('"'):
            pieces.append('\\"')
        elif 32 <= byte < 127:
            pieces.append(chr(byte))
        else:
            pieces.append(f"\\x{byte:02X}")
    literal = "".join(pieces)
    literal = re.sub(r"(\\x[0-9A-Fa-f]{2})(?=[0-9A-Fa-f])", r'\1" "', literal)
    return f'"{literal}"'


@dataclass(frozen=True)
class PushSlot:
    kind: str
    value: int | None
    size: int


def _push_ending_at(data: bytes, end: int) -> PushSlot | None:
    if end >= 3 and data[end - 3:end] == b"\xD9\x1C\x24":
        if end >= 6 and data[end - 6:end - 3] == b"\x83\xEC\x04":
            return PushSlot("float-stack", None, 6)
        return PushSlot("float-stack", None, 3)
    if end >= 5 and data[end - 5] == 0x68:
        value = struct.unpack_from("<I", data, end - 4)[0]
        return PushSlot("imm32", value, 5)
    if end >= 6 and data[end - 6] == 0xFF and data[end - 5] == 0x35:
        value = struct.unpack_from("<I", data, end - 4)[0]
        return PushSlot("mem32", value, 6)
    if end >= 6 and data[end - 6] == 0xFF and data[end - 5] == 0xB5:
        value = struct.unpack_from("<i", data, end - 4)[0]
        return PushSlot("ebp32", value, 6)
    if end >= 4 and data[end - 4] == 0xFF and data[end - 3] == 0x74 and data[end - 2] == 0x24:
        return PushSlot("esp8", data[end - 1], 4)
    if end >= 3 and data[end - 3] == 0xFF and data[end - 2] == 0x75:
        return PushSlot("ebp8", struct.unpack_from("<b", data, end - 1)[0], 3)
    if end >= 2 and data[end - 2] == 0x6A:
        return PushSlot("imm8", struct.unpack_from("<b", data, end - 1)[0], 2)
    if end >= 1 and 0x50 <= data[end - 1] <= 0x57:
        return PushSlot("reg", data[end - 1] - 0x50, 1)
    return None


def argument_pushes_before(data: bytes, call_offset: int, count: int = 7, window: int = 96) -> list[PushSlot] | None:
    """Walk the pushes that feed a __cdecl call. Result is left-to-right arguments."""
    slots: list[PushSlot] = []
    cursor = call_offset
    floor = max(0, call_offset - window)
    while len(slots) < count and cursor > floor:
        slot = _push_ending_at(data, cursor)
        if slot is None or cursor - slot.size < floor:
            return None
        slots.append(slot)
        cursor -= slot.size
    if len(slots) != count:
        return None
    slots.reverse()
    return slots


def iter_call_sites(image: ExeImage, target_va: int) -> list[int]:
    text = next(section for section in image.sections if section.name == ".text")
    blob = image.data
    sites: list[int] = []
    start = text.raw
    stop = text.raw + text.raw_size - 5
    position = start
    while position < stop:
        position = blob.find(b"\xE8", position, stop)
        if position < 0:
            break
        rel = struct.unpack_from("<i", blob, position + 1)[0]
        call_va = image.va_of(position)
        if call_va is not None and (call_va + 5 + rel) & 0xFFFFFFFF == target_va:
            sites.append(position)
        position += 1
    return sites


@dataclass(frozen=True)
class PlayWaveCall:
    call_va: int
    function: str
    string_va: int | None
    string: str | None
    status: str


@dataclass(frozen=True)
class FunctionSpan:
    va: int
    name: str
    file: str


def load_function_spans(root: pathlib.Path = ROOT) -> list[FunctionSpan]:
    spans: list[FunctionSpan] = []
    files = list((root / "Ref").glob("*.cpp")) + [DECOMPILE_PATH]
    for path in files:
        if not path.exists():
            continue
        lines = path.read_text(encoding="latin-1").splitlines()
        pending_name: str | None = None
        for line in lines:
            signature = SIGNATURE_RE.search(line)
            if signature:
                pending_name = signature.group("qual") + signature.group("name")
                continue
            address = ADDRESS_RE.search(line)
            if address and pending_name:
                spans.append(FunctionSpan(int(address.group("addr"), 16), pending_name, path.name))
                pending_name = None
            elif address:
                pending_name = None
    spans.sort(key=lambda item: (item.va, item.name))
    return spans


def function_at(spans: list[FunctionSpan], va: int) -> str:
    best: list[FunctionSpan] = []
    for span in spans:
        if span.va <= va:
            best.append(span)
        else:
            break
    if not best:
        return ""
    start = best[-1].va
    names = []
    for span in best:
        if span.va == start and span.name not in names:
            names.append(span.name)
    return "|".join(names)


def align_truncated_symbols(
    body: str,
    start_va: int,
    end_va: int,
    image: ExeImage,
    symbols: SymbolTable,
) -> dict[str, ResolvedString]:
    """Pair truncated IDA names with the string pushes in one function.

    Used only when several symbols were decompiled to the same prefix, so the
    prefix is not a unique exe string. Source order and push order are both
    taken from that single function.
    """
    mentioned = re.findall(
        r"(?:assign\([^;\n]*_First(?: \+ \d+)?, |qmemcpy\(\w+->_Ptr, )"
        r"(a[A-Za-z0-9_]+|byte_[A-Za-z0-9_]+)",
        body,
    )
    ordered: list[str] = []
    for name in mentioned:
        if name not in ordered:
            ordered.append(name)
    start = image.offset_of(start_va)
    end = image.offset_of(end_va)
    if start is None or end is None or end <= start:
        return {}
    blob = image.data[start:end]
    pushes: list[ResolvedString] = []
    seen: set[int] = set()
    for index in range(len(blob) - 5):
        if blob[index] != 0x68:
            continue
        value = struct.unpack_from("<I", blob, index + 1)[0]
        if value in seen:
            continue
        item = image.read_cstring_at(value)
        if item is None or not item.text.lower().endswith((".wav", ".str")):
            continue
        seen.add(value)
        pushes.append(item)
    aligned: dict[str, ResolvedString] = {}
    cursor = 0
    for name in ordered:
        resolved = symbols.resolve_name(name)
        if resolved is not None:
            while cursor < len(pushes) and pushes[cursor].raw != resolved.raw:
                cursor += 1
            if cursor < len(pushes):
                cursor += 1
            continue
        prefix = symbols.declared.get(name)
        if prefix is None:
            return {}
        while cursor < len(pushes) and not pushes[cursor].raw.startswith(prefix):
            cursor += 1
        if cursor >= len(pushes):
            return {}
        aligned[name] = pushes[cursor]
        cursor += 1
    return aligned


def wav_like(text: str | None) -> bool:
    if not text:
        return False
    lowered = text.lower()
    return lowered.endswith(".wav") or "effect\\" in lowered or lowered.startswith("_")


# Right-to-left __cdecl: vFactor, minDist and maxDist are pushed first. Every literal
# PlayWave site in this client uses 1.0, 40 and 250 for those three.
_PLAYWAVE_TAIL = b"\x68\x00\x00\x80\x3F\x6A\x28\x68\xFA\x00\x00\x00"


def _nearest_name_push(image: ExeImage, call_offset: int) -> ResolvedString | None:
    """The name is the push closest to the call, after the 1.0/40/250 tail."""
    anchor = image.data.rfind(_PLAYWAVE_TAIL, max(0, call_offset - 96), call_offset)
    region_start = anchor + len(_PLAYWAVE_TAIL) if anchor >= 0 else max(0, call_offset - 24)
    best: ResolvedString | None = None
    for pos in range(call_offset - 5, region_start - 1, -1):
        if image.data[pos] != 0x68:
            continue
        value = struct.unpack_from("<I", image.data, pos + 1)[0]
        item = image.read_cstring_at(value)
        if item is None or not wav_like(item.text) or len(item.text) <= 4:
            continue
        best = item
        break
    return best


def resolve_playwave_calls(image: ExeImage, symbols: SymbolTable, spans: list[FunctionSpan]) -> list[PlayWaveCall]:
    del symbols
    calls: list[PlayWaveCall] = []
    for offset in iter_call_sites(image, PLAYWAVE_VA):
        call_va = image.va_of(offset) or 0
        function = function_at(spans, call_va)
        resolved = _nearest_name_push(image, offset)
        status = "ok" if resolved is not None else "UNRESOLVED"
        calls.append(PlayWaveCall(
            call_va,
            function,
            None if resolved is None else resolved.va,
            None if resolved is None else resolved.text,
            status,
        ))
    return calls
