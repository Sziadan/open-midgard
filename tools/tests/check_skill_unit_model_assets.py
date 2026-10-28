import codecs
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / "src/world/SkillUnit.cpp").read_text(encoding="utf-8")
MODEL_ROOT = ROOT / "Ref/GRF-Content/data/model"
TEXTURE_ROOT = ROOT / "Ref/GRF-Content/data/texture"


def c_string_bytes(expression: str) -> bytes:
    pieces = re.findall(r'"((?:\\.|[^"\\])*)"', expression)
    assert pieces, f"no C string literal found in {expression!r}"
    return b"".join(codecs.decode(piece, "unicode_escape").encode("latin1") for piece in pieces)


constants = {}
for match in re.finditer(r"const char (kTrap\w+)\[\]\s*=\s*([^;]+);", SOURCE):
    constants[match.group(1)] = c_string_bytes(match.group(2))

mapping_body_start = SOURCE.index("const char* CSkill::TrapModelName() const")
mapping_body_open = SOURCE.index("{", mapping_body_start)
depth = 0
for index in range(mapping_body_open, len(SOURCE)):
    if SOURCE[index] == "{":
        depth += 1
    elif SOURCE[index] == "}":
        depth -= 1
        if depth == 0:
            mapping_body = SOURCE[mapping_body_open + 1:index]
            break
else:
    raise AssertionError("TrapModelName body is unterminated")

mapping = {
    int(job): constants[name]
    for job, name in re.findall(r"case\s+(\d+)\s*:\s*return\s+(kTrap\w+)\s*;", mapping_body)
}
expected_jobs = {143, 144, 145, 147, 148, 149, 150, 151, 152, 153}
assert set(mapping) == expected_jobs, f"TrapModelName jobs differ: {sorted(mapping)}"

for job, model_bytes in sorted(mapping.items()):
    model_relative = model_bytes.decode("latin1").replace("\\", "/")
    model_path = MODEL_ROOT / model_relative
    assert model_path.is_file(), f"job{job} trap model is missing: {model_path}"

    raw = model_path.read_bytes()
    expected_material_prefix = model_bytes.rsplit(b"\\", 1)[0] + b"\\"
    material_refs = {
        chunk.decode("latin1").replace("\\", "/")
        for chunk in raw.split(b"\0")
        if chunk.lower().endswith(b".bmp") and chunk.startswith(expected_material_prefix)
    }
    assert material_refs, f"job{job} model has no decodable BMP material reference: {model_path}"
    for material_relative in material_refs:
        texture_path = TEXTURE_ROOT / material_relative
        assert texture_path.is_file(), f"job{job} material is missing: {texture_path}"

print(f"Trap model assets: {len(mapping)} job mappings, all RSM and referenced BMP paths exist")
