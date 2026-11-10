"""Regenerate selected source chat alias rules from reviewed PE/source evidence.

Use --check for verification without writes. This checks data provenance and
regeneration; it does not certify the still-open general command dispatcher.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "docs/packets/evidence/PK09.21-chat-parser-data.json"

def require(value, message):
    if not value:
        raise SystemExit("FAIL: " + message)

def bytes_literal(hex_text):
    h = hex_text.replace(" ", "")
    return '"' + ''.join('\\x' + h[i:i+2] for i in range(0, len(h), 2)) + '"'

def utf8_literal(text):
    return 'u8' + json.dumps(text, ensure_ascii=True)

def verify(data):
    for name, expected in data['reference_sha256'].items():
        require(hashlib.sha256((ROOT/name).read_bytes()).hexdigest() == expected,
                'reference changed; strong source review required: ' + name)
    binary = (ROOT/data['selected_binary']).read_bytes()
    pe = struct.unpack_from('<I', binary, 0x3c)[0]
    count = struct.unpack_from('<H', binary, pe+6)[0]
    optional = pe+24
    base = struct.unpack_from('<I', binary, optional+28)[0]
    sections = optional+struct.unpack_from('<H', binary, pe+20)[0]
    def at(va, length):
        rva = va-base
        for i in range(count):
            _, start, size, raw = struct.unpack_from('<IIII', binary, sections+40*i+8)
            if start <= rva and rva+length <= start+size:
                return binary[raw+rva-start:raw+rva-start+length]
        raise SystemExit('FAIL: non-file-backed PE address ' + hex(va))
    aliases = data['no_parameter_aliases']
    require(len(aliases) == 292, 'complete lookup registration count')
    require(sum(x['dynamic_msg_id'] is not None for x in aliases) == 49, 'resource alias count')
    require(sum(x['registration_kind']=='insert' for x in aliases) == 209, 'insert count')
    require(sum(x['registration_kind']=='push_back' for x in aliases) == 83, 'push_back count')
    text = (ROOT/'Ref/Session.cpp').read_text(encoding='utf-8')
    start = text.index('void __thiscall CSession::InitTalkTypeTable(')
    body = text[start:text.index('std::stable_sort', start)]
    tags, current = [], None
    for line in body.splitlines():
        found = re.search(r'= (TT_\w+);', line)
        if found:
            current = found.group(1)
        if 'm_talkTypeTable' in line and ('::insert' in line or '::push_back' in line):
            tags.append(current)
    require(len(tags) == len(aliases), 'primary source registration count')
    tag_types = {}
    restricted = {'/noshift', '/noctrl', '/ns', '/nc'}
    for x, tag in zip(aliases, tags):
        require(x['source_tag'] == tag, 'primary source tag/order')
        require(tag_types.setdefault(tag, x['type']) == x['type'], 'inconsistent selected type for ' + tag)
        call = int(x['registration_call_address'], 16)
        require(at(call, 1) == b'\xe8', 'registration must be a direct PE call')
        window = at(call-140, 140)
        if x['dynamic_msg_id'] is None:
            address = int(x['alias_address'], 16)
            raw = bytes.fromhex(x['alias_raw_hex'])
            require(at(address, len(raw)+1) == raw+b'\0', 'static alias PE bytes')
            require(raw.decode('cp949') == x['alias_cp949'], 'CP949 alias decode')
            require(struct.pack('<I', address) in window, 'alias address absent before registration')
        else:
            require(b'\x68'+struct.pack('<I', x['dynamic_msg_id']) in window,
                    'resource ID push absent before registration')
        require(x['ordinary_server_only'] == (x['alias_cp949'] in restricted), 'InstantEvent exclusion')
    require(tag_types['TT_AUTO_MSG_ON_OFF'] == 152, 'selected auto-message type; secondary153 is wrong')
    rules = data['parameter_aliases']
    require(len(rules) == 82 and len({x['address'] for x in rules}) == 82, 'parameter alias coverage')
    for x in rules:
        address = int(x['address'], 16)
        raw = bytes.fromhex(x['bytes_hex'])
        require(at(address, len(raw)+1) == raw+b'\0', 'parameter alias PE bytes')
        require(raw.decode('cp949') == x['decoded_text'], 'parameter CP949 decode')
        for site in x['reference_code_addresses']:
            require(struct.pack('<I', address) in at(int(site,16), 12), 'parameter literal reference')

    return at

def render(data):
    lines = ['// Selected InitTalkTypeTable663E20, original registration order.',
             '// All209 inserts +83 push_back entries before stable_sort6663E2.',
             'inline constexpr SourceNoParamRule kSourceNoParamRules[] = {']
    for x in data['no_parameter_aliases']:
        mid = x['dynamic_msg_id']
        utf8 = utf8_literal(x['alias_cp949']) if mid is None else '""'
        raw = bytes_literal(x['alias_raw_hex']) if mid is None else '""'
        lines.append(f'    {{{utf8}, {raw}, {mid if mid is not None else -1}, {x["type"]}, {str(x["ordinary_server_only"]).lower()}}}, // {x["registration_call_address"]}')
    lines.append('};')
    aliases = '\n'.join(lines)+'\n'
    lines = ['// Selected GetTalkType6621D0 literals and primary branch semantics.',
             '// Generated from hash-pinned static PE/source inventory; review numeric types.',
             'inline constexpr SourceParameterRule kSourceParameterRules[] = {']
    for x in data['parameter_aliases']:
        lines.append(f'    {{{utf8_literal(x["decoded_text"])}, {bytes_literal(x["bytes_hex"])}, SourceParameterKind::{x["kind"]}, {x["type"]}, {x["value"]}}}, // {int(x["address"],16):08X}')
    lines.append('};')
    return {'src/core/SourceChatAliases.generated.inc': aliases,
            'src/core/SourceChatParameterRules.generated.inc': '\n'.join(lines)+'\n'}

def verify_submission(data, parser_data, at):
    for name, expected in data['reference_sha256'].items():
        require(hashlib.sha256((ROOT/name).read_bytes()).hexdigest() == expected,
                'submission reference changed; strong review required: ' + name)
    types = {x['source_tag']: x['type'] for x in parser_data['no_parameter_aliases']}
    source = (ROOT/'Ref/GameMode.cpp').read_text(encoding='utf-8')
    start = source.index('void __thiscall CGameMode::ProcessTalkType(')
    body = source[start:source.index('// Original Signature:', start)]
    parts = re.split(r'^    case (TT_\w+):', body, flags=re.M)
    expected, unregistered = [], []
    for tag, block in zip(parts[1::2], parts[2::2]):
        if not tag.startswith('TT_REQ_EMOTION_'):
            continue
        if tag not in types:
            unregistered.append(tag)
            continue
        calls = re.findall(r'CMode::SendMsg\(this,\s*(\d+),\s*(\d+)', block)
        calls += re.findall(r'CMode::SendMsg\)\(\s*(\d+),\s*(\d+)', block)
        values = [int(value) for message, value in calls if int(message) == 33]
        if values:
            value = values[0]
        else:
            require(tag == 'TT_REQ_EMOTION_DICE' and '% 6 + 58' in block,
                    'unresolved exact emotion case: ' + tag)
            value = -1
        expected.append({'tag': tag, 'type': types[tag], 'emotion': value})
    require(expected == data['emotion_routes'] and len(expected) == 60,
            'source exact-case emotion routes differ')
    require(unregistered == data['unregistered_primary_emotion_tags'], 'unregistered emotion disposition')
    table = int(data['switch_table_address'], 16)
    for index, target in data['no_op_targets'].items():
        address = struct.unpack('<I', at(table+4*int(index), 4))[0]
        require(address == int(target, 16), 'selected no-op target')
        require(at(address, 6) == bytes.fromhex('8b4df45f5e5b'), 'selected no-op epilogue')

def render_submission(data):
    lines = ['// Primary exact-case33 arguments joined to selected alias types.',
             'inline constexpr SourceEmotionRoute kSourceEmotionRoutes[] = {']
    for row in data['emotion_routes']:
        lines.append(f'    {{{row["type"]}, {row["emotion"]}}}, // {row["tag"]}')
    lines.append('};')
    return {'src/core/SourceChatEmotions.generated.inc': '\n'.join(lines)+'\n'}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    data = json.loads(DATA.read_text(encoding='utf-8'))
    at = verify(data)
    submission = json.loads((ROOT/'docs/packets/evidence/PK09.21-chat-submission-data.json').read_text(encoding='utf-8'))
    verify_submission(submission, data, at)
    outputs = render(data)
    outputs.update(render_submission(submission))
    for name, content in outputs.items():
        path = ROOT/name
        if args.check:
            require(path.read_text(encoding='utf-8') == content, 'generated content differs: ' + name)
        else:
            path.write_text(content, encoding='utf-8')
    print('PASS: 292 lookup aliases (209 inserts +83 push_back, 49 resources), 82 parameter aliases; selected PE/source provenance and generated data agree.')
    print('PASS: 60 exact-case emotion routes and selected no-op targets agree with primary source/PE.')
    print('Core dispatch is implemented separately; remaining cases/options/global-special/editor/charset acceptance remains open.')

if __name__ == '__main__':
    main()
