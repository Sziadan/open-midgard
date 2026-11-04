"""Compile production character-creation mouse/action handlers with a headless window stub."""
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
    source = (ROOT / 'src/ui/UIMakeCharWnd.cpp').read_text(encoding='utf-8')
    methods = [
        'bool UIMakeCharWnd::HandleQtMouseDown(',
        'bool UIMakeCharWnd::HandleQtMouseUp(',
        'bool UIMakeCharWnd::GetQtButtonDisplayForQt(',
        'bool UIMakeCharWnd::HitQtButton(',
        'void UIMakeCharWnd::SetQtPressedButtonIndex(',
        'msgresult_t UIMakeCharWnd::SendMsg(',
    ]
    with tempfile.TemporaryDirectory(prefix='ro_make_char_actions_') as directory:
        build = Path(directory)
        (build / 'UIMakeCharActionBodies.inc').write_text(
            '\n'.join(function(source, signature) for signature in methods), encoding='utf-8')
        executable = build / 'ui_make_char_action_checks.exe'
        subprocess.run([
            compiler, '-std=c++17', '-UNDEBUG', f'-I{build}',
            str(ROOT / 'tools/tests/UIMakeCharActionTests.cpp'), '-o', str(executable),
        ], check=True)
        environment = os.environ.copy()
        environment['PATH'] = str(Path(compiler).resolve().parent) + os.pathsep + environment.get('PATH', '')
        subprocess.run([str(executable)], check=True, env=environment)
    print('Production character-creation handlers: matched confirm/cancel releases and name/stat submit data passed')


if __name__ == '__main__':
    main()
