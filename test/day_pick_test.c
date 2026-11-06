/* The time of day asked before a story race and a 10-in-a-row level (menu.h day_pick): the time
 * zone select's cards between the rival (level) select and the pre-race tune check, the story's
 * choice kept apart from the time attack's, the back keys. menu.c is included whole and driven
 * through menu_update (a screen's decide is its `deciding` counter running out); the animation
 * and sound calls it reaches are stubbed below, the rest of the GL and sound ones are left
 * unresolved, as none is reached. The headers the data tools write (tools/gen_from_original.sh)
 * must be in src/ first.
 *
 *   gcc -std=gnu99 -DRC_FPT=double -Isrc -Itest test/day_pick_test.c src/hud.c -lm \
 *       -Wl,--unresolved-symbols=ignore-all -o day_pick_test && ./day_pick_test
 */
#include "../src/menu.c"

void snd_menu(int which) { (void)which; }
int anm_sprite(int id) { return id; }

static menu_state M;

/* the card on show sits where a tap at (200, 200) hits it; its arrows and the other card are far off */
int anm_sprite_rect(int anim, int frame, int sprite, float r[4]) {
  (void)anim, (void)frame;
  float x = sprite == (M.zone ? T_NIGHT : T_MORNING) ? 100.0f : 800.0f;
  r[0] = r[1] = x;
  r[2] = r[3] = x + 200.0f;
  return 1;
}

static int fails;
#define CHECK(c) \
  do { \
    if (!(c)) { \
      printf("FAIL line %d: %s\n", __LINE__, #c); \
      ++fails; \
    } \
  } while (0)

static void frames(int n) {
  for (int i = 0; i < n; ++i) menu_update(&M, 1.0f / 60.0f);
}
static void back(void) {
  M.back = 1;
  frames(1);
}
static void tap(float x, float y) {
  M.tap = 1, M.tap_x = x, M.tap_y = y;
  frames(1);
}
/* a screen's decide, run out */
static void decide(void) {
  M.deciding = 1;
  frames(1);
}
/* the card chosen: its fade, then the next screen */
static void choose(void) {
  tap(200, 200);
  frames(DECIDE_DAY + FADE + 1);
}
/* the tune check's DONE, and its fade */
static void done(void) {
  tap(300, 420);
  frames(FADE + 1);
}
/* what the app does while the cards are up: zones by the race's city (does it have a night map) */
static void app_zones(int zones) {
  if (M.day_pick) {
    M.zones = zones;
    if (M.zone >= M.zones) M.zone = 0;
  }
}

static void fresh(int game, int screen) {
  memset(&M, 0, sizeof M);
  M.zones = 2;
  M.from = -1;
  M.music = 1;
  M.own_sel = 0; /* the car is one of the save's: the setting's back key would go to the garage */
  M.game = game;
  M.screen = screen;
}

/* a night level of the 10-in-a-row's course 0, and a morning one */
static int ten_level_of(int night) {
  for (int l = 0; l < 10; ++l)
    if (ten_night(0, l) == night) return l;
  return -1;
}

int main(void) {
  /* ---- a story race: the menus on night (the time attack's), the story last asked morning ---- */
  fresh(MENU_GAME_STORY, MENU_ST_RIVAL);
  M.st_level = 1;
  M.sel = 3;
  M.zone = 1, M.ta_zone = 1, M.story_zone = 0;
  decide();
  CHECK(M.screen == MENU_BGM && M.day_pick == MENU_ST_RIVAL && M.tune_confirm == 0);
  CHECK(M.story_n == 8 && M.story_pick == 8);
  CHECK(M.zone == 0 && M.day_zone_was == 1 && M.day_zones_was == 2); /* on the story's own choice */
  CHECK(M.song > 0);
  CHECK(menu_bgm(&M, 0) != NULL); /* the select music plays on through the card */

  /* a race in a city without a night map: the morning card alone (the app sets that while the card is up) */
  app_zones(1);
  CHECK(M.zones == 1 && M.zone == 0);
  /* the BGM line is not on this card: tapped or keyed, it does not step */
  int line = M.sel;
  tap(300, 360);
  M.key = 1;
  frames(1);
  CHECK(M.sel == line && M.deciding == 0 && M.slide == 0);

  /* the card chosen as it is: nothing new to keep; then the tune check, the zones as they were */
  tap(200, 200);
  CHECK(M.deciding > 0 && M.garage_changed == 0 && M.story_zone == 0);
  CHECK(menu_bgm(&M, 0) != NULL); /* (the fade is not silent: a time attack's BGM choice is) */
  frames(DECIDE_DAY + FADE + 1);
  CHECK(M.screen == MENU_SETTING && M.tune_confirm == MENU_ST_RIVAL && M.day_pick == 0);
  CHECK(M.zones == 2 && M.zone == 0);
  CHECK(menu_bgm(&M, 0) != NULL);

  /* the tune check's back key goes to the card (not to the garage), on the zone chosen, still
   * remembering the zone from before the card */
  back();
  CHECK(M.screen == MENU_BGM && M.day_pick == MENU_ST_RIVAL && M.tune_confirm == 0);
  CHECK(M.zone == 0 && M.day_zone_was == 1 && M.day_zones_was == 2);

  /* the card's back key goes to the rival select, on the episode chosen, the zone and zones as before */
  app_zones(1);
  back();
  CHECK(M.screen == MENU_ST_RIVAL && M.day_pick == 0);
  CHECK(M.sel == 3 && M.zones == 2 && M.zone == 1);

  /* ---- the same race again: the wheel turns the card over, the choice is kept, DONE starts the race ---- */
  decide();
  CHECK(M.screen == MENU_BGM && M.zone == 0 && M.day_zone_was == 1);
  M.key = 1;
  frames(1);
  CHECK(M.slide > 0);
  back(); /* (the back key waits for the card to come to rest) */
  CHECK(M.screen == MENU_BGM && M.day_pick == MENU_ST_RIVAL);
  frames(SLIDE + 1);
  CHECK(M.slide == 0 && M.zone == 1);
  choose();
  CHECK(M.story_zone == 1 && M.garage_changed == 1); /* kept for the next race, with the options */
  CHECK(M.screen == MENU_SETTING && M.tune_confirm == MENU_ST_RIVAL);
  done();
  CHECK(M.screen == MENU_PRE && M.tune_confirm == 0 && M.day_pick == 0);
  CHECK(M.zone == 1 && M.zones == 2 && M.ta_zone == 1);

  /* ---- the time attack after it: its own choice, not the story's ---- */
  M.ta_zone = 0;
  go(&M, MENU_COURSE);
  M.game = MENU_GAME_TA;
  M.sel = 2;
  decide();
  CHECK(M.screen == MENU_BGM && M.day_pick == 0);
  CHECK(M.zone == 0); /* (the story race's night, the menus' map, is not its choice) */
  line = M.sel;
  tap(300, 360);
  CHECK(M.sel == (line + 1) % bgm_lines(&M) && M.deciding == 0); /* (this card has the BGM line) */
  tap(200, 200);
  CHECK(M.deciding > 0);
  CHECK(menu_bgm(&M, 0) == NULL); /* (as before: the BGM chosen, the music stops) */
  frames(DECIDE_DAY + FADE + 1);
  CHECK(M.screen == MENU_SETTING && M.tune_confirm == MENU_BGM);
  CHECK(M.ta_zone == 0 && M.story_zone == 1);
  back(); /* its tune check's back key as before: the BGM select, whose back key goes on to the course select */
  CHECK(M.screen == MENU_BGM && M.day_pick == 0 && M.tune_confirm == 0);
  back();
  CHECK(M.screen == MENU_COURSE);

  /* the card in a time attack turned to night: that one is the time attack's */
  decide();
  M.zone = 1;
  choose();
  CHECK(M.ta_zone == 1 && M.story_zone == 1 && M.screen == MENU_SETTING);

  /* ---- a story race on a map without a night at all opens on morning whatever was kept ---- */
  fresh(MENU_GAME_STORY, MENU_ST_RIVAL);
  M.zones = 1;
  M.story_zone = 1;
  decide();
  CHECK(M.screen == MENU_BGM && M.zone == 0);

  /* ---- a 10-in-a-row level: it starts on its own time, the arcade's ---- */
  int night = ten_level_of(1), day = ten_level_of(0);
  CHECK(night >= 0 && day >= 0);
  for (int pass = 0; pass < 2; ++pass) {
    int level = pass ? night : day;
    if (level < 0) continue;
    fresh(MENU_GAME_TEN, MENU_TEN_LEVEL);
    M.ten_levels = 10;
    M.sel = level;
    M.zone = pass ? 0 : 1; /* (the other, from before) */
    decide();
    CHECK(M.screen == MENU_BGM && M.day_pick == MENU_TEN_LEVEL && M.ten_level == level);
    CHECK(M.zone == pass && M.day_zone_was == (pass ? 0 : 1));
    CHECK(M.tune_confirm == 0 && M.song > 0);
    /* turned to the other, chosen (not the story's), the tune check, and back twice */
    M.zone = !M.zone;
    choose();
    CHECK(M.story_zone == 0 && M.garage_changed == 0);
    CHECK(M.screen == MENU_SETTING && M.tune_confirm == MENU_TEN_LEVEL && M.zone == !pass);
    back();
    CHECK(M.screen == MENU_BGM && M.day_pick == MENU_TEN_LEVEL && M.zone == !pass);
    back();
    CHECK(M.screen == MENU_TEN_LEVEL && M.day_pick == 0 && M.sel == level && M.ten_face_lv == level);
    CHECK(M.zone == (pass ? 0 : 1) && M.zones == 2);
    /* and on to the race, on its own time */
    decide();
    choose();
    done();
    CHECK(M.screen == MENU_PRE && M.zone == pass);
  }

  printf(fails ? "day pick: %d FAILED\n" : "day pick: ok\n", fails);
  return fails != 0;
}
