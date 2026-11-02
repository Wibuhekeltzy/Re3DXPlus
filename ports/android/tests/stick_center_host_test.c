#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "hud.h"
#include "stick_center_store.h"
#include "hud_steering_extracted.h"
static int keys_pause_only(void) { return 0; }
static int near(float x,float y) { return fabsf(x-y)<0.0001f; }
int main(int argc,char **argv) {
  (void)argc; hud_state h; void *a=(void*)1,*b=(void*)2;
  EDGE=0; KEY_K=1; hud_layout_set(HUD_KEYS_LEFT,1,0,0);
  hud_init(&h,1024,768); hud_set_steer_mode(&h,HUD_STEER_STICK);
  hud_touch(&h,a,TOUCH_BEGAN,100,500); assert(h.steer_touch==a && near(h.stick_x0,100) && near(h.steer_v,0));
  hud_touch(&h,a,TOUCH_MOVED,145,500); assert(near(h.steer_v,(.5f-.08f)/.92f));
  hud_set_stick_center_mode(&h,HUD_STICK_FIXED); assert(!h.steer_touch && h.steer_v==0);
  hud_touch(&h,a,TOUCH_BEGAN,252,640); assert(!h.steer_touch);
  hud_touch(&h,a,TOUCH_BEGAN,141,640); assert(h.steer_touch==a && near(h.steer_v,0));
  hud_touch(&h,b,TOUCH_BEGAN,180,640); assert(h.steer_touch==a);
  hud_touch(&h,b,TOUCH_MOVED,230,640); assert(h.steer_v==0);
  hud_touch(&h,a,TOUCH_MOVED,148.2f,640); assert(h.steer_v<.00001f);
  hud_touch(&h,a,TOUCH_MOVED,151,640); assert(h.steer_v>0);
  hud_touch(&h,a,TOUCH_MOVED,231,730); assert(near(h.stick_x,141+90/sqrtf(2)) && near(h.stick_y,640+90/sqrtf(2)));
  hud_touch(&h,a,TOUCH_CANCELLED,0,0); assert(!h.steer_touch && !h.steer_v);
  hud_touch(&h,a,TOUCH_BEGAN,186,640); assert(h.steer_touch==a && near(h.steer_v,(.5f-.08f)/.92f));
  hud_touch(&h,a,TOUCH_ENDED,0,0); assert(!h.steer_touch && !h.steer_v);
  hud_set_stick_deadzone_percent(50);
  hud_touch(&h,a,TOUCH_BEGAN,186,640); assert(h.steer_touch==a && h.steer_v==0);
  hud_touch(&h,a,TOUCH_MOVED,208.5f,640); assert(near(h.steer_v,.5f));
  hud_touch(&h,a,TOUCH_ENDED,0,0); hud_set_stick_deadzone_percent(0);
  hud_touch(&h,a,TOUCH_BEGAN,251,640); assert(h.steer_touch==a && near(h.steer_v,1));
  hud_set_stick_center_mode(&h,HUD_STICK_FLOAT); assert(!h.steer_touch && !h.steer_v);
  hud_set_stick_center_mode(&h,HUD_STICK_FIXED); hud_set_stick_deadzone_percent(8);
  EDGE=100; KEY_K=.8f; hud_layout_set(HUD_KEYS_LEFT,1.5f,40,-30);
  rect center=key((rect){141,640,1,1},KEY_LEFT);
  hud_touch(&h,a,TOUCH_BEGAN,center.x+54,center.y); assert(h.steer_touch==a && near(h.steer_v,(.5f-.08f)/.92f));
  hud_set_steer_mode(&h,HUD_STEER_KEYS); assert(!h.steer_touch && !h.steer_v);
  hud_set_steer_mode(&h,HUD_STEER_STICK); hud_set_stick_center_mode(&h,HUD_STICK_FLOAT);
  int mode=9; assert(stick_store_load(argv[1],&mode)==STICK_STORE_MISSING && mode==0);
  assert(stick_store_save(argv[1],1,0)==STICK_STORE_EDIT_REQUIRED);
  assert(stick_store_save(argv[1],1,1)==STICK_STORE_OK);
  assert(stick_store_load(argv[1],&mode)==STICK_STORE_OK && mode==1);
  FILE *f=fopen(argv[1],"wb"); assert(f); fputs("STICK_CENTER 2\nmode=0\n",f); fclose(f);
  assert(stick_store_load(argv[1],&mode)==STICK_STORE_UNKNOWN && mode==0);
  assert(stick_store_save(argv[1],0,1)==STICK_STORE_UNKNOWN);
  f=fopen(argv[1],"rb"); char buf[64]={0}; assert(f && fread(buf,1,sizeof buf,f)>0); fclose(f); assert(!strcmp(buf,"STICK_CENTER 2\nmode=0\n"));
  f=fopen(argv[1],"wb"); assert(f); fputs("broken\n",f); fclose(f);
  assert(stick_store_save(argv[1],1,1)==STICK_STORE_UNKNOWN);
  assert(stick_store_save("/no/such/directory/stick",1,1)==STICK_STORE_IO);
  puts("stick center host tests passed"); return 0;
}
