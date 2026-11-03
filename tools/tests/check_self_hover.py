"""Exercise self-hover, real name/emblem packets and Qt rendering without login."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
executable = root / 'build-mingw-preset/open-midgard.exe'
environment = {k: v for k, v in os.environ.items() if not k.startswith('RO_HEAL_PREVIEW_')}
for backend in (3,):  # The MinGW preset includes Vulkan, not native D3D11/12.
    output = root / f'build-mingw-preset/self-hover-check/backend-{backend}'
    output.mkdir(parents=True, exist_ok=True)
    (output / 'open-midgard.ini').write_text(
        '[Graphics]\nWidth=800\nHeight=600\nSSAO=0\nRenderScale=100\nAntiAliasing=0\n'
        f'TextureUpscale=1\nWindowMode=0\n[Renderer]\nBackend={backend}\n')
    environment.update(OPEN_MIDGARD_DATA_DIR=str(output), RO_HEAL_PREVIEW_OUT=str(output / 'frame.bmp'),
        RO_HEAL_PREVIEW_ASSETS='D:/Spel/OldRO', RO_HEAL_PREVIEW_SELF_HOVER='1')
    with (output / 'stderr.log').open('w') as errors:
        process = subprocess.Popen([str(executable)], env=environment, stderr=errors)
        assert process.wait(timeout=40) == 0
    log = (executable.parent / f'debug_hp_{process.pid}.log').read_text(errors='replace')
    marker = log.rfind('[Build] marker=')
    if marker >= 0: log = log[marker:]
    (output / 'client.log').write_text(log)
    combined = log + (output / 'stderr.log').read_text(errors='replace')
    assert "Active backend confirmed as 'Vulkan'" in log
    expected = ('[SelfHoverPreview] framed=1 handled=1 nameParty=1 guildRank=1 emblem=1 '
        'belowBars=1 corruptIgnored=1 cleared=1 persisted=1 singleBubble=1')
    assert expected in log, log[-4000:]
    assert '[GuildEmblemPreview] confirmed=1 corruptUnconfirmed=1' in log
    assert '[GuildEmblemPreview] spansBothRows=1' in log
    for error in ('VUID-', 'Validation Error', 'ReferenceError:', 'TypeError:', 'is not a type'):
        assert error not in combined, combined[-4000:]
    print(f'PASS backend {backend}: one name/party/guild/rank/emblem bubble survives map-cache reset, below bars, corrupt-packet rejection, hover exit and Qt render')
