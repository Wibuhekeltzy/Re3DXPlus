#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../src/hud.h"
#define WMMT_HUD_CUSTOM_TEST 1
#include "../src/menu.h"
static int race_calls, shift_calls, inject_nan;
static float edge;
float hud_race_layout_max_scale(int group) { (void)group; return 1.2f; }
void hud_race_layout_clamp(int g,float k,float *x,float *y) {
  assert(g>=0 && g<HUD_RACE_GROUPS && k>=0.6f && k<=1.6f); race_calls++;
  if (inject_nan && g==HUD_RACE_MAP) { *x=NAN; *y=INFINITY; return; }
  *x=fmaxf(-edge-100,fminf(edge+100,*x)); *y=fmaxf(-200,fminf(200,*y));
}
void hud_layout_clamp_mode(int group,int mode,float k,float *x,float *y) {
  assert(group==HUD_EDIT_SHIFT && mode==HUD_STEER_STICK && k>0); shift_calls++;
  *x=fmaxf(-edge-50,fminf(edge+50,*x)); *y=fmaxf(-70,fminf(70,*y));
}
#include "hud_custom_clamp_under_test.inc"
int main(void) {
  HudCustomLayout a; hud_custom_default(&a);
  a.map.dx=2000; a.rank.dy=-2000; a.shift.dx=1900; a.shift_separate=0;
  edge=0; custom_layout_clamp_visible(&a,HUD_STEER_STICK);
  assert(race_calls==4 && !shift_calls && a.map.dx==100 && a.rank.dy==-200 && a.shift.dx==1900);
  hud_custom_default(&a); a.map.dx=2000; a.shift.dx=-2000; a.shift_separate=1;
  edge=120.4f; custom_layout_clamp_visible(&a,HUD_STEER_STICK);
  assert(race_calls==6 && shift_calls==2 && a.map.dx==220 && a.shift.dx==-170);
  inject_nan=1; custom_layout_clamp_visible(&a,HUD_STEER_STICK);
  assert(a.map.dx==0 && a.map.dy==0 && hud_custom_validate(&a));
  HudCustomLayout base, view, merged, loaded; hud_custom_default(&base);
  base.map=(HudCustomTransform){160,1900,-1900}; base.rank.dx=1800; base.shift_separate=1; base.shift.dx=1700;
  view=base; inject_nan=0; custom_layout_clamp_visible(&view,HUD_STEER_STICK);
  assert(view.map.k==120 && view.map.dx!=base.map.dx && view.timer.k==100 && !view.timer.dx && !view.timer.dy);
  view.stick_deadzone_percent=9; merged=custom_layout_merge(base,&view,CUSTOM_DIRTY_DEADZONE);
  assert(merged.map.dx==1900 && merged.map.k==160 && merged.rank.dx==1800 && merged.shift.dx==1700);
  char p[]="/tmp/wmmt-sidecar-XXXXXX"; int fd=mkstemp(p); assert(fd>=0); close(fd); unlink(p);
  assert(hud_custom_save(p,&merged,1)==HUD_CUSTOM_OK && hud_custom_load(p,&loaded)==HUD_CUSTOM_OK);
  assert(loaded.map.dx==1900 && loaded.shift.dx==1700 && loaded.stick_deadzone_percent==9);
  view.map.dx=42; merged=custom_layout_merge(loaded,&view,CUSTOM_DIRTY_MAP);
  assert(hud_custom_save(p,&merged,1)==HUD_CUSTOM_OK && hud_custom_load(p,&loaded)==HUD_CUSTOM_OK);
  assert(loaded.map.dx==42 && loaded.rank.dx==1800 && loaded.shift.dx==1700);
  assert(hud_custom_save("/definitely-missing-wmmt-dir/file",&merged,1)==HUD_CUSTOM_IO_ERROR);
  unlink(p); puts("custom clamp ok"); return 0;
}
