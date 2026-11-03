"""Compile shared geometry and production Qt login input/button functions."""
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
    source = (ROOT / 'src/ui/UILoginWnd.cpp').read_text(encoding='utf-8')
    with tempfile.TemporaryDirectory(prefix='ro_login_layout_') as directory:
        build = Path(directory)
        (build / 'LoginLayoutBodies.inc').write_text('\n'.join(function(source, name) for name in [
            'loginLayout::Layout UILoginWnd::GetQtLayout()', 'int UILoginWnd::GetQtButtonCount()',
            'bool UILoginWnd::GetQtButtonDisplayForQt(', 'bool UILoginWnd::HandleQtMouseDown(']), encoding='utf-8')
        executable = build / 'login_layout_checks.exe'
        subprocess.run([compiler, '-std=c++17', '-UNDEBUG', f'-I{build}', f'-I{ROOT / "src"}',
                        str(ROOT / 'tools/tests/LoginLayoutTests.cpp'), '-o', str(executable)], check=True)
        environment = os.environ.copy()
        environment['PATH'] = str(Path(compiler).resolve().parent) + os.pathsep + environment.get('PATH', '')
        subprocess.run([str(executable)], check=True, env=environment)
    print('Production login: responsive geometry, field focus, remember-label clicks and all button hit areas passed')


if __name__ == '__main__':
    main()
