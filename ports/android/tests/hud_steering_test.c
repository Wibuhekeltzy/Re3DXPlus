#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "hud.h"
#include "hud_steering_extracted.h"
static int keys_pause_only(void) { return 0; }
static void test_return_rate(int hz, float sign) {
  hud_state h; car_input in = {0};
  hud_init(&h, 1024, 768); hud_set_steer_mode(&h, HUD_STEER_WHEEL);
  h.wheel_turn = sign * WHEEL_LOCK; h.steer_v = sign;
  int n = 0;
  while (h.wheel_turn != 0 && n < hz) { hud_input_dt(&h, &in, 1.0f / hz); ++n; }
  assert(n == hz / 3);
  assert(in.steer == 0 && in.analog);
}
static void test_bad_dt(void) {
  hud_state h; car_input in = {0};
  hud_init(&h, 1024, 768); hud_set_steer_mode(&h, HUD_STEER_WHEEL);
  h.wheel_turn = WHEEL_LOCK; h.steer_v = 1;
  const float invalid[] = {0, -1, NAN, INFINITY, -INFINITY};
  for (unsigned i=0; i<sizeof invalid/sizeof invalid[0]; ++i) {
    hud_input_dt(&h, &in, invalid[i]); assert(h.wheel_turn == WHEEL_LOCK);
  }
  hud_input_dt(&h, &in, 10); assert(fabsf(in.steer - 0.7f) < 1e-5f);
  h.wheel_turn = WHEEL_LOCK; h.steer_v=1;
  hud_input(&h, &in); assert(fabsf(in.steer - 0.95f) < 1e-5f);
}
int main(void) {
  for (int hz=30; hz<=120; hz*=2) { test_return_rate(hz, 1); test_return_rate(hz, -1); }
  test_bad_dt();
  hud_state h; car_input in={0}; void *a=(void*)1, *b=(void*)2;
  hud_init(&h,1024,768); hud_set_steer_mode(&h,HUD_STEER_STICK);
  hud_touch(&h,a,TOUCH_BEGAN,100,500); assert(h.steer_touch==a);
  hud_touch(&h,b,TOUCH_BEGAN,140,500); assert(h.steer_touch==a);
  hud_touch(&h,b,TOUCH_MOVED,230,500); assert(h.steer_v==0);
  hud_touch(&h,a,TOUCH_MOVED,105,500); assert(h.steer_v==0);
  hud_touch(&h,a,TOUCH_MOVED,190,500); hud_input(&h,&in); assert(in.analog && in.steer>.99f);
  hud_touch(&h,b,TOUCH_BEGAN,950,650); assert(h.throttle_touch==b);
  hud_touch(&h,a,TOUCH_CANCELLED,0,0); hud_input(&h,&in); assert(in.steer==0 && in.throttle==1);
  hud_touch(&h,b,TOUCH_ENDED,0,0);
  hud_touch(&h,a,TOUCH_BEGAN,100,500); hud_touch(&h,a,TOUCH_MOVED,10,500);
  assert(h.steer_v < -.99f); hud_touch(&h,a,TOUCH_ENDED,0,0); assert(h.steer_v==0);
  hud_set_steer_mode(&h,HUD_STEER_WHEEL); hud_touch(&h,a,TOUCH_BEGAN,253,640);
  hud_touch(&h,a,TOUCH_MOVED,141,752); hud_input(&h,&in); assert(in.steer>.99f);
  hud_touch(&h,a,TOUCH_ENDED,0,0); hud_input(&h,&in); assert(in.steer>.9f && in.steer<1);
  hud_touch(&h,a,TOUCH_BEGAN,253,640); hud_touch(&h,a,TOUCH_CANCELLED,0,0); hud_input(&h,&in); assert(in.steer==0);
  EDGE=100; KEY_K=1; hud_layout_set(HUD_KEYS_LEFT,1,40,-30);
  assert(hud_layout_hit_mode(HUD_STEER_STICK,-55,405)==HUD_KEYS_LEFT);
  hud_set_steer_mode(&h,HUD_STEER_STICK); hud_touch(&h,a,TOUCH_BEGAN,40,470); assert(h.steer_touch==a);
  hud_set_steer_mode(&h,HUD_STEER_GYRO); assert(!h.steer_touch); h.tilt=.5f; h.full=.5f; hud_input(&h,&in); assert(in.steer>.99f);
  float dx=-1000,dy=1000; hud_layout_clamp_mode(HUD_KEYS_LEFT,HUD_STEER_WHEEL,1,&dx,&dy);
  hud_layout_set(HUD_KEYS_LEFT,1,dx,dy); rect r=key(group_rect_mode(KEY_LEFT,HUD_STEER_WHEEL),KEY_LEFT);
  assert(r.x>=-100.01f && r.y+r.h<=768.01f);
  puts("HUD steering host tests passed"); return 0;
}
