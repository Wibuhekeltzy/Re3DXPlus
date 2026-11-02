/* Production menu with invented defaults; no runtime assets or persistent card. */
#define main timezone_fixture_main
#include "story_timezone_host.c"
#undef main
int android_bgm_group_available(int group) { return group == 1 || group == 2; }
int hud_meter_available(void) { return 1; }
int app_card_choice_commit(menu_state *m, int row, int choice, long long born) {
  (void)m; (void)row; (void)choice; (void)born; return 0;
}
static void setup(menu_state *m, int group) {
  memset(m, 0, sizeof *m);
  m->game = MENU_GAME_STORY; m->screen = MENU_ST_RIVAL; m->from = -1;
  m->deciding = 1; m->zones = 1; m->bgm = 7; m->bgm_source = 0; m->ta_zone = 1;
  m->n_own = 1; m->own_sel = 0; m->maker = 0; m->car = 0;
  m->own[0].maker = 0; m->own[0].car = 0; m->own[0].born = 42;
  m->card_born[0][0] = 42; m->sound_car[0][0] = group;
}
static void pick(menu_state *m, int div, int level, int slot, int expected, int group) {
  m->screen = MENU_ST_RIVAL; m->from = -1; m->deciding = 1;
  m->st_div = div; m->st_level = level; m->sel = slot;
  tick(m, 1);
  assert(m->screen == MENU_BGM && m->story_day_select && m->story_pick == expected);
  assert(m->zone == 1 - expected % 2);
  assert(m->song == 1 + expected % 10 + group && m->sel == m->song);
  assert(m->bgm == 7 && m->bgm_source == 0 && m->ta_zone == 1);
}
/* Mirrors only app.c's next-frame zone clamp; this is not app.c integration. */
static void next_frame_availability(menu_state *m, int has_night) {
  m->zones = has_night ? 2 : 1;
  if (m->zone >= m->zones) m->zone = 0;
}
int main(void) {
  menu_state m;
  for (int group = 0; group < 3; group++) {
    setup(&m, group); pick(&m, 0, 0, 0, 0, group);
    assert(m.zones == 1 && m.zone == 1); /* old screen has no night */
    next_frame_availability(&m, 1); assert(m.zone == 1);
    setup(&m, group); pick(&m, 0, 0, 0, 0, group);
    next_frame_availability(&m, 0); assert(m.zone == 0);
    pick(&m, 0, 0, 1, 1, group); assert(m.zone == 0);
    pick(&m, 5, 0, 0, 100, group);
    pick(&m, 10, 1, 4, 110, group);
    pick(&m, 5, 2, 0, 10, group);
  }
  setup(&m, 0); pick(&m, 0, 0, 0, 0, 0);
  next_frame_availability(&m, 1);
  m.key = 1; tick(&m, 1); tick(&m, SLIDE); assert(m.zone == 0);
  m.tap = 1; m.tap_x = 200; m.tap_y = 350; tick(&m, 1);
  assert(m.sel == 2 && m.song == 1);
  m.tap = 1; m.tap_x = m.tap_y = 50; tick(&m, 1);
  assert(m.song == 2 && m.story_zone == 0 && m.bgm == 7 && m.ta_zone == 1);
  tick(&m, DECIDE_DAY + FADE); assert(m.screen == MENU_PRE && m.song == 2 && m.zone == 0);
  pick(&m, 0, 0, 2, 2, 0); assert(m.zone == 1 && m.song == 3);
  setup(&m, 0); m.sel = -1; tick(&m, 1);
  assert(m.story_pick == -1 && m.zone == 0 && m.song == 0);
  setup(&m, 0); m.sel = 111; tick(&m, 1);
  assert(m.story_pick == 111 && m.zone == 0 && m.song == 0);
  setup(&m, 0); m.sel = 1000; tick(&m, 1);
  assert(m.story_pick == 111 && m.zone == 0 && m.song == 0);
  puts("synthetic story defaults menu tests passed");
  return 0;
}
