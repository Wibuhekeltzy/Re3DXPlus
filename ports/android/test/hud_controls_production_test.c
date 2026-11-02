#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../src/hud.c"
static int near(float a,float b){return fabsf(a-b)<0.002f;}
static void same(rect a,rect b){assert(near(a.x,b.x)&&near(a.y,b.y)&&near(a.w,b.w)&&near(a.h,b.h));}
int main(void){
 void *a=(void*)1,*b=(void*)2; hud_state h; car_input in={0};
 hud_wide(140,1.2f); hud_layout_set(HUD_KEYS_RIGHT,1.15f,17,-9);
 for(int m=0;m<HUD_STEER_MODES;m++){
  rect base=m==HUD_STEER_GYRO?G_SHIFT_UP:SHIFT_UP;
  hud_shift_layout_set(0,.8f,-90,20); same(shift_key(base),key(base,KEY_RIGHT));
  rect r=shift_key(base); assert(hud_layout_hit_mode(m,r.x+r.w/2,r.y+r.h/2)==HUD_EDIT_SHIFT);
  hud_init(&h,1500,900); hud_set_steer_mode(&h,m); hud_touch(&h,a,TOUCH_BEGAN,r.x+r.w/2,r.y+r.h/2); assert(h.shift_up==1);
 }
 hud_shift_layout_set(1,.8f,-200,-80);
 for(int m=0;m<HUD_STEER_MODES;m++){
  rect base=m==HUD_STEER_GYRO?G_SHIFT_UP:SHIFT_UP; rect r=shift_key(base);
  same(r,place(base,KEY_RIGHT,.8f,-200,-80)); assert(hud_layout_hit_mode(m,r.x+r.w/2,r.y+r.h/2)==HUD_EDIT_SHIFT);
  hud_init(&h,1500,900); hud_set_steer_mode(&h,m); rect t=key(THROTTLE,KEY_RIGHT);
  hud_touch(&h,b,TOUCH_BEGAN,t.x+t.w/2,t.y+t.h/2); hud_touch(&h,a,TOUCH_BEGAN,r.x+r.w/2,r.y+r.h/2);
  assert(h.throttle_touch==b && h.shift_up==1); hud_touch(&h,a,TOUCH_CANCELLED,0,0); hud_input(&h,&in); assert(in.throttle==1 && in.shift_up);
  hud_touch(&h,b,TOUCH_CANCELLED,0,0); hud_input(&h,&in); assert(!in.throttle);
 }
 float x=9999,y=-9999; hud_layout_clamp_mode(HUD_EDIT_SHIFT,HUD_STEER_GYRO,1,&x,&y);
 rect cr=place(group_rect_mode(HUD_EDIT_SHIFT,HUD_STEER_GYRO),KEY_RIGHT,1,x,y);
 assert(cr.x>=-EDGE-.01f && cr.x+cr.w<=1024+EDGE+.01f && cr.y>=-.01f && cr.y+cr.h<=768.01f);
 hud_wide(0,1); hud_layout_set(HUD_KEYS_LEFT,1,0,0);
 for(int d=-1;d<=51;d++){
  hud_set_stick_deadzone_percent(d); int p=d<0?0:d>50?50:d;
  hud_init(&h,1024,768); hud_set_steer_mode(&h,HUD_STEER_STICK);
  hud_touch(&h,a,TOUCH_BEGAN,100,500); hud_touch(&h,b,TOUCH_BEGAN,150,500);
  hud_touch(&h,a,TOUCH_MOVED,100+90*p/100.f,500); assert(fabsf(h.steer_v)<.0001f);
  hud_touch(&h,a,TOUCH_MOVED,190,500); assert(near(h.steer_v,1));
  hud_touch(&h,a,TOUCH_MOVED,10,500); assert(near(h.steer_v,-1));
  hud_touch(&h,a,TOUCH_CANCELLED,0,0); assert(!h.steer_touch && !h.steer_v);
 }
 hud_set_steer_mode(&h,HUD_STEER_GYRO); h.tilt=.5f; h.full=.5f; hud_input(&h,&in); assert(near(in.steer,1));
 puts("production HUD control tests passed"); return 0;
}
