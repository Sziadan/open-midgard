"""Native flag emblem, legacy ownership and pre-render hit-cache regression."""
from pathlib import Path
import os
import subprocess
from PIL import Image, ImageChops

root=Path(__file__).resolve().parents[2]
exe=root/'build-mingw-preset/open-midgard.exe'
out=root/'build-mingw-preset/guild-flag-check'
out.mkdir(exist_ok=True)
source=(root/'src/gamemode/GameMode.cpp').read_text(encoding='utf-8')
def function(signature):
    start=source.index(signature);brace=source.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
(out/'GuildFlagInteraction.inc').write_text('\n'.join(function(signature) for signature in ('bool IsNpcLikeHoverActor(', 'bool RequestNpcContact(', 'bool TryRequestNpcTalkFromScreenPoint(')))
compiler=Path('C:/Qt/Tools/mingw1310_64/bin/g++.exe')
subprocess.run([str(compiler),'-std=c++17','-DNOMINMAX','-O2','-I'+str(root/'src'),'-I'+str(out),str(root/'tools/tests/GuildFlagInteractionTests.cpp'),'-o',str(out/'interaction.exe')],check=True)
testenv=dict(os.environ);testenv['PATH']=str(compiler.parent)+os.pathsep+testenv.get('PATH','')
subprocess.run([str(out/'interaction.exe')],env=testenv,check=True)
emblem=Image.new('RGB',(24,24),(255,0,255))
for y in range(3,21):
    for x in range(3,21):emblem.putpixel((x,y),(40,180,245) if x<12 else (255,220,40))
emblem.save(out/'emblem.bmp')
frames={}
for case,job in [('blank',722),('owned',722),('guardian',1286),('lever',111)]:
    target=out/case;target.mkdir(exist_ok=True)
    (target/'open-midgard.ini').write_text('[Graphics]\nWidth=800\nHeight=600\nSSAO=0\nRenderScale=100\nTextureUpscale=1\nWindowMode=0\n[Renderer]\nBackend=3\n')
    env={k:v for k,v in os.environ.items() if not k.startswith('RO_HEAL_PREVIEW_')}
    env.update(OPEN_MIDGARD_DATA_DIR=str(target),RO_HEAL_PREVIEW_OUT=str(target/'frame.bmp'),RO_HEAL_PREVIEW_ASSETS='D:/Spel/OldRO',RO_HEAL_PREVIEW_PLAYER='1',RO_HEAL_PREVIEW_MONSTER=str(job),RO_HEAL_PREVIEW_MONSTER_FRAMES='3',RO_HEAL_PREVIEW_MONSTER_AGE='0')
    if case=='owned':env['RO_HEAL_PREVIEW_FLAG_EMBLEM']=str(out/'emblem.bmp')
    with (target/'stderr.log').open('w') as errors:
        process=subprocess.Popen([str(exe)],env=env,stderr=errors)
        assert process.wait(timeout=40)==0
    log=(exe.parent/f'debug_hp_{process.pid}.log').read_text(errors='replace')
    log=log[log.rfind('[Build] marker='):]
    (target/'client.log').write_text(log)
    if case=='lever':assert '[HiddenNpcPreview] pick=1 picked=17 sprite=0' in log,log[-4000:]
    else:assert f'[MonsterPreview] job={job} rendered=1 pick=1 picked=17' in log,log[-4000:]
    assert '[MonsterPreview] proprietaryRuntimeLoaded=0' in log
    assert '[MonsterPreview] instanceCleanup=1' in log
    if case=='owned':
        assert '[GuildFlagPreview] ownership=1 texture=1 isolated=1 legacy=1 clear=1 unspecified=1' in log,log[-5000:]
        assert '[GuildFlag] rendered emblem actor=17 guild=65543 version=3' in log
        assert '[GuildFlagPreview] referencePadding=1' in log
    else:assert '[GuildFlag] rendered emblem' not in log
    combined=log+(target/'stderr.log').read_text(errors='replace')
    for error in ('VUID-','Validation Error','QML error:','ReferenceError:','TypeError:'):
        assert error not in combined
    frames[case]=Image.open(target/'frame.bmp').convert('RGB')
    frames[case].save(target/'frame.png')
assert ImageChops.difference(frames['blank'],frames['owned']).getbbox(), 'Emblem did not affect the rendered flag'
print('PASS Vulkan: version-zero flag downloads, legacy owner words, emblem mesh, unowned flag, cached picking, guardians and invisible lever interaction')
