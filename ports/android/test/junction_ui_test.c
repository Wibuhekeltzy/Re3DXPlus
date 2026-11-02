#include "../src/junction_ui.h"
#include "../src/anm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int draws, missing=-1, missing_anim=-1, anim, frame, count, flash_frame; static anm_swap swaps[4];
int anm_find(const char *s) { int a=!strcmp(s,"cons_g_bunki_mes_select") ? 1 : !strcmp(s,"cons_g_bunki_select") ? 2 : !strcmp(s,"cons_g_bunki_flash") ? 3 : -1; return a==missing_anim ? -1 : a; }
int anm_sprite(int s) { return s==missing ? -1 : s; }
int anm_frames(int a) { return a==1 ? 60 : 200; }
void anm_draw_ex(int a,int f,float x,float y,float z,const anm_swap *s,int n) { (void)x;(void)y;(void)z; draws++;anim=a;frame=f;count=n;memcpy(swaps,s,n*sizeof *s); }
void anm_draw(int a,int f,float x) { (void)a;(void)x;flash_frame=f;draws++; }
static void put(unsigned char *p, unsigned v) { for(int i=0;i<4;i++) p[i]=(v>>(i*8))&255; }
int main(void) {
  unsigned char b[384]={0}; junction_ui u;
  for(int i=0;i<16;i++) { put(b+i*24,100+i);put(b+i*24+4,0);put(b+i*24+8,253);put(b+i*24+12,200+i);put(b+i*24+16,2);put(b+i*24+20,254); }
  assert(!junction_ui_init(&u,b,383)); assert(junction_ui_init(&u,b,sizeof b));
  junction_ui_tick(&u,1,100,200,1,0,0);assert(u.timer==79); assert(junction_ui_draw(&u)&&anim==1&&count==1&&swaps[0].to==102246);
  int timer=u.timer; for(int i=0;i<100;i++)junction_ui_tick(&u,1,100,200,1,0,1);assert(u.timer==timer);
  for(int i=0;i<78;i++)junction_ui_tick(&u,1,100,200,1,0,0); assert(u.timer==1);
  junction_ui_tick(&u,1,100,200,1,0,0); assert(u.timer==0); assert(junction_ui_draw(&u)&&anim==2&&count==4);
  assert(swaps[0].to==102253&&swaps[1].to==102254&&swaps[2].to==102248&&swaps[3].to==102250);
  missing=102253; draws=0; assert(!junction_ui_draw(&u)&&draws==0);missing=-1;
  junction_ui_tick(&u,2,100,200,0,0,0);assert(u.timer==74); draws=0; assert(junction_ui_draw(&u)&&draws==2&&frame>=60&&frame<160);
  for(int i=0;i<74;i++)junction_ui_tick(&u,2,100,200,0,0,0);assert(!u.visible&&!junction_ui_draw(&u));
  junction_ui_reset(&u);for(int i=0;i<80;i++)junction_ui_tick(&u,1,100,100,0,0,0); assert(u.left_kind==1&&u.right_kind==3&&u.left_name==252&&u.right_name==252);
  junction_ui_tick(&u,3,100,100,0,1,0); draws=0;assert(junction_ui_draw(&u)&&draws==1&&frame>=160);
  junction_ui_reset(&u); assert(!u.visible && u.initialized);
  for(int pass=0;pass<5;pass++) {
    for(int i=0;i<80;i++) junction_ui_tick(&u,1,101,201,0,0,0);
    assert(u.left_area==101 && u.right_area==201 && u.left_name==253 && u.right_name==254);
    junction_ui_tick(&u,0,0,0,0,0,0); junction_ui_reset(&u);
  }
  /* Fast-confirm fallback: either side at prompt tick 1 or 79. */
  for(int elapsed_case=0;elapsed_case<2;elapsed_case++) for(int side=2;side<=3;side++) {
    int prompt_ticks=elapsed_case ? 79 : 1;
    junction_ui_reset(&u);
    for(int i=0;i<prompt_ticks;i++) junction_ui_tick(&u,1,100,200,0,0,0);
    assert(!u.panel_ready && u.visible);
    junction_ui_tick(&u,side,101,201,0,0,0);
    assert(u.visible && u.panel_ready && u.timer==74 && u.left_area==101 && u.right_area==201);
    draws=0; assert(junction_ui_draw(&u) && draws==2 && anim==2 && count==4);
    assert(swaps[0].to==102253 && swaps[1].to==102254);
    assert(frame==(side==2 ? 60 : 160) && flash_frame==(side==2 ? 1 : 101));
    missing=102253; draws=0; assert(!junction_ui_draw(&u) && draws==0);
    missing=-1; draws=0; assert(junction_ui_draw(&u) && draws==2);
    missing_anim=3; draws=0; assert(junction_ui_draw(&u) && draws==1 && anim==2); missing_anim=-1;
    junction_ui_tick(&u,side,100,200,0,0,0);
    assert(u.timer==73 && u.left_area==100 && u.right_area==200);
    for(int i=0;i<73;i++) junction_ui_tick(&u,side,101,201,0,0,0);
    assert(!u.visible && !junction_ui_draw(&u));
    junction_ui_reset(&u); assert(!u.visible && u.initialized && !u.panel_ready);
  }
  /* Inactive always disappears, including prompt, ready panel and confirmation. */
  for(int phase=0;phase<3;phase++) {
    junction_ui_reset(&u);
    for(int i=0;i<(phase==0?1:80);i++) junction_ui_tick(&u,1,100,200,0,0,0);
    if(phase==2) junction_ui_tick(&u,2,100,200,0,0,0);
    junction_ui_tick(&u,0,100,200,0,0,0);
    if(phase==0) assert(!u.visible);
    else { assert(u.visible && u.timer==14); for(int i=0;i<14;i++) junction_ui_tick(&u,0,100,200,0,0,0); assert(!u.visible); }
  }
  /* Area changes in a stable displayed state refresh the ordered pair. */
  for(int state=1;state<=3;state++) {
    junction_ui_reset(&u);
    for(int i=0;i<80;i++) junction_ui_tick(&u,1,100,200,0,0,0);
    if(state!=1) junction_ui_tick(&u,state,100,200,0,1,0);
    junction_ui_tick(&u,state,100,100,0,1,0);
    assert(u.left_area==100 && u.right_area==100 && u.left_name==252 && u.right_name==252);
    draws=0; assert(junction_ui_draw(&u) && swaps[0].to==102252 && swaps[1].to==102252);
  }
  put(b+8,276); assert(!junction_ui_init(&u,b,sizeof b));
  puts("junction_ui mock tests passed");return 0;
}
