"""Run with .venv/Scripts/python.exe tools/test_skill_reference_index.py."""
import csv
import re
from pathlib import Path
import tempfile
import unittest

from skill_reference_index import SourceFunction, dispatch_cases, mask_literals, read_function, switch_case_locations
from ro_exe_resolve import cpp_string_literal

ROOT = Path(__file__).resolve().parents[1]


class ReferenceIndexTests(unittest.TestCase):
    def test_primitive_enum_matches_original_ids(self):
        def entries(path):
            source = path.read_text(encoding="latin-1")
            body = re.search(r"enum EFFECTPRIMID(?: : __int32)?\s*\{([\s\S]*?)\};", source).group(1)
            rows = re.findall(r"(PP_\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)", body)
            return [(name, int(value, 0)) for name, value in rows]
        expected = entries(ROOT / "HighPriest.exe.h")
        actual = entries(ROOT / "src/world/RagEffect.h")
        self.assertEqual(len(expected), 129)
        self.assertEqual(actual, expected)
        self.assertEqual({value for _, value in actual}, set(range(129)))

    def test_cp949_literal_hex_termination(self):
        self.assertEqual(cpp_string_literal(b'\xb0A.wav'), '"\\xB0" "A.wav"')
        self.assertEqual(cpp_string_literal(b'\xb01.wav'), '"\\xB0" "1.wav"')
        self.assertEqual(cpp_string_literal(b'\xb0\xa1F.wav'), '"\\xB0\\xA1" "F.wav"')
        self.assertEqual(cpp_string_literal(b'effect\\foo.wav'), '"effect\\\\foo.wav"')

    def test_shared_labels_nested_cases_and_literals(self):
        text = '''void Example::Run()
{
    switch (type) {
    case 1:
        goto shared;
    case 2:
        CRagEffect::Other(this);
        break;
    case 3:
shared:
        const char* literal = "} case 999: CRagEffect::Fake(";
        // { case 888:
        switch (level) {
        case 4:
            CRagEffect::Shared(this);
            break;
        }
        break;
    case 5:
        result = CRagEffect::Last(this);
        goto done;
    case 6:
        CRagEffect::Never(this);
        return 1;
    }
done:
    return 0;
}'''
        function = SourceFunction(text, mask_literals(text), 100)
        cases = {item.id: item for item in dispatch_cases(function, "type")}
        self.assertEqual(set(cases), {1, 2, 3, 5, 6})
        self.assertEqual(cases[1].handlers, ("Shared",))
        self.assertEqual(cases[2].handlers, ("Other",))
        self.assertEqual(cases[5].handlers, ("Last",))
        self.assertEqual(cases[1].line, 103)
        locations = list(switch_case_locations(function))
        self.assertIn((4, "level", 113), locations)

    def test_ref_duplicate_declaration_uses_signature(self):
        text = '''// Original Signature: bool Example::Run()
bool Example::Run2() { return false; }
// Original Signature: bool Example::Run2()
bool Example::Run2(
    int value)
{
    return true;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "ref.cpp"
            path.write_text(text)
            function = read_function(path, "Example::Run2")
        self.assertEqual(function.first_line, 4)
        self.assertIn("return true", function.text)
        self.assertNotIn("return false", function.text)

    def test_original_dispatchers(self):
        main = read_function(ROOT / "HighPriest.exe.c", "CRagEffect::OnProcess")
        cases = {row.id: row for row in dispatch_cases(main, "v6")}
        self.assertEqual(len(cases), 416)
        for case_id, handler in ((0, "Hit1"), (1, "Hit2"), (2, "Hit3"),
            (10, "Coin"), (11, "Endure"), (13, "GlassWall"), (709, "Test")):
            self.assertEqual(cases[case_id].handlers, (handler,))
        self.assertEqual(cases[474].handlers, ())
        self.assertEqual(cases[460].handlers, ("StormKick", "PortalWind"))
        path = ROOT / "Ref/RagEffect.cpp"
        for tier, count in (("OnProcess2", 73), ("OnProcess3", 204)):
            function = read_function(path, f"CRagEffect::{tier}")
            cases = dispatch_cases(function, "this->m_type")
            self.assertEqual(len(cases), count)
            self.assertGreater(function.first_line, 10600)

    def test_generated_effect_schema_and_anchors(self):
        path = ROOT / "docs/skill/generated/effect_handler_index.tsv"
        with path.open(encoding="utf-8") as stream:
            rows = list(csv.DictReader(stream, delimiter="\t"))
        self.assertEqual(len(rows), 693)
        cache = {}
        seen = set()
        for row in rows:
            self.assertNotIn(None, row)
            identity = row["tier"], row["case"]
            self.assertNotIn(identity, seen)
            seen.add(identity)
            source = row["source"]
            if source not in cache:
                cache[source] = (ROOT / source).read_text(encoding="latin-1").splitlines()
            anchor = cache[source][int(row["sourceLine"]) - 1].strip()
            token = anchor.removeprefix("case ").removesuffix(":")
            self.assertEqual(int(token, 0), int(row["case"]))


if __name__ == "__main__":
    unittest.main()
