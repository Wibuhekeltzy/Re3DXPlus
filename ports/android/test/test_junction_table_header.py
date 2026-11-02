"""Synthetic header roundtrip with a private temporary output root."""
import importlib.util, re, subprocess, tempfile
from pathlib import Path

spec = importlib.util.spec_from_file_location('junction_pack', Path(__file__).resolve().parents[1] / 'tools/junction_pack.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
with tempfile.TemporaryDirectory(prefix='junction-header-test-') as tmp:
    base = Path(tmp)
    work = base / 'private'
    work.mkdir(mode=0o700)
    header = work / 'junction_table_private.h'
    original = bytes(i % 256 for i in range(384))
    m.write_table_header(header, original, work)
    data = header.read_text(encoding='ascii')
    vals = bytes(int(x, 16) for x in re.findall(r'0x([0-9a-f]{2})', data))
    assert vals == original
    try: m.write_table_header(header, original, work)
    except FileExistsError: pass
    else: raise AssertionError('existing header overwritten')
    assert header.read_text(encoding='ascii') == data
    try: m.write_table_header(work / 'bad.h', original[:-1], work)
    except ValueError: pass
    else: raise AssertionError('truncated input accepted')
    try: m.write_table_header(base / 'outside.h', original, work)
    except ValueError: pass
    else: raise AssertionError('outside path accepted')
    source = work / 'roundtrip.c'
    source.write_text('#include "junction_table_private.h"\nint main(void){for(int i=0;i<384;i++)if(junction_table_private[i]!=(unsigned char)i)return 1;return 0;}\n')
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', str(source), '-o', str(work / 'roundtrip')], check=True)
    subprocess.run([str(work / 'roundtrip')], check=True)
print('synthetic C header roundtrip/O_EXCL OK')
