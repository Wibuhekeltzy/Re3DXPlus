/* Android story music through production menu_update, with host render/audio stubs. */
#define WMMT_BGM_TEST
#define main timezone_fixture_main
#include "story_timezone_host.c"
#undef main
int android_bgm_group_available(int group) { return group == 1 || group == 2; }
int hud_meter_available(void) { return 1; }
int app_card_choice_commit(menu_state *m, int row, int choice, long long born) {
  (void)m; (void)row; (void)choice; (void)born; return 0;
}
int main(void) {
  menu_state m;
  enter_story(&m);
  assert(m.sel == m.song && m.song >= 1 && m.song <= 20);
  m.bgm=7; m.bgm_source=0; m.ta_zone=1;
  slide_to_night(&m);
  int before=m.song, selected=m.sel;
  m.tap=1; m.tap_x=200; m.tap_y=350; tick(&m,1);
  assert(m.sel == (selected+1)%22 && m.song==before);
  selected=m.sel;
  m.tap=1; m.tap_x=m.tap_y=50; tick(&m,1);
  if (selected == 0) assert(m.song >= 1 && m.song <= 20);
  else assert(m.song == (selected==21 ? -1 : selected));
  int race_song=m.song;
  tick(&m,DECIDE_DAY+FADE);
  assert(m.screen==MENU_PRE && m.song==race_song && m.story_zone==1 && m.ta_zone==1 && m.bgm==7);
  m.screen=MENU_ST_RIVAL; m.game=MENU_GAME_STORY; m.from=-1; m.deciding=1; m.sel=1;
  tick(&m,1); assert(m.screen==MENU_BGM && m.sel==m.song && m.song>0 && m.bgm==7 && m.story_zone==1);
  before=m.song; m.tap=1; m.tap_x=200; m.tap_y=350; tick(&m,1);
  assert(m.song==before && m.bgm==7);
  m.back=1; tick(&m,1);
  assert(m.screen==MENU_ST_RIVAL && m.story_zone==1 && m.ta_zone==1 && m.bgm==7);
  enter_story(&m); m.zones=1; m.sel=0; m.bgm=9;
  m.tap=1; m.tap_x=m.tap_y=50; tick(&m,1);
  assert(m.song>=1 && m.song<=20 && m.bgm==9); tick(&m,DECIDE_DAY+FADE);
  assert(m.screen==MENU_PRE && m.bgm==9);
  enter_story(&m); m.zones=1; m.sel=21; m.bgm=9;
  m.tap=1; m.tap_x=m.tap_y=50; tick(&m,1); assert(m.song==-1 && m.bgm==9);
  tick(&m,DECIDE_DAY+FADE); assert(m.screen==MENU_PRE && m.song==-1 && m.bgm==9);
  puts("Android story BGM state tests passed");
  return 0;
}
