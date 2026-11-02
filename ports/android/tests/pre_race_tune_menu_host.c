/* Actual production menu_update, host-only stubs; no device or assets. */
#define main previous_story_test_main
#include "story_timezone_host.c"
#undef main

float anm_sprite_width(int sprite) { (void)sprite; return 60; }
int hud_menu_key(float x, float y) {
  if (x >= 22 && x < 226 && y >= 634 && y < 726) return -1;
  if (x >= 798 && x < 1002 && y >= 634 && y < 726) return 1;
  if (x >= 722 && x < 806 && y >= 46 && y < 106) return HUD_MENU_BACK;
  return 0;
}
static void full(menu_state *m, int game) {
  memset(m, 0, sizeof *m); m->game = game; m->from = -1;
  m->tune_full[0][0] = 1; m->tune_power[0][0] = 24;
  m->zones = 2; m->song = 3; m->sel = 3;
}
static void day_done(menu_state *m) {
  m->screen = MENU_BGM; m->deciding = 1; tick(m, 1);
}
static void input(menu_state *m, int a, float x, float y) {
  menu_touch(m, (void *)1, a, x, y);
}
static void click(menu_state *m, float x, float y) {
  input(m, TOUCH_BEGAN, x, y); input(m, TOUCH_ENDED, x, y); tick(m, 1);
}
int main(void) {
  menu_state m;
  assert(previous_story_test_main() == 0);
  full(&m, MENU_GAME_TA); m.zone = 1; m.bgm = 5; m.sel = 5; day_done(&m);
  assert(m.screen == MENU_SETTING && m.pre_tune_setting && m.ta_zone == 1 && m.song == 3);
  menu_scene scene; assert(menu_scene3d(&m, &scene) && scene.n == 1);
  assert(scene.cars[0].pack == chosen_car(&m, m.car)->colors[m.color].pack);
  m.key = 1; tick(&m, 1); assert(m.pre_tune.power == 23);
  assert(m.tune_power[0][0] == 24 && !m.garage_changed);
  click(&m, 512, 320); assert(m.pre_tune.state == 2);
  tick(&m, FADE); assert(m.screen == MENU_PRE && m.pre_tune.state == 2);
  int p = 24, h = 10;
  assert(pre_race_tune_take(&m.pre_tune, m.game, m.maker, m.car, p, h, &p, &h));
  assert(p == 23 && h == 11 && m.tune_power[0][0] == 24);
  assert(!pre_race_tune_take(&m.pre_tune, m.game, m.maker, m.car, 24, 10, &p, &h));

  full(&m, MENU_GAME_STORY); m.tune_full[0][0] = 0; m.st_power[0][0] = 20; m.st_handling[0][0] = 14;
  m.story_day_select = 1; m.story_zones_saved = 2; m.zone = 1; m.sel = 7; m.song = 7; day_done(&m);
  assert(m.screen == MENU_SETTING && !m.story_day_select && m.song == 7);
  click(&m, 100, 670); assert(m.pre_tune.power == 21);
  click(&m, 760, 70); assert(m.screen == MENU_BGM && m.story_day_select && m.sel == 7 && m.zone == 1);
  assert(m.pre_tune.state == 0 && m.st_power[0][0] == 20 && m.st_handling[0][0] == 14);

  full(&m, MENU_GAME_TEN); m.screen = MENU_TEN_LEVEL; m.deciding = 1; m.sel = 0; tick(&m, 1);
  assert(m.screen == MENU_SETTING); m.back = 1; tick(&m, 1);
  assert(m.screen == MENU_TEN_LEVEL && m.pre_tune.state == 0);

  full(&m, MENU_GAME_TA); m.tune_full[0][0] = 0; m.st_power[0][0] = 1; day_done(&m);
  assert(m.screen == MENU_PRE && m.pre_tune.state == 0);
  full(&m, MENU_GAME_STORY); m.tune_full[0][0] = 0; m.st_power[0][0] = 0; day_done(&m);
  assert(m.screen == MENU_PRE);
  full(&m, MENU_GAME_STORY); m.finish_story = 3; back_from_race(&m, 0);
  assert(m.screen == MENU_PRE && m.pre_quiet && m.pre_tune.state == 0);

  /* A captured car touch cannot confirm on press, cancellation, or drag-off. */
  full(&m, MENU_GAME_TA); day_done(&m);
  input(&m, TOUCH_BEGAN, 512, 320); tick(&m, 1);
  assert(m.pre_tune.state == 1 && !m.deciding);
  input(&m, TOUCH_CANCELLED, 512, 320); tick(&m, 1);
  assert(m.pre_tune.state == 1 && !m.deciding);
  input(&m, TOUCH_BEGAN, 512, 320); tick(&m, 1);
  input(&m, TOUCH_MOVED, 100, 320); input(&m, TOUCH_ENDED, 100, 320); tick(&m, 1);
  assert(m.pre_tune.state == 1 && !m.deciding);
  input(&m, TOUCH_BEGAN, 512, 320); tick(&m, 1);
  menu_touch(&m, (void *)2, TOUCH_BEGAN, 512, 320);
  menu_touch(&m, (void *)2, TOUCH_ENDED, 512, 320); tick(&m, 1);
  assert(m.pre_tune.state == 1 && !m.deciding);
  /* Other fingers may still operate the original keys. */
  menu_touch(&m, (void *)2, TOUCH_BEGAN, 900, 670); tick(&m, 1);
  menu_touch(&m, (void *)2, TOUCH_ENDED, 900, 670);
  assert(m.pre_tune.power == 23 && m.pre_tune.state == 1);
  input(&m, TOUCH_ENDED, 512, 320); tick(&m, 1);
  assert(m.pre_tune.state == 2 && m.deciding);
  /* Captures are retired on transition and cannot leak across screens. */
  full(&m, MENU_GAME_TA); day_done(&m); input(&m, TOUCH_BEGAN, 512, 320); tick(&m, 1);
  m.back = 1; tick(&m, 1); assert(m.screen == MENU_BGM && !m.pre_tune_car_touch);
  input(&m, TOUCH_ENDED, 512, 320); tick(&m, 1); assert(m.screen == MENU_BGM && !m.deciding);

  /* Original keys adjust the draft, and chip/blank taps cannot change persistent settings. */
  full(&m, MENU_GAME_TA); day_done(&m);
  click(&m, 900, 670); assert(m.pre_tune.power == 23);
  click(&m, 100, 670); assert(m.pre_tune.power == 24);
  click(&m, 500, 440); assert(m.pre_tune.state == 1);
  click(&m, 50, 170); click(&m, 910, 170);
  m.dress = 1; tick(&m, 1);
  assert(m.screen == MENU_SETTING && !m.dress_free && m.tune_full[0][0] == 1 && m.manual == 0 && !m.garage_changed);
  menu_open(&m); assert(m.pre_tune.state == 0 && m.tune_power[0][0] == 24);
  day_done(&m); assert(m.screen == MENU_SETTING && m.pre_tune.power == 24);
  click(&m, 512, 320); tick(&m, FADE); assert(m.pre_tune.state == 2);
  m.car = 1; p = 24; h = 10;
  assert(!pre_race_tune_take(&m.pre_tune, m.game, m.maker, m.car, p, h, &p, &h));
  assert(p == 24 && h == 10 && m.pre_tune.state == 0);

  /* After a paused race is abandoned, the next attempt starts from the card. */
  full(&m, MENU_GAME_TA); day_done(&m); click(&m, 900, 670);
  click(&m, 512, 320); tick(&m, FADE); back_from_race(&m, 1);
  assert(m.screen == MENU_BGM && m.pre_tune.state == 0);
  day_done(&m); assert(m.screen == MENU_SETTING && m.pre_tune.power == 24);

  /* BGM return preserves song choice, while Story never rewrites TA preference. */
  full(&m, MENU_GAME_STORY); m.story_day_select = 1; m.story_zones_saved = 2;
  m.zone = 1; m.ta_zone = 0; m.song = 7; m.sel = 7; m.bgm = 4;
  day_done(&m); m.back = 1; tick(&m, 1);
  assert(m.screen == MENU_BGM && m.story_day_select && m.sel == 7 && m.song == 7 && m.bgm == 4 && m.ta_zone == 0);
  /* Original setting retains persistent tuning; the second edit is race-only. */
  full(&m, MENU_GAME_TA); m.screen = MENU_SETTING; m.key = 1; tick(&m, 1);
  assert(m.tune_power[0][0] == 23 && m.garage_changed);
  m.garage_changed = 0; day_done(&m);
  assert(m.pre_tune.power == 23); m.key = 1; tick(&m, 1);
  assert(m.pre_tune.power == 22 && m.tune_power[0][0] == 23 && !m.garage_changed);
  click(&m, 512, 320); tick(&m, FADE);
  p = 23; h = 11; assert(pre_race_tune_take(&m.pre_tune, m.game, m.maker, m.car, p, h, &p, &h));
  assert(p == 22 && h == 12 && m.tune_power[0][0] == 23 && !m.garage_changed);
  day_done(&m); assert(m.pre_tune.power == 23); m.key = 1; tick(&m, 1);
  m.back = 1; tick(&m, 1); assert(m.screen == MENU_BGM && m.tune_power[0][0] == 23 && !m.garage_changed);
  m.screen = MENU_SETTING; m.pre_tune_setting = 0;
  m.tap = 1; m.tap_x = 30; m.tap_y = 110; tick(&m, 1);
  assert(m.tune_full[0][0] == 0 && m.garage_changed);
  m.garage_changed = 0; m.tap = 1; m.tap_x = 570; m.tap_y = 110; tick(&m, 1);
  assert(m.manual == 1 && m.garage_changed);
  /* Saturate the temporary balance without touching the card. */
  full(&m, MENU_GAME_TA); day_done(&m);
  for (int i = 0; i < 30; ++i) { m.key = 1; tick(&m, 1); }
  assert(m.pre_tune.power == 10 && m.tune_power[0][0] == 24 && !m.garage_changed);
  for (int i = 0; i < 30; ++i) { m.key = -1; tick(&m, 1); }
  assert(m.pre_tune.power == 24 && !m.garage_changed);
  full(&m, MENU_GAME_VS); m.screen = MENU_SETTING; m.key = 1; tick(&m, 1);
  assert(m.tune_power[0][0] == 23 && m.garage_changed);
  m.vs.day = 1; day_done(&m);
  assert(m.screen == MENU_PRE && !m.pre_tune_setting && m.tune_power[0][0] == 23);
  puts("pre-race menu integration tests passed");
  return 0;
}
