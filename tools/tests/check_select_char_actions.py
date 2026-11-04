"""Compile the production character-select mouse/action handlers with a headless window stub."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from check_ammo_cursor_regressions import function

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cxx', required=True)
    args = parser.parse_args()
    compiler = shutil.which(args.cxx) or args.cxx
    source = (ROOT / 'src/ui/UISelectCharWnd.cpp').read_text(encoding='utf-8')
    methods = [
        'bool UISelectCharWnd::HandleQtMouseDown(',
        'bool UISelectCharWnd::HandleQtMouseUp(',
        'int UISelectCharWnd::FindCharacterIndexForSlot(',
        'msgresult_t UISelectCharWnd::SendMsg(',
        'void UISelectCharWnd::ActivateSelectedSlot(',
        'void UISelectCharWnd::ActivateCreate(',
        'void UISelectCharWnd::ActivateDelete(',
    ]
    with tempfile.TemporaryDirectory(prefix='ro_select_char_actions_') as directory:
        build = Path(directory)
        (build / 'UISelectCharActionBodies.inc').write_text(
            '\n'.join(function(source, signature) for signature in methods), encoding='utf-8')
        executable = build / 'ui_select_char_action_checks.exe'
        subprocess.run([
            compiler, '-std=c++17', '-UNDEBUG', f'-I{build}',
            str(ROOT / 'tools/tests/UISelectCharActionTests.cpp'), '-o', str(executable),
        ], check=True)
        environment = os.environ.copy()
        environment['PATH'] = str(Path(compiler).resolve().parent) + os.pathsep + environment.get('PATH', '')
        subprocess.run([str(executable)], check=True, env=environment)
    print('Production character-select handlers: select, create, delete, cancel, hidden-window and outside-click cases passed')


if __name__ == '__main__':
    main()
