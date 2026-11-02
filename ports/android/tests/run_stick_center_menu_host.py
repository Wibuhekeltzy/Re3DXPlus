from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/menu.c').read_text(); a=s.index('enum { LAY_PAGE,'); b=s.index('static void layout_text_mid',a)
with tempfile.TemporaryDirectory() as d:
 p=Path(d); (p/'hud_custom_editor_under_test.inc').write_text(s[a:b])
 wrapper=r"""
#define main original_editor_test
#include "hud_custom_editor_test.c"
#undef main
int main(void) {
 original_editor_test();
 menu_state m; memset(&m,0,sizeof m); hud_custom_default(&m.custom_layout);
 m.screen=MENU_LAYOUT; m.gyro=HUD_STEER_STICK; m.lay_sel=HUD_KEYS_LEFT;
 for(int i=0;i<3;i++) m.key_k[i]=100;
 const float *r=LAY_BUTTON[LAY_CENTER]; float x=r[0]+10,y=r[1]+5;
 layout_touch(&m,(void*)1,TOUCH_BEGAN,x,y); layout_touch(&m,(void*)2,TOUCH_ENDED,x,y); layout_step(&m,0); assert(!m.stick_center_dirty);
 layout_touch(&m,(void*)1,TOUCH_CANCELLED,x,y); layout_step(&m,0); assert(!m.stick_center_dirty);
 press(&m,LAY_CENTER); assert(m.stick_center_mode==HUD_STICK_FIXED && m.stick_center_dirty);
 m.stick_center_dirty=0; m.lay_page=1; press(&m,LAY_CENTER); assert(!m.stick_center_dirty && m.stick_center_mode==HUD_STICK_FIXED);
 press(&m,LAY_RESET_ALL); assert(m.stick_center_mode==HUD_STICK_FIXED && !m.stick_center_dirty);
 m.lay_page=0; m.lay_sel=HUD_KEYS_LEFT; press(&m,LAY_RESET_ALL); assert(m.stick_center_mode==HUD_STICK_FLOAT && m.stick_center_dirty);
 puts("stick center menu tests passed");
}
"""
 (p/'test.c').write_text(wrapper)
 subprocess.run(['cc','-std=gnu11','-I',str(p),'-I',str(root/'test'),'-I',str(root/'src'),str(p/'test.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
