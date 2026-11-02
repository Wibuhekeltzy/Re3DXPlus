#include "glstub.h"
#define WMMT_GL_H
#define WMMT_BGM_TEST
#include "../src/menu.c"
#include <assert.h>
static int mask=6, meter_ready=1;
int android_bgm_group_available(int g){return (mask&(1<<g))!=0;}
int hud_meter_available(void){return meter_ready;}
void snd_menu(int x){(void)x;}
int anm_sprite(int id){return id;}
int anm_sprite_rect(int anim,int frame,int sprite,float r[4]){(void)anim;if(sprite==T_ARROW || sprite==T_ARROW2 || (sprite==T_NIGHT && frame<100) || (sprite==T_MORNING && frame>=100)) return 0;r[0]=0;r[1]=0;r[2]=100;r[3]=100;return 1;}
static void story_bgm_checks(menu_state *m) {
  m->vs.state=MENU_VS_OFF; m->vs.day=0; m->story_day_select=1; m->zones=1;
  m->zone=0; m->bgm=5; m->bgm_source=0; m->song=7; m->sel=7;
  assert(!day_select_step(m,0,1) && m->sel==8 && m->song==7);
  m->tap_x=50; m->tap_y=50; assert(day_select_step(m,1,0));
  assert(m->song==8 && m->story_zone==0 && m->bgm==5 && m->bgm_source==0);
  /* Browsing then backing out does not commit either track or preference. */
  m->song=4; m->sel=4; m->tap_x=200; m->tap_y=350;
  assert(!day_select_step(m,1,0) && m->sel==5 && m->song==4 && m->bgm==5);
  m->tap_x=50; m->tap_y=50; m->sel=0; assert(day_select_step(m,1,0));
  assert(m->song>=1 && m->song<=card_bgm_lines(m)-2 && m->bgm==5);
  m->sel=card_bgm_lines(m)-1; assert(day_select_step(m,1,0) && m->song==-1 && m->bgm==5);
  /* A new race gets an independent selection; TA still updates its preference. */
  m->song=3; m->sel=3; assert(day_select_step(m,1,0) && m->song==3);
  m->zones=2; assert(!day_select_step(m,0,1) && m->slide==SLIDE);
  for(int i=0;i<SLIDE;i++) assert(!day_select_step(m,0,0));
  assert(m->zone==1 && m->song==3); m->sel=4;
  assert(day_select_step(m,1,0) && m->story_zone==1 && m->song==4 && m->bgm==5);
  m->zones=1; m->story_day_select=0; m->sel=2; assert(day_select_step(m,1,0));
  assert(m->song==2 && m->bgm==2);
}
int app_card_choice_commit(menu_state *m,int row,int choice,long long born){m->card_born[m->maker][m->car]=born;if(row)m->sound_car[m->maker][m->car]=choice;else m->meter_car[m->maker][m->car]=choice;return 1;}
int main(void){menu_state m={0};m.n_own=1;m.own_sel=0;m.maker=0;m.car=0;m.own[0].maker=0;m.own[0].car=0;m.own[0].born=42;
/* All meters are usable for an owned car with resources, even without bonus bits. */
assert(menu_card_choice_available(&m,0,1) && menu_card_choice_available(&m,0,2));
assert(menu_card_choice_set(&m,0,2) && menu_card_meter(&m)==2 && !m.st_bonus_car[0][0]);
meter_ready=0;assert(!menu_card_choice_available(&m,0,2) && menu_card_meter(&m)==0);
meter_ready=1;assert(menu_card_meter(&m)==2);
assert(menu_card_choice_available(&m,1,1));assert(menu_card_choice_set(&m,1,1));assert(!m.st_bonus_car[0][0] && card_bgm_lines(&m)==25);
for(int i=1;i<=23;i++){char e[16];snprintf(e,sizeof e,"wm2_%02d",i);assert(!strcmp(card_bgm_file(&m,i),e));}
/* Simulate the native availability bit after a failed track; do not read device files. */
m.song=23;assert(menu_bgm(&m,1)==SONG_FILE_MT[23]);mask=4;assert(menu_card_bgm_source(&m)==0&&m.sound_car[0][0]==1);assert(menu_bgm(&m,1)==SONG_FILE[3]);assert(card_bgm_play_line(card_bgm_set(&m),m.song)==3);m.song=-1;assert(menu_bgm(&m,1)==NULL);mask=6;
for(int i=0;i<300;i++){m.sel=0;day_decide(&m);assert(m.song>=1&&m.song<=23);}m.sel=24;day_decide(&m);assert(m.song==-1);
assert(menu_card_choice_set(&m,1,2));assert(card_bgm_lines(&m)==14);for(int i=1;i<=12;i++){char e[16];snprintf(e,sizeof e,"wmr_%02d",i);assert(!strcmp(card_bgm_file(&m,i),e));}
m.sel=24;day_decide(&m);assert(m.bgm==0&&m.song>=1&&m.song<=12);mask=2;assert(menu_card_bgm_source(&m)==0&&m.sound_car[0][0]==2);
for(int i=21;i<=23;i++){assert(card_bgm_play_line(0,i)==i-20);assert(card_bgm_file(&m,i)==SONG_FILE[i-20]);}assert(!card_bgm_file(&m,-1));mask=6;assert(menu_card_bgm_source(&m)==2);
story_bgm_checks(&m);
 mask=2; story_bgm_checks(&m); mask=6; /* missing selected group falls back safely */
 m.bgm=21; m.bgm_source=0; assert(card_bgm_saved_line(&m)==13); /* NO BGM -> group 2 */
 m.bgm=13; m.bgm_source=2; mask=2; assert(card_bgm_saved_line(&m)==21); /* failure -> group 0 */
 mask=6; m.bgm=23; m.bgm_source=1; assert(card_bgm_saved_line(&m)==11); /* out-of-range song */
 m.vs.state=MENU_VS_LOBBY;assert(card_bgm_set(&m)==0&&card_bgm_lines(&m)==22);m.vs.state=MENU_VS_OFF;m.own[0].born=43;assert(!menu_card_bgm_source(&m)&&menu_card_choice_available(&m,1,2));puts("BGM group checks passed");}
