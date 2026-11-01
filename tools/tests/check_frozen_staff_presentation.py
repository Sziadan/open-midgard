"""Compile real resource/depth paths; inspect asset headers without rendering."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

from check_ammo_cursor_regressions import function

ROOT = Path(__file__).resolve().parents[2]


def literal(value):
    return '"' + ''.join(f'\\x{byte:02x}' for byte in value) + '"'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cxx', required=True)
    args = parser.parse_args()
    actor = (ROOT / 'src/world/GameActor.cpp').read_text(encoding='utf-8')
    session = (ROOT / 'src/session/Session.cpp').read_text(encoding='utf-8')
    msg = (ROOT / 'src/world/MsgEffect.cpp').read_text(encoding='utf-8')
    world = (ROOT / 'src/world/World.cpp').read_text(encoding='utf-8')
    tokens = [re.search(r'kWeaponTokenRod = "([^"]+)"', source)[1] for source in [actor, session]]
    canonical = (ROOT / 'HighPriest.exe').read_bytes()[0x31D824:0x31D82A]
    assert canonical == b'_\xb7\xd4\xb5\xe5\0'
    assert tokens == [r'\xB7\xD4\xB5\xE5'] * 2
    # Run the real resolver against paths from the extracted CP949 asset names.
    assets = []
    root = ROOT / 'Ref/GRF-Content'
    for path in (root / 'data/sprite').rglob('*'):
        if not path.is_file() or path.suffix not in ['.act', '.spr']:
            continue
        raw = str(path.relative_to(root)).replace('/', '\\').encode('latin1', errors='replace')
        if raw.decode('cp949', errors='replace').split('\\')[-1] not in [
            f'위저드_{sex}_롯드.{extension}' for sex in ['남', '여'] for extension in ['act', 'spr']
        ]:
            continue
        header = path.read_bytes()[:2]
        assert header == (b'AC' if path.suffix == '.act' else b'SP')
        stem = raw.split(b'\\')[-1].rsplit(b'_', 1)[0]
        body = 'data\\sprite\\인간족\\몸통\\'.encode('cp949') + stem + b'.act'
        assets.append('{ ' + ', '.join([literal(body), literal(raw), literal(path.suffix[1:].encode())]) + ' }')
    assert len(assets) == 4, f'Expected male/female wizard ACT and SPR, got {len(assets)}'
    render = function(msg, 'void CMsgEffect::Render(')
    assert '|| (m_msgEffectType == 5 && m_masterActor)' in render
    assert 'SampleActorShellDepth(vertex.y' in function(msg, 'bool QueueMsgSpriteQuad(')
    assert 'RenderFrozenActorEffectPass(*entry.actor->m_iceEffect' in world
    with tempfile.TemporaryDirectory(prefix='ro_frozen_staff_') as directory:
        build = Path(directory)
        constants = actor[actor.index('constexpr const char* kHumanSpriteBodyDirMarker'):actor.index('const char* GetGenericWeaponToken(')]
        (build / 'StaffOverlayBodies.inc').write_text(constants + '\n' + '\n'.join(function(actor, name) for name in [
            'const char* GetGenericWeaponToken(', 'std::string GetPcOverlayWeaponToken(',
            'bool ExtractPcOverlayPathParts(', 'bool BuildWeaponOverlayPath(']), encoding='utf-8')
        (build / 'StaffOverlayAssets.inc').write_text(
            'struct Asset { const char* body; const char* overlay; const char* extension; };\n'
            'const Asset assets[] = {\n' + ',\n'.join(assets) + '\n};', encoding='utf-8')
        (build / 'FrozenShellBody.inc').write_text(function(msg, 'void RenderFrozenActorEffectPass('), encoding='utf-8')
        executable = build / 'frozen_staff_checks.exe'
        subprocess.run([args.cxx, '-std=c++17', '-UNDEBUG', f'-I{build}', f'-I{ROOT / "src"}',
                        str(ROOT / 'tools/tests/FrozenStaffPresentationTests.cpp'), '-o', str(executable)], check=True)
        environment = os.environ.copy()
        environment['PATH'] = str(Path(args.cxx).resolve().parent) + os.pathsep + environment.get('PATH', '')
        subprocess.run([str(executable)], check=True, env=environment)


if __name__ == '__main__':
    main()
