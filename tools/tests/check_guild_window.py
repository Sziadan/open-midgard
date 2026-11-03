"""Check guild packet parsing and native Qt guild tabs using isolated fixtures."""
import os
from pathlib import Path
import subprocess
import sys
from PIL import Image, ImageStat

root=Path(__file__).resolve().parents[2]
build=root/'build-mingw-preset/guild-window-check'
build.mkdir(exist_ok=True)
compiler=Path('C:/Qt/Tools/mingw1310_64/bin/g++.exe')
test=build/'packets.exe'
subprocess.run([str(compiler),'-std=c++17','-O2','-I'+str(root/'src'),str(root/'tools/tests/GuildWindowTests.cpp'),'-o',str(test)],check=True)
env={k:v for k,v in os.environ.items() if not k.startswith('RO_HEAL_PREVIEW_')}
env['PATH']=str(compiler.parent)+os.pathsep+env.get('PATH','')
subprocess.run([str(test)],env=env,check=True)

def body(source, marker):
    start=source.index(marker);brace=source.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[brace:end]

adapter=(root/'src/qtui/QtUiStateAdapter.cpp').read_text(encoding='utf-8')
window=(root/'src/ui/UIGuildWnd.cpp').read_text(encoding='utf-8')
(build/'GuildAction.inc').write_text('void GuildAction(const QString& action,const QVariantMap& args)'+body(adapter,'QObject::connect(m_state,&QtUiState::guildActionRequested,'))
(build/'GuildRefresh.inc').write_text('void UIGuildWnd::Refresh()'+body(window,'void UIGuildWnd::Refresh()'))
qt=Path('C:/Qt/6.11.0/mingw_64')
actions=build/'actions.exe'
subprocess.run([str(compiler),'-std=c++17','-O2','-I'+str(root/'src'),'-I'+str(build),'-I'+str(qt/'include'),'-I'+str(qt/'include/QtCore'),'-I'+str(qt/'include/QtGui'),str(root/'tools/tests/GuildActionTests.cpp'),'-L'+str(qt/'lib'),'-lQt6Gui','-lQt6Core','-o',str(actions)],check=True)
env['PATH']=str(qt/'bin')+os.pathsep+env['PATH']
subprocess.run([str(actions),*sys.argv[1:]],env=env,check=True)
exe=root/'build-mingw-preset/open-midgard.exe'
# This MinGW preset compiles Vulkan only; assert the active backend explicitly.
for backend in (3,):
    for tab in [*range(6),'member','none']:
        out=build/f'backend-{backend}-tab-{tab}';out.mkdir(exist_ok=True)
        (out/'open-midgard.ini').write_text(f'[Graphics]\nWidth=800\nHeight=600\nSSAO=0\nRenderScale=100\nTextureUpscale=1\nWindowMode=0\n[Renderer]\nBackend={backend}\n')
        env.update(OPEN_MIDGARD_DATA_DIR=str(out),RO_HEAL_PREVIEW_OUT=str(out/'frame.bmp'),RO_HEAL_PREVIEW_ASSETS='D:/Spel/OldRO',RO_HEAL_PREVIEW_SELF_HOVER='1',RO_HEAL_PREVIEW_GUILD_WINDOW=str(tab))
        if tab==0:env['RO_HEAL_PREVIEW_GUILD_INPUT']='1'
        else:env.pop('RO_HEAL_PREVIEW_GUILD_INPUT',None)
        with (out/'stderr.log').open('w') as err:
            p=subprocess.Popen([str(exe)],env=env,stderr=err);assert p.wait(timeout=40)==0
        log=(exe.parent/f'debug_hp_{p.pid}.log').read_text(errors='replace')
        # Debug logs append when Windows reuses a process id. Check this run.
        marker=log.rfind('[Build] marker=')
        if marker>=0:log=log[marker:]
        (out/'client.log').write_text(log)
        combined=log+(out/'stderr.log').read_text(errors='replace')
        assert '[GuildWindowPreview] altOpen=1 altClose=1' in log
        assert f'tab={tab} framed=1' in log
        assert '[GuildWindowPreview] visible=1 joined=' + ('0 canEdit=0' if tab=='none' else '1 canEdit=0' if tab=='member' else '1 canEdit=1') in log
        assert "Active backend confirmed as 'Vulkan'" in log
        if tab==0:assert '[GuildInputPreview] tabs=1 drag=1 unicodeEdit=1 wheel=1 close=1 title=1 escape=1 altShortcut=1' in log,log[-6000:]
        for failure in ('QML error:','ReferenceError:','TypeError:','Binding loop','VUID-','Validation Error'):
            assert failure not in combined,combined[-5000:]
        frame=Image.open(out/'frame.bmp').convert('RGB');frame.save(out/'frame.png')
        assert max(ImageStat.Stat(frame.crop((40,65,760,535))).stddev)>10,'Blank guild window'
    print('PASS Vulkan: Alt+G toggles, six live-data Qt tabs, member permissions and no-guild state render')
