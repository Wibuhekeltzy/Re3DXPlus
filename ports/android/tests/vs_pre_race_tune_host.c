/* Exercises real menu transitions using the existing host stubs. */
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
  m->tune_full[0][0] = 1; m->tune_power[0][0] = 24; m->zones = 2;
}
static void click(menu_state *m, float x, float y) {
  menu_touch(m, (void *)1, TOUCH_BEGAN, x, y); menu_touch(m, (void *)1, TOUCH_ENDED, x, y); tick(m, 1);
}
int main(void) {
  menu_state m;
  for (int kind = 0; kind < MENU_VS_KINDS; ++kind) {
    full(&m, MENU_GAME_VS); m.screen = MENU_VS_SEL; m.sel = kind; m.deciding = 1; tick(&m, 1);
    assert(m.screen == MENU_SETTING && m.pre_tune_setting && m.vs.kind == kind && m.vs.state == MENU_VS_OFF);
    m.key = 1; tick(&m, 1); assert(m.pre_tune.power == 23);
    assert(m.tune_power[0][0] == 24 && !m.garage_changed);
    click(&m, 512, 320); tick(&m, FADE);
    assert(m.screen == MENU_VS && m.vs.kind == kind && m.pre_tune.state == 2);
    int p = 24, h = 10;
    assert(pre_race_tune_vs_apply(&m.pre_tune, 0, 0, &p, &h) && p == 23 && h == 11);
    p = 24; h = 10; assert(pre_race_tune_vs_apply(&m.pre_tune, 0, 0, &p, &h));
    p = 24; h = 10; assert(!pre_race_tune_vs_apply(&m.pre_tune, 0, 1, &p, &h));
    p = 23; h = 11; assert(!pre_race_tune_vs_apply(&m.pre_tune, 0, 0, &p, &h));
    full(&m, MENU_GAME_VS); m.screen = MENU_VS_SEL; m.sel = kind; m.deciding = 1; tick(&m, 1);
    m.key = 1; tick(&m, 1); m.back = 1; tick(&m, 1);
    assert(m.screen == MENU_VS_SEL && m.sel == kind && m.pre_tune.state == 0);
    assert(m.vs.state == MENU_VS_OFF && m.tune_power[0][0] == 24 && !m.garage_changed);
  }
  full(&m, MENU_GAME_VS); m.tune_full[0][0] = 0; m.st_power[0][0] = 3; m.st_handling[0][0] = 1;
  m.screen = MENU_VS_SEL; m.deciding = 1; tick(&m, 1); assert(m.screen == MENU_SETTING);
  m.key = 1; tick(&m, 1); click(&m, 512, 320); tick(&m, FADE);
  int p = 3, h = 1; assert(pre_race_tune_vs_apply(&m.pre_tune, 0, 0, &p, &h) && p == 2 && h == 2);
  puts("versus pre-race transitions passed"); return 0;
}
