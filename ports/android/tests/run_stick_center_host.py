from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/hud.c').read_text()
prefix=s[s.index('typedef struct { float x, y, w, h; } rect;'):s.index('/* ---- drawing ---- */')]
a=prefix.index('/* the projection of the layout'); b=prefix.index("/* the gyro's dead zone",a); prefix=prefix[:a]+prefix[b:]
layout=s[s.index('static rect group_rect(int group, int gyro) {'):s.index('void hud_draw_layout_mode(')]
with tempfile.TemporaryDirectory() as t:
 p=Path(t); (p/'hud_steering_extracted.h').write_text(prefix+'\n'+layout)
 subprocess.run(['cc','-std=gnu11','-I',str(root/'src'),'-I',str(p),str(root/'tests/stick_center_host_test.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),str(p/'center')],check=True)
