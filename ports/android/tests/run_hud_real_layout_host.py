#!/usr/bin/env python3
"""Host integration: current menu editor with production HUD; no GPU execution.
Set HUD_GENERATED to the build's generated src directory and optionally CC.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
android = root / 'ports' / 'android'
generated = os.environ.get('HUD_GENERATED')
if not generated or not Path(generated).is_dir():
    sys.exit('Set HUD_GENERATED to the generated build-snapshot/src directory')
source = (android / 'src' / 'menu.c').read_text()
start = source.index('enum { LAY_PAGE,')
end = source.index('static void layout_text_mid', start)
with tempfile.TemporaryDirectory(prefix='hud-real-layout-') as d:
    temp = Path(d)
    (temp / 'real_editor.inc').write_text(source[start:end])
    exe = temp / 'test'
    cmd = [os.environ.get('CC', 'clang'), '-std=gnu11', '-U__APPLE__',
           '-DWMMT_HUD_CUSTOM_TEST', '-ffunction-sections', '-fdata-sections']
    for p in (temp, android / 'src', android / 'test', root / 'src', Path(generated)):
        cmd.extend(['-I', str(p)])
    cmd += [str(android / 'test' / 'hud_real_layout_test.c'),
            '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections',
            '-lz', '-lm', '-o', str(exe)]
    subprocess.run(cmd, check=True)
    subprocess.run([str(exe)], check=True)
