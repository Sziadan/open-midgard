"""Test production commerce protocol with a captured connection."""
from pathlib import Path
import os
import subprocess
root=Path(__file__).resolve().parents[2]
build=root/'build-mingw-preset/commerce-check'
build.mkdir(exist_ok=True)
source=(root/'src/gamemode/Commerce.cpp').read_text(encoding='utf-8')
(build/'Commerce.inc').write_text(source[source.index('namespace ro::commerce {'):])
compiler=Path('C:/Qt/Tools/mingw1310_64/bin/g++.exe')
exe=build/'protocol.exe'
subprocess.run([str(compiler),'-std=c++17','-DNOMINMAX','-O2','-I'+str(root/'src'),'-I'+str(build),str(root/'tools/tests/CommerceTests.cpp'),'-o',str(exe)],check=True)
env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env.get('PATH','')
subprocess.run([str(exe)],env=env,check=True)

from PIL import Image, ImageStat
client=root/'build-mingw-preset/open-midgard.exe'
def client_log(pid):
    candidates=[folder/f'debug_hp_{pid}.log' for folder in (root,client.parent,Path('D:/Spel/OldRO'))]
    files=sorted((p for p in candidates if p.exists()),key=lambda p:p.stat().st_mtime)
    assert files,('Missing client log',pid)
    log='\n'.join(p.read_text(errors='replace') for p in files)
    marker=log.rfind('[Build] marker=')
    return log[marker:] if marker>=0 else log
for case in ('inventory','vending','trade','incoming','mail','equipment','context','prompt','shopboard'):
    target=build/case;target.mkdir(exist_ok=True)
    (target/'open-midgard.ini').write_text('[Graphics]\nWidth=1280\nHeight=800\nSSAO=0\nRenderScale=100\nTextureUpscale=1\nWindowMode=0\n[Renderer]\nBackend=3\n')
    runenv={k:v for k,v in os.environ.items() if not k.startswith('RO_HEAL_PREVIEW_')}
    runenv.update(OPEN_MIDGARD_DATA_DIR=str(target),RO_HEAL_PREVIEW_OUT=str(target/'frame.bmp'),RO_HEAL_PREVIEW_ASSETS='D:/Spel/OldRO',RO_HEAL_PREVIEW_SELF_HOVER='1',RO_HEAL_PREVIEW_COMMERCE=case)
    with (target/'stderr.log').open('w') as errors:
        process=subprocess.Popen([str(client)],env=runenv,stderr=errors)
        assert process.wait(timeout=40)==0
    log=client_log(process.pid)
    (target/'client.log').write_text(log)
    combined=log+(target/'stderr.log').read_text(errors='replace')
    assert f'[CommercePreview] case={case} cartItems=1 inventory=2' in log
    assert '[CommercePreview] timedCartRoute=1 equipmentCartRoute=1 friendPromptRoute=1' in log
    if case=='equipment':assert '[CommercePreview] equipmentViewRoute=1' in log
    if case=='shopboard':assert '[CommercePreview] buffedBlacksmithVisible=1 selectedSpeed=150 boardReceived=1' in log
    if case=='shopboard':assert '[CommercePreview] guildPositionStream=1 packets=3 sit=1 stand=1 tradeAccepted=1' in log
    if case=='shopboard':
        assert '[CommercePreview] guildMarker=1 guildMarkerRemoved=1 guildFraming=1 guildChat=1' in log
        assert '[CommercePreview] frontCartWorldDepth=1' in log
        assert '[CommercePreview] partyHpMatchesPlayer=1' in log
        assert '[CommercePreview] remotePlayerAnchorsAligned=1 zoomSamples=3' in log
    for failure in ('QML error:','ReferenceError:','TypeError:','Binding loop','Unable to assign','VUID-','Validation Error'):
        assert failure not in combined,combined[-5000:]
    frame=Image.open(target/'frame.bmp').convert('RGB');frame.save(target/'frame.png')
    if case!='shopboard':assert max(ImageStat.Stat(frame.crop((10,60,370,360))).stddev)>10,'Blank inventory'
    if case=='inventory':
        assert sum(max(c)-min(c)>45 for c in frame.crop((29,141,61,173)).getdata())>15,'Item icon missing'
print('PASS Vulkan: inventory, cart, vending, trade and invitation, mail, equipment inspection, context menu, guild invitation and vendor board render without QML errors')

import socket
import threading
expected={
    'inventory':bytes.fromhex('26010200030000002701020003000000'),
    'vending':b'',
    'trade':bytes.fromhex('e800020003000000eb00ef00'),
    'context':bytes.fromhex('e400e7030000'),
    'prompt':bytes.fromhex('6b010700000001000000'),
    'shopboard':bytes.fromhex('3001e703000034010c00e703000001000200'),
}
for case,wire in expected.items():
    target=build/(case+'-input');target.mkdir(exist_ok=True)
    (target/'open-midgard.ini').write_text('[Graphics]\nWidth=1280\nHeight=800\nSSAO=0\nRenderScale=100\nTextureUpscale=1\nWindowMode=0\n[Renderer]\nBackend=3\n')
    listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(15)
    captured=bytearray()
    def collect():
        with listener:
            conn,_=listener.accept()
            with conn:
                conn.settimeout(15)
                while True:
                    try:data=conn.recv(4096)
                    except ConnectionResetError:break # Fixture process shuts down Winsock.
                    if not data:break
                    captured.extend(data)
    thread=threading.Thread(target=collect,daemon=True);thread.start()
    runenv={k:v for k,v in os.environ.items() if not k.startswith('RO_HEAL_PREVIEW_')}
    runenv.update(OPEN_MIDGARD_DATA_DIR=str(target),RO_HEAL_PREVIEW_OUT=str(target/'frame.bmp'),RO_HEAL_PREVIEW_ASSETS='D:/Spel/OldRO',RO_HEAL_PREVIEW_SELF_HOVER='1',RO_HEAL_PREVIEW_COMMERCE=case,RO_HEAL_PREVIEW_COMMERCE_INPUT=case,RO_HEAL_PREVIEW_COMMERCE_PORT=str(listener.getsockname()[1]))
    with (target/'stderr.log').open('w') as errors:
        process=subprocess.Popen([str(client)],env=runenv,stderr=errors);assert process.wait(timeout=40)==0
    thread.join(timeout=5);assert not thread.is_alive()
    log=client_log(process.pid)
    (target/'client.log').write_text(log)
    assert f'[CommerceInputPreview] case={case} passed=1' in log,log[-6000:]
    # The hover fixture can also request its actor name using packetver23's
    # padded 11-byte 008c. Inspect every frame; exclude only that known request.
    at=0;commerce_wire=bytearray();sizes={0x8c:11,0x126:8,0x127:8,0xe8:8,0xeb:2,0xef:2,0xe4:6,0x130:6,0x134:12,0x16b:10}
    while at<len(captured):
        op=int.from_bytes(captured[at:at+2],'little');assert op in sizes,(case,captured.hex(),hex(op))
        size=sizes[op];assert at+size<=len(captured)
        if op!=0x8c:commerce_wire.extend(captured[at:at+size])
        at+=size
    assert bytes(commerce_wire)==wire,(case,captured.hex(),wire.hex())
print('PASS native input + loopback: bidirectional inventory/cart drag and quantity, cart/vending drag and drag-out removal, trade offer/lock/commit and player-menu trade, shop-bubble browsing and purchases send exact packets')
