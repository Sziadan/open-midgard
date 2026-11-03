"""Reproduce the client/rAthena packet inventory without changing either client.

Compiles packet-size probes with the installed MinGW compiler, evaluates the
server's real preprocessor configuration, and preserves evidence rather than
calling a registered handler a complete implementation. Run from any directory.
"""
from pathlib import Path
from collections import defaultdict, Counter
import csv
import json
import os
import re
import subprocess
import argparse
import hashlib

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-mingw-preset/packet-audit'
OUT = ROOT / 'docs/packet-audit'
CXX = Path('C:/Qt/Tools/mingw1310_64/bin/g++.exe')
ENV = dict(os.environ, PATH=str(CXX.parent) + os.pathsep + os.environ.get('PATH', ''))

def read(path):
    return Path(path).read_text(encoding='utf-8', errors='replace')

def run(args):
    result = subprocess.run([str(x) for x in args], capture_output=True, text=True, env=ENV)
    if result.returncode:
        raise RuntimeError(result.stderr[:12000])
    return result.stdout

def block(text, start):
    """Balanced C++ block, with strings and comments excluded from brace counts."""
    at = text.index('{', start)
    masked = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/',
                    lambda m: ' ' * len(m[0]), text, flags=re.S)
    depth, end = 1, at + 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return text[at:end], end

def compile_run(name, source, includes, extra=()):
    cpp = BUILD / (name + '.cpp')
    cpp.write_text(source, encoding='utf-8')
    exe = BUILD / (name + '.exe')
    run([CXX, '-std=c++17', *['-I' + str(x) for x in includes], cpp, *extra, '-o', exe])
    output = run([exe])
    (BUILD / (name + '.txt')).write_text(output, encoding='utf-8')
    return output

def preprocess(name, source, includes):
    cpp = BUILD / (name + '.cpp')
    cpp.write_text(source, encoding='utf-8')
    output = run([CXX, '-E', '-P', '-std=c++17', *['-I' + str(x) for x in includes], cpp])
    (BUILD / (name + '.i')).write_text(output, encoding='utf-8')
    return output

def functions(text, pattern):
    for match in re.finditer(pattern, text, flags=re.M):
        body, end = block(text, match.start())
        yield match.group(1), body, match.start(), end

def line(text, position):
    return text.count('\n', 0, position) + 1

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', default='C:/Users/sziad/source/repos/rathena-master')
    args = parser.parse_args()
    server = Path(args.server)
    BUILD.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    inc = [server / 'src', server / '3rdparty', server / '3rdparty/libconfig']
    # Map packets do not need map.hpp to evaluate their packed wire layouts.
    # The substituted constants are newer-only map constants, not 2008 fields.
    header = read(server / 'src/map/packets.hpp').replace('#include "map.hpp"',
        '#include <common/mmo.hpp>\n#define MESSAGE_SIZE 80\n#define TALKBOX_MESSAGE_SIZE 80\n'
        '#define MAX_ACHIEVEMENT_OBJECTIVES 10\n#define MAX_ITEM_RDM_OPT 5')
    header = header.replace('#include "packets_struct.hpp"',
        '#include "' + (server / 'src/map/packets_struct.hpp').as_posix() + '"')
    (BUILD / 'server_packets.hpp').write_text(header, encoding='utf-8')
    registry_text = compile_run('server_registry',
        '#include <cstdio>\n#include "server_packets.hpp"\nint main(){\n'
        '#define packetdb_addpacket(op,len,handler,...) std::printf("%04x|%d|%s|%s\\n",int(op),int(len),#handler,#__VA_ARGS__)\n'
        '#include "' + (server / 'src/map/clif_packetdb.hpp').as_posix() + '"\n}', inc)
    server_db, server_history = {}, defaultdict(list)
    for value in registry_text.splitlines():
        op, size, handler, positions = value.split('|')
        op = int(op, 16)
        server_db[op] = {'size': int(size), 'parser': '' if handler == 'nullptr' else handler, 'positions': positions}
        server_history[op].append(server_db[op].copy())

    constants, names, sizes, variable, declarations = {}, defaultdict(set), {}, set(), {}
    active_sources = {}
    for stage, include in [('map', '"server_packets.hpp"'), ('login-char', '<common/packets.hpp>')]:
        pre = preprocess(stage + '_headers', '#include ' + include, inc)
        headers = re.findall(r'const int16 (HEADER_\w+)\s*=\s*([^;]+);', pre)
        enum = re.search(r'enum packet_headers\s*\{(.*?)\};', pre, re.S)
        symbols = [name for name, _ in headers]
        if enum:
            symbols += re.findall(r'^\s*(\w+)\s*=', enum[1], re.M)
        structs = {}
        for m in re.finditer(r'\bstruct\s+(\w+)\s*\{', pre):
            body, _ = block(pre, m.start())
            structs[m[1]] = body
        queries = []
        for symbol in symbols:
            queries.append(f'printf("CONST|{symbol}|%04x\\n",int({symbol}));')
            struct = 'PACKET_' + symbol.removeprefix('HEADER_')
            if struct in structs:
                queries.append(f'printf("SIZE|{symbol}|%04x|%zu\\n",int({symbol}),sizeof({struct}));')
                if re.search(r'\[\s*(?:0)?\s*\]', structs[struct]):
                    variable.add(symbol)
                declarations[symbol] = {'stage': stage, 'unavailable': '_____unavailable' in structs[struct]}
        values = compile_run(stage + '_constants', '#include <cstdio>\n#include ' + include + '\nint main(){\n' + '\n'.join(queries) + '\n}', inc)
        for row in values.splitlines():
            parts = row.split('|')
            kind, name, opcode = parts[:3]
            opcode = int(opcode, 16)
            constants[name] = opcode
            names[opcode].add(name)
            if kind == 'SIZE':
                sizes[name] = int(parts[3])
        files = ['src/map/clif.cpp'] if stage == 'map' else ['src/login/loginclif.cpp', 'src/char/char_clif.cpp']
        for filename in files:
            raw = re.sub(r'^\s*#\s*include[^\n]*', '', read(server / filename), flags=re.M)
            active_sources[filename] = preprocess(Path(filename).stem + '_active', '#include ' + include + '\n' + raw, inc)

    emitted, server_function_ops = defaultdict(list), defaultdict(set)
    for filename, text in active_sources.items():
        # All client-interface functions, including replies produced by parsers.
        pattern = r'^[\w:<>,*& \t]+\b((?:clif_|logclif_|loginclif_|char_clif_|chclif_)\w+)\s*\([^;{]*?\)\s*\{'
        for name, body, at, _ in functions(text, pattern):
            symbols = set(re.findall(r'\b(?:HEADER_(?:ZC|AC|HC|SC)_\w+|\w+Type)\b', body))
            ops = {constants[s] for s in symbols if s in constants}
            for m in re.finditer(r'(?:[WR]FIFOW|WBUFW)\([^,]*,\s*0\s*\)\s*=\s*(0x[0-9a-fA-F]+)|[Pp]acket[Tt]ype\s*=\s*(0x[0-9a-fA-F]+)', body):
                ops.add(int(m[1] or m[2], 16))
            aliases = {m[1]: int(m[2], 16) for m in re.finditer(r'\b(\w+)\s*=\s*(0x[0-9a-fA-F]+)\s*;', body)}
            for m in re.finditer(r'(?:[WR]FIFOW|WBUFW)\([^,]*,\s*0\s*\)\s*=\s*(\w+)\s*;|[Pp]acket[Tt]ype\s*=\s*(\w+)\s*;', body):
                alias = m[1] or m[2]
                if alias in aliases:
                    ops.add(aliases[alias])
            for op in ops:
                # A broad symbol reference is conservative evidence, not a replay.
                emitted[op].append({'file': filename, 'function': name})
                server_function_ops[name].add(op)

    game = read(ROOT / 'src/gamemode/GameModePacket.cpp')
    body, _ = block(game, game.index('void RegisterDefaultGameModePacketHandlers('))
    body = re.sub(r'\[\]\(CGameMode& mode,const PacketView& packet\)\{ro::commerce::Receive\(mode,packet\);\}', '"ro::commerce::Receive"', body)
    body = re.sub(r'\bHandle\w+\b', lambda m: '"' + m[0] + '"', body)
    profiles = read(ROOT / 'src/network/MapSendProfile.cpp')
    chunk = profiles[profiles.index('MapReceiveProfile MakePacketVer23ReceiveProfile()'):]
    chunk = chunk[:chunk.index('return profile;')]
    assignments = '\n'.join('p.' + key + '=' + value + ';' for key, value in re.findall(r'profile\.(\w+)\s*=\s*([^;]+);', chunk) if key not in ('id', 'name'))
    client_text = compile_run('client_registry', '''#include <cstdio>
#include <map>
#include <string>
#include "network/Packet.h"
#include "network/MapSendProfile.h"
#include "network/GronPacket.h"
namespace ro::net {const MapReceiveProfile& GetActiveMapReceiveProfile(){static auto p=[](){MapReceiveProfile p{};
''' + assignments + '''
return p;}();return p;}}
struct CGameModePacketRouter {std::map<u16,std::string> handlers;void Clear(){handlers.clear();}void Register(u16 op,const char* name){if(handlers.count(op))printf("OVERRIDE|%04x|%s|%s\\n",op,handlers[op].c_str(),name);handlers[op]=name;}};
void RegisterHandlerIfValid(CGameModePacketRouter& r,u16 op,const char* name){if(op)r.Register(op,name);}
void RegisterDefaultGameModePacketHandlers(CGameModePacketRouter& router)
''' + body + '''
int main(){CGameModePacketRouter r;RegisterDefaultGameModePacketHandlers(r);for(int op=0;op<65536;++op){auto n=ro::net::GetPacketSize(op);if(n||r.handlers.count(op))printf("CLIENT|%04x|%d|%s\\n",op,n,r.handlers[op].c_str());}}
''', [ROOT / 'src'], [ROOT / 'src/network/GronPacket.cpp'])
    client, overrides = {}, []
    for row in client_text.splitlines():
        parts = row.split('|')
        if parts[0] == 'OVERRIDE':
            overrides.append({'opcode': '0x' + parts[1].upper(), 'before': parts[2], 'after': parts[3]})
        else:
            _, op, size, handler = parts
            client[int(op, 16)] = {'size': int(size), 'handler': handler}
    login = read(ROOT / 'src/gamemode/LoginMode.cpp')
    login_handlers = {int(op, 16): handler for op, handler in re.findall(r'case (0x\w+):\s*(On\w+)\(pkt\)', login)}
    login_handlers.update({0x1dc: 'OnAckHash', 0x1f1: 'OnNotifyError', 0x73: 'OnZcAcceptEnter', 0x2eb: 'OnZcAcceptEnter'})
    ignored_login = {0x283, 0x8482, 0x8483}
    handlers = {}
    for name, content, at, _ in functions(game, r'^void\s+(Handle\w+)\s*\([^;{]*?\)\s*\{'):
        handlers[name] = {'line': line(game, at), 'lines': len(content.splitlines()), 'body': content}

    outgoing = defaultdict(list)
    profile_code, _ = block(profiles, profiles.index('MapGameplaySendProfile MakePacketVer23Profile()'))
    profile_fields_match = re.search(r'struct MapGameplaySendProfile\s*\{(.*?)\};', read(ROOT / 'src/network/MapSendProfile.h'), re.S)
    profile_fields = re.findall(r'u16\s+(\w+)\s*;', profile_fields_match[1])
    profile_values = compile_run('outbound_profile', '#include <cstdio>\n#include "network/Packet.h"\n#include "network/MapSendProfile.h"\nusing namespace ro::net;\nro::net::MapGameplaySendProfile profile()' + profile_code + '\nint main(){auto p=profile();' + ''.join(f'printf("{field}|%04x\\n",p.{field});' for field in profile_fields) + '}', [ROOT / 'src'])
    send_fields = {name: int(value, 16) for name, value in (x.split('|') for x in profile_values.splitlines())}
    # Evidence of opcode construction, rather than arbitrary hexadecimal matches.
    for file in (ROOT / 'src').rglob('*.cpp'):
        if file.name in ('HealRenderPreview.cpp', 'GronPacket.cpp'):
            continue
        text = read(file)
        for m in re.finditer(r'\b(?:packet|begin|named|simple)\s*\(\s*(0x[\da-fA-F]+)\b', text):
            outgoing[int(m[1], 16)].append({'file': file.relative_to(ROOT).as_posix(), 'line': line(text, m.start())})
        for field, op in send_fields.items():
            for m in re.finditer(r'\b(?:\w+\.)?' + re.escape(field) + r'\b', text):
                if 'GetActiveMapGameplaySendProfile' in text:
                    outgoing[op].append({'file': file.relative_to(ROOT).as_posix(), 'line': line(text, m.start()), 'profile_field': field})
        for m in re.finditer(r'\b(?:packet|pkt|request)\s*\[[^\]]*\]\s*=\s*\{\s*(0x[\da-fA-F]+|\d+)\s*,\s*(0x[\da-fA-F]+|\d+)\s*[,}]', text):
            lo, hi = int(m[1], 0), int(m[2], 0)
            if lo < 256 and hi < 256:
                outgoing[lo | hi << 8].append({'file': file.relative_to(ROOT).as_posix(), 'line': line(text, m.start()), 'kind': 'byte-array'})
    rows = []
    all_ops = set(server_db) | set(client) | set(names) | set(emitted)
    for op in sorted(all_ops):
        db, local = server_db.get(op, {}), client.get(op, {})
        declared = sorted(names.get(op, []))
        c2s = bool(db.get('parser')) or any(n.startswith(('HEADER_CZ_', 'HEADER_CA_', 'HEADER_CH_')) for n in declared)
        s2c = bool(emitted.get(op)) or any(n.startswith(('HEADER_ZC_', 'HEADER_AC_', 'HEADER_HC_', 'HEADER_SC_')) for n in declared)
        # Opcode numbers are reused across server stages and legacy versions.
        available = any(not declarations.get(n, {}).get('unavailable', False) for n in declared) if declared else True
        stage = 'login/char' if any(n.startswith(('HEADER_AC_', 'HEADER_HC_', 'HEADER_CA_', 'HEADER_CH_')) for n in declared) or any(e['file'] != 'src/map/clif.cpp' for e in emitted.get(op, [])) else 'map'
        handler = local.get('handler', '')
        if stage == 'login/char' or op in login_handlers and not handler:
            handler = login_handlers.get(op, handler)
        wire = db.get('size')
        if wire is None:
            fixed = [sizes[n] for n in declared if n in sizes and not declarations.get(n, {}).get('unavailable')]
            wire = -1 if any(n in variable for n in declared) else (fixed[0] if fixed else None)
        flags = []
        if handler == 'HandleIgnorePacket' or (stage == 'login/char' and op in ignored_login):
            flags.append('ignored/stubbed')
        elif handler:
            flags.append('handler present; semantics not certified')
        elif s2c and available:
            flags.append('missing receive handler')
        else:
            flags.append('no receive handler; direction/activity needs review')
        if s2c and available and not local.get('size'):
            flags.append('missing framing')
        if s2c and available and wire is not None and local.get('size') and wire != local['size']:
            flags.append('length disagreement; inspect stage/version')
        if c2s and db.get('parser'):
            flags.append('outbound constructor found' if outgoing.get(op) else 'outbound route not proven by literal constructor scan')
        rows.append({'opcode': f'0x{op:04X}', 'stage': stage, 'names': declared, 'server_size': wire,
                     'client_size': local.get('size', 0), 'client_handler': handler,
                     'server_parser': db.get('parser', ''), 'server_emitters': emitted.get(op, []),
                     'server_field_positions': db.get('positions', ''),
                     'outbound_evidence': outgoing.get(op, []), 'direction': 'both/reused' if c2s and s2c else 'C2S' if c2s else 'S2C' if s2c else 'unresolved',
                     'available_declaration': available, 'status': flags,
                     'reachability': 'active sender/reference plus known wire layout' if emitted.get(op) and wire is not None else 'sender/reference exists; wire/reachability needs review' if emitted.get(op) else 'declared/registered; runtime emission not established',
                     'client_handler_line': handlers.get(handler, {}).get('line'), 'review': ''})
    data = {'scope': 'Client-facing login/character/map packets; rAthena configured PACKETVER 20080910, default client packetver23.',
            'method': 'Compiled registry probes and packed sizes; actual server preprocessor guards; source-referenced emitters; no gameplay replays.',
            'limitations': ['Registered handlers are not certified complete.', 'Outbound literal-constructor scan is evidence only; profile and raw-buffer sends need manual review.',
                           'Opcode reuse across stages/versions requires manual adjudication of apparent length conflicts.',
                           'Server packetdb contains historical and unused entries; declaration or reference does not prove every row is reachable.'],
            'client_overrides': overrides, 'counts': {'union': len(rows), 'client_table': len(client), 'server_registry': len(server_db),
                                                     'server_registry_assignments': sum(map(len, server_history.values())), 'handler_count': len(handlers)},
            'rows': rows}
    provenance = [ROOT / 'src/network/GronPacket.cpp', ROOT / 'src/network/MapSendProfile.cpp', ROOT / 'src/gamemode/GameModePacket.cpp',
                  server / 'src/custom/defines_pre.hpp', server / 'src/map/clif_packetdb.hpp', server / 'src/map/packets.hpp', server / 'src/map/packets_struct.hpp', server / 'src/map/clif.cpp']
    data['source_sha256'] = {p.as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in provenance}
    (OUT / 'inventory.json').write_text(json.dumps(data, indent=2), encoding='utf-8')
    with (OUT / 'inventory.csv').open('w', newline='', encoding='utf-8-sig') as f:
        fields = ['opcode', 'stage', 'direction', 'names', 'server_size', 'client_size', 'client_handler', 'server_parser', 'status', 'review']
        writer = csv.DictWriter(f, fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({k: '; '.join(row[k]) if isinstance(row[k], list) else row[k] for k in fields})
    (BUILD / 'handler_bodies.json').write_text(json.dumps(handlers, indent=2), encoding='utf-8')
    # Historical definitions are a catalog, not claims of missing 2008 behavior.
    historical = defaultdict(set)
    for file in [server / 'src/map/clif_packetdb.hpp', server / 'src/map/packets.hpp', server / 'src/map/packets_struct.hpp', server / 'src/common/packets.hpp']:
        text = read(file)
        for m in re.finditer(r'(?:DEFINE_PACKET_(?:HEADER|ID)\s*\(\s*(\w+)\s*,\s*|(?:parseable_)?packet\s*\(\s*)(0x[\da-fA-F]+)', text):
            historical[int(m[2], 16)].add(m[1] or file.name)
    with (OUT / 'historical-catalog.csv').open('w', newline='', encoding='utf-8-sig') as f:
        writer = csv.writer(f)
        writer.writerow(['opcode', 'definitions_all_versions', 'in_configured_registry', 'in_client_table'])
        for op, symbols in sorted(historical.items()):
            writer.writerow([f'0x{op:04X}', '; '.join(sorted(symbols)), op in server_db, op in client])
    print(json.dumps(data['counts']))
    print('Final router overrides:', overrides)
    print('Status flags:', dict(Counter(flag for row in rows for flag in row['status'])))

if __name__ == '__main__':
    main()
