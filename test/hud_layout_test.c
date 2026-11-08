/* The KEY LAYOUT's HUD page (hud.h HUD_RACE_*: the race's map, rank, time column and meters,
 * their size and place in menu.h hud_*) and the steering stick's dead zone and centre (hud.h
 * hud_set_stick_*): the consoles' boxes (set, hit, max_scale, clamp), the stick's reach, dead
 * zone and floating or fixed centre, the wheel's letting go, and the editor's second page with
 * its panel keys (page, < >, - +, RESET GROUP and RESET KEYS / HUD, the stick's own keys, the
 * back key). menu.c is included whole for its static editor (layout_touch, layout_step), hud.c
 * is linked; the GL and sound calls are left unresolved, as none is reached. The headers the
 * data tools write (tools/gen_from_original.sh) must be in src/ first; the two -D give the
 * declaration-only stub (test/glstub.h) the blend factors hud.c asks for.
 *
 *   gcc -std=gnu99 -DRC_FPT=double -DGL_ZERO=0 -DGL_ONE_MINUS_DST_COLOR=0x0307 -Isrc -Itest \
 *       test/hud_layout_test.c src/hud.c -lm -Wl,--unresolved-symbols=ignore-all \
 *       -o hud_layout_test && ./hud_layout_test
 */
#include "../src/menu.c"

void snd_menu(int which) { (void)which; }

static int fails;
#define CHECK(c) \
  do { \
    if (!(c)) { \
      printf("FAIL line %d: %s\n", __LINE__, #c); \
      ++fails; \
    } \
  } while (0)
#define NEAR(a, b, e) CHECK(fabsf((float)(a) - (float)(b)) <= (e))

/* ---- the consoles' boxes ---- */

typedef struct { int n, x0, y0, x1, y1; } box;

/* where a console answers hud_race_layout_hit, asked over the screen and a margin round it, a
 * point in every step px (the clamp's pushes are checked in 4s, the rest in 2s) */
static box scan_step(int g, int step) {
  box b = { 0, 9999, 9999, -9999, -9999 };
  for (int y = -40; y < 820; y += step)
    for (int x = -80; x < 1100; x += step)
      if (hud_race_layout_hit((float)x, (float)y) == g) {
        ++b.n;
        if (x < b.x0) b.x0 = x;
        if (y < b.y0) b.y0 = y;
        if (x > b.x1) b.x1 = x;
        if (y > b.y1) b.y1 = y;
      }
  return b;
}

static box scan(int g) { return scan_step(g, 2); }

static void race_defaults(void) {
  for (int g = 0; g < HUD_RACE_GROUPS; ++g) hud_race_layout_set(g, 1, 0, 0);
}

static void test_race_layout(void) {
  race_defaults();
  box d[HUD_RACE_GROUPS];
  for (int g = 0; g < HUD_RACE_GROUPS; ++g) {
    d[g] = scan(g);
    CHECK(d[g].n > 100); /* every console has a box */
    /* on the screen as it comes, but for the time column's margin hanging over the left edge */
    CHECK(d[g].x0 >= -32 && d[g].y0 >= 0 && d[g].x1 <= 1025 && d[g].y1 < 768);
    CHECK(hud_race_layout_max_scale(g) >= HUD_KEY_K_MIN && hud_race_layout_max_scale(g) <= HUD_KEY_K_MAX);
  }
  CHECK(hud_race_layout_max_scale(-1) == 1.0f && hud_race_layout_max_scale(HUD_RACE_GROUPS) == 1.0f);
  CHECK(hud_race_layout_hit(-300, -300) == -1);

  /* moved: its box moves with it, the others stay */
  hud_race_layout_set(HUD_RACE_RANK, 1, 100, 50);
  box r = scan(HUD_RACE_RANK);
  CHECK(r.x0 == d[HUD_RACE_RANK].x0 + 100 && r.y0 == d[HUD_RACE_RANK].y0 + 50);
  CHECK(r.x1 == d[HUD_RACE_RANK].x1 + 100 && r.y1 == d[HUD_RACE_RANK].y1 + 50);
  box same = scan(HUD_RACE_MAP);
  CHECK(!memcmp(&same, &d[HUD_RACE_MAP], sizeof same));
  same = scan(HUD_RACE_METER);
  CHECK(!memcmp(&same, &d[HUD_RACE_METER], sizeof same));

  /* sized about its own corner: that stays, the width and height grow */
  hud_race_layout_set(HUD_RACE_RANK, 1.5f, 0, 0);
  r = scan(HUD_RACE_RANK);
  CHECK(r.x0 == d[HUD_RACE_RANK].x0 && r.y0 == d[HUD_RACE_RANK].y0);
  NEAR(r.x1 - r.x0, (d[HUD_RACE_RANK].x1 - d[HUD_RACE_RANK].x0) * 1.5f, 4);
  NEAR(r.y1 - r.y0, (d[HUD_RACE_RANK].y1 - d[HUD_RACE_RANK].y0) * 1.5f, 4);

  /* a size past the limits is held to them */
  hud_race_layout_set(HUD_RACE_RANK, 5.0f, 0, 0);
  r = scan(HUD_RACE_RANK);
  NEAR(r.x1 - r.x0, (d[HUD_RACE_RANK].x1 - d[HUD_RACE_RANK].x0) * HUD_KEY_K_MAX, 4);
  hud_race_layout_set(HUD_RACE_RANK, 0.1f, 0, 0);
  r = scan(HUD_RACE_RANK);
  NEAR(r.x1 - r.x0, (d[HUD_RACE_RANK].x1 - d[HUD_RACE_RANK].x0) * HUD_KEY_K_MIN, 4);

  /* a bad value or a bad console changes nothing */
  hud_race_layout_set(HUD_RACE_RANK, 1.5f, 20, 10);
  box before = scan(HUD_RACE_RANK);
  hud_race_layout_set(HUD_RACE_RANK, NAN, 0, 0);
  hud_race_layout_set(HUD_RACE_RANK, 1, INFINITY, 0);
  hud_race_layout_set(HUD_RACE_RANK, 1, 0, NAN);
  hud_race_layout_set(-1, 2, 0, 0);
  hud_race_layout_set(HUD_RACE_GROUPS, 2, 0, 0);
  box after = scan(HUD_RACE_RANK);
  CHECK(!memcmp(&before, &after, sizeof before));

  /* left at 1 and 0 the boxes are what they were */
  race_defaults();
  for (int g = 0; g < HUD_RACE_GROUPS; ++g) {
    box a = scan(g);
    CHECK(!memcmp(&a, &d[g], sizeof a));
  }

  /* the clamp keeps a console on the screen, at any size and from any push (the others parked far
   * off so nothing covers it): all of it but the margin it comes with, which grows with it */
  for (int g = 0; g < HUD_RACE_GROUPS; ++g) {
    float over_l = d[g].x0 < 0 ? (float)-d[g].x0 : 0, over_r = d[g].x1 > 1024 ? (float)(d[g].x1 - 1024) : 0;
    for (int size = 0; size < 3; ++size) {
      float k = size == 0 ? HUD_KEY_K_MIN : size == 1 ? 1.0f : hud_race_layout_max_scale(g);
      for (int dir = 0; dir < 4; ++dir) {
        float dx = dir & 1 ? -3000.0f : 3000.0f, dy = dir & 2 ? -3000.0f : 3000.0f;
        hud_race_layout_clamp(g, k, &dx, &dy);
        for (int i = 0; i < HUD_RACE_GROUPS; ++i) hud_race_layout_set(i, 1, i == g ? dx : 5000, i == g ? dy : 5000);
        hud_race_layout_set(g, k, dx, dy);
        box b = scan_step(g, 4);
        CHECK(b.n > 0 && b.y0 >= -1 && b.y1 <= 768);
        CHECK(b.x0 >= -1 - over_l * k - 1 && b.x1 <= 1024 + over_r * k + 1);
        /* and it is pushed as far as it will go: on the screen's edge, not short of it */
        if (dir & 1) CHECK(b.x0 <= 4 - over_l * k + 1);
        else CHECK(b.x1 >= 1020 + over_r * k - 1);
        if (dir & 2) CHECK(b.y0 <= 5);
        else CHECK(b.y1 >= 763);
      }
    }
  }
  race_defaults();

  /* a console as it comes is never moved by the clamp: not by a nudge down, nor a size kept */
  for (int g = 0; g < HUD_RACE_GROUPS; ++g) {
    float nx = 0, ny = 0;
    hud_race_layout_clamp(g, 1.0f, &nx, &ny);
    CHECK(fabsf(nx) < 0.5f && fabsf(ny) < 0.5f);
    nx = 0, ny = 12;
    hud_race_layout_clamp(g, 1.0f, &nx, &ny);
    CHECK(fabsf(nx) < 0.5f && ny == 12);
  }

  /* a place already on the screen is left alone, a bad one is not touched */
  float dx = 10, dy = 10;
  hud_race_layout_clamp(HUD_RACE_RANK, 1.0f, &dx, &dy);
  CHECK(dx == 10 && dy == 10);
  dx = NAN, dy = 7;
  hud_race_layout_clamp(HUD_RACE_RANK, 1.0f, &dx, &dy);
  CHECK(isnan(dx) && dy == 7);
}

/* ---- the stick and the wheel ---- */

static hud_state H;
static int ids[4];

static void steer_setup(int mode, int centre, int dead) {
  hud_init(&H, 1024, 768);
  for (int g = 0; g < HUD_KEY_GROUPS; ++g) hud_layout_set(g, 1, 0, 0);
  hud_shift_layout_set(0, 1, 0, 0);
  hud_set_steer_mode(&H, mode);
  hud_set_stick_center_mode(&H, centre);
  hud_set_stick_deadzone_percent(dead);
}

/* the steer for a knob dx px from its centre, 90 px being full lock, past a dead zone of dead */
static float stick_steer(float dx, float dead) {
  float v = fabsf(dx) / 90.0f;
  v = v > 1.0f ? 1.0f : v;
  float a = v <= dead ? 0.0f : (v - dead) / (1.0f - dead);
  return dx < 0 ? -a : a;
}

static void test_stick(void) {
  /* floating: the stick is where the thumb goes down */
  steer_setup(HUD_STEER_STICK, HUD_STICK_FLOAT, 10);
  CHECK(H.steer_mode == HUD_STEER_STICK && H.gyro == 0 && H.stick_center_mode == HUD_STICK_FLOAT);
  hud_touch(&H, &ids[0], TOUCH_BEGAN, 600, 300); /* outside its zone */
  CHECK(H.steer_touch == NULL);
  hud_touch(&H, &ids[0], TOUCH_BEGAN, 200, 600);
  CHECK(H.steer_touch == &ids[0] && H.steer_v == 0);
  NEAR(H.stick_x0, 200, 1e-3f);
  NEAR(H.stick_y0, 600, 1e-3f);
  hud_touch(&H, &ids[1], TOUCH_BEGAN, 220, 560); /* a second finger does not take it over */
  CHECK(H.steer_touch == &ids[0]);
  hud_touch(&H, &ids[0], TOUCH_MOVED, 208, 600); /* 8 px: inside the 10% dead zone */
  CHECK(H.steer_v == 0);
  static const float moves[] = { 10, 25, 45, 80, 90, 200, -10, -45, -90, -200, 0 };
  for (unsigned i = 0; i < sizeof moves / sizeof *moves; ++i) {
    hud_touch(&H, &ids[0], TOUCH_MOVED, 200 + moves[i], 600);
    NEAR(H.steer_v, stick_steer(moves[i], 0.1f), 1e-4f);
  }
  hud_touch(&H, &ids[0], TOUCH_MOVED, 380, 700); /* far off to a corner: the knob held to its reach */
  NEAR(hypotf(H.stick_x - H.stick_x0, H.stick_y - H.stick_y0), 90, 1e-2f);
  NEAR(H.steer_v, stick_steer(180.0f * 90.0f / hypotf(180, 100), 0.1f), 1e-4f); /* (steering by its sideways part) */
  hud_touch(&H, &ids[0], TOUCH_MOVED, 200, 690); /* straight up or down steers nothing */
  CHECK(H.steer_v == 0);
  hud_touch(&H, &ids[0], TOUCH_MOVED, 245, 600);
  car_input in;
  memset(&in, 0, sizeof in);
  hud_input_dt(&H, &in, 1.0f / 60.0f);
  CHECK(in.analog == 1);
  NEAR(in.steer, stick_steer(45, 0.1f), 1e-4f);
  hud_touch(&H, &ids[0], TOUCH_ENDED, 245, 600); /* let go: the stick goes with it */
  CHECK(H.steer_touch == NULL && H.steer_v == 0);
  hud_input_dt(&H, &in, 1.0f / 60.0f);
  CHECK(in.steer == 0);

  /* fixed: only near the base, steering from the first touch */
  steer_setup(HUD_STEER_STICK, HUD_STICK_FIXED, 10);
  hud_touch(&H, &ids[0], TOUCH_BEGAN, 400, 450); /* in the zone, far from the base */
  CHECK(H.steer_touch == NULL);
  hud_touch(&H, &ids[0], TOUCH_BEGAN, 141 + 115, 640); /* a little beyond the ring */
  CHECK(H.steer_touch == NULL);
  hud_touch(&H, &ids[0], TOUCH_BEGAN, 141 + 60, 640);
  CHECK(H.steer_touch == &ids[0]);
  NEAR(H.stick_x0, 141, 1e-3f);
  NEAR(H.stick_y0, 640, 1e-3f);
  NEAR(H.steer_v, stick_steer(60, 0.1f), 1e-4f);
  hud_touch(&H, &ids[0], TOUCH_MOVED, 141 - 90, 640);
  NEAR(H.steer_v, -1.0f, 1e-4f);
  hud_touch(&H, &ids[0], TOUCH_ENDED, 141 - 90, 640);
  CHECK(H.steer_touch == NULL && H.steer_v == 0);
  hud_touch(&H, &ids[0], TOUCH_BEGAN, 141 + 105, 640); /* the ring and a little more takes it, held to the reach */
  CHECK(H.steer_touch == &ids[0]);
  NEAR(H.steer_v, 1.0f, 1e-4f);

  /* the centre changed under a finger lets it go; the same one again does not */
  hud_set_stick_center_mode(&H, HUD_STICK_FIXED);
  CHECK(H.steer_touch == &ids[0]);
  hud_set_stick_center_mode(&H, HUD_STICK_FLOAT);
  CHECK(H.steer_touch == NULL && H.steer_v == 0);
  hud_set_stick_center_mode(&H, 7); /* not a mode: fixed only for HUD_STICK_FIXED */
  CHECK(H.stick_center_mode == HUD_STICK_FLOAT);

  /* the steering changed under a finger lets it go, a bad one is the keys */
  steer_setup(HUD_STEER_STICK, HUD_STICK_FLOAT, 10);
  hud_touch(&H, &ids[0], TOUCH_BEGAN, 200, 600);
  hud_touch(&H, &ids[0], TOUCH_MOVED, 250, 600);
  CHECK(H.steer_touch == &ids[0] && H.steer_v > 0);
  hud_set_steer_mode(&H, HUD_STEER_STICK); /* unchanged: kept */
  CHECK(H.steer_touch == &ids[0]);
  hud_set_steer_mode(&H, HUD_STEER_KEYS);
  CHECK(H.steer_touch == NULL && H.steer_v == 0 && H.steer_mode == HUD_STEER_KEYS && H.gyro == 0);
  hud_set_steer_mode(&H, HUD_STEER_GYRO);
  CHECK(H.gyro == 1 && H.steer_mode == HUD_STEER_GYRO);
  hud_set_steer_mode(&H, 99);
  CHECK(H.steer_mode == HUD_STEER_KEYS && H.gyro == 0);
  hud_set_steer_mode(&H, -1);
  CHECK(H.steer_mode == HUD_STEER_KEYS && H.gyro == 0);
  hud_input_dt(&H, &in, 1.0f / 60.0f);
  CHECK(in.analog == 0);

  /* the dead zone's limits */
  steer_setup(HUD_STEER_STICK, HUD_STICK_FLOAT, -5); /* none at all */
  hud_touch(&H, &ids[0], TOUCH_BEGAN, 200, 600);
  hud_touch(&H, &ids[0], TOUCH_MOVED, 208, 600);
  NEAR(H.steer_v, stick_steer(8, 0.0f), 1e-4f);
  hud_touch(&H, &ids[0], TOUCH_ENDED, 208, 600);
  hud_set_stick_deadzone_percent(99); /* held to half the reach */
  hud_touch(&H, &ids[0], TOUCH_BEGAN, 200, 600);
  hud_touch(&H, &ids[0], TOUCH_MOVED, 240, 600);
  CHECK(H.steer_v == 0);
  hud_touch(&H, &ids[0], TOUCH_MOVED, 280, 600);
  NEAR(H.steer_v, stick_steer(80, 0.5f), 1e-4f);
  hud_touch(&H, &ids[0], TOUCH_ENDED, 280, 600);

  /* the wheel: the turn stays after a lift and runs back with the time, a cancelled touch is straight at once */
  steer_setup(HUD_STEER_WHEEL, HUD_STICK_FLOAT, 8);
  hud_touch(&H, &ids[0], TOUCH_BEGAN, 141 + 100, 640);
  CHECK(H.steer_touch == &ids[0]);
  hud_touch(&H, &ids[0], TOUCH_MOVED, 141, 640 + 100); /* a quarter turn about the centre */
  CHECK(H.steer_v > 0.999f);
  hud_touch(&H, &ids[0], TOUCH_ENDED, 141, 740);
  CHECK(H.steer_touch == NULL && H.steer_v > 0.999f);
  float turn = H.wheel_turn;
  hud_input_dt(&H, &in, NAN); /* a bad time: no progress */
  CHECK(H.wheel_turn == turn);
  hud_input_dt(&H, &in, -1.0f);
  CHECK(H.wheel_turn == turn);
  hud_input_dt(&H, &in, 5.0f); /* a stalled frame: a tenth of a second's worth at most */
  NEAR(H.wheel_turn, turn - 1.5708f * 0.3f, 1e-3f);
  NEAR(in.steer, H.steer_v, 1e-6f);
  for (int i = 0; i < 30; ++i) hud_input_dt(&H, &in, 1.0f / 60.0f);
  CHECK(H.wheel_turn == 0 && H.steer_v == 0);
  hud_touch(&H, &ids[0], TOUCH_BEGAN, 141 + 100, 640);
  hud_touch(&H, &ids[0], TOUCH_MOVED, 141, 640 + 100);
  hud_touch(&H, &ids[0], TOUCH_CANCELLED, 141, 740);
  CHECK(H.steer_touch == NULL && H.steer_v == 0 && H.wheel_turn == 0);

  /* the left keys' group is the stick's and the wheel's place in the editor too */
  CHECK(hud_layout_hit_mode(HUD_STEER_STICK, 141, 640) == HUD_KEYS_LEFT);
  CHECK(hud_layout_hit_mode(HUD_STEER_WHEEL, 141, 640) == HUD_KEYS_LEFT);
}

/* ---- the editor's second page ---- */

static menu_state M;
static int tid;

static void fresh(menu_state *m) {
  memset(m, 0, sizeof *m);
  for (int g = 0; g < 3; ++g) m->key_k[g] = 100;
  for (int g = 0; g < HUD_RACE_GROUPS; ++g) m->hud_k[g] = 100;
  m->key_shift_k = 100;
  m->stick_dead = LAY_STICK_DEAD;
  layout_enter(m);
}

/* a panel key tapped by a finger down and up on it */
static void press(menu_state *m, int b) {
  const float *r = LAY_BUTTON[b];
  float x = r[0] + r[2] / 2, y = r[1] + r[3] / 2;
  layout_touch(m, &tid, TOUCH_BEGAN, x, y);
  layout_touch(m, &tid, TOUCH_ENDED, x, y);
  layout_step(m, 0);
}

static void drag(menu_state *m, float x0, float y0, float x1, float y1) {
  layout_touch(m, &tid, TOUCH_BEGAN, x0, y0);
  layout_touch(m, &tid, TOUCH_MOVED, (x0 + x1) / 2, (y0 + y1) / 2);
  layout_touch(m, &tid, TOUCH_ENDED, x1, y1);
}

static void test_editor(void) {
  menu_state *m = &M;
  fresh(m);
  CHECK(m->screen == MENU_LAYOUT && m->lay_page == 0 && m->lay_sel == HUD_KEYS_RIGHT);

  /* the page key: the HUD page starts on the map; back, on the right keys */
  press(m, LAY_PAGE);
  CHECK(m->lay_page == 1 && m->lay_sel == HUD_RACE_MAP);
  for (int i = 1; i <= HUD_RACE_GROUPS; ++i) {
    press(m, LAY_NEXT);
    CHECK(m->lay_sel == i % HUD_RACE_GROUPS);
  }
  press(m, LAY_PREV);
  CHECK(m->lay_sel == HUD_RACE_GROUPS - 1);
  press(m, LAY_PAGE);
  CHECK(m->lay_page == 0 && m->lay_sel == HUD_KEYS_RIGHT);
  press(m, LAY_NEXT);
  CHECK(m->lay_sel == HUD_EDIT_SHIFT);
  press(m, LAY_NEXT);
  CHECK(m->lay_sel == HUD_KEYS_TOP);
  press(m, LAY_PREV);
  CHECK(m->lay_sel == HUD_EDIT_SHIFT);

  /* a console is taken where it is and dragged; the old place is left */
  fresh(m);
  press(m, LAY_PAGE);
  drag(m, 100, 60, 130, 90); /* in the rank */
  CHECK(m->lay_sel == HUD_RACE_RANK && m->lay_drag == 0 && m->lay_touch == NULL);
  CHECK(m->hud_dx[HUD_RACE_RANK] == 30 && m->hud_dy[HUD_RACE_RANK] == 30);
  CHECK(m->hud_k[HUD_RACE_RANK] == 100 && m->hud_dx[HUD_RACE_MAP] == 0 && m->hud_dx[HUD_RACE_METER] == 0);
  CHECK(m->garage_changed == 1);
  CHECK(hud_race_layout_hit(130, 90) == HUD_RACE_RANK);
  CHECK(hud_race_layout_hit(10, 40) == -1);

  /* a finger the system takes puts it back */
  m->garage_changed = 0;
  layout_touch(m, &tid, TOUCH_BEGAN, 130, 90);
  layout_touch(m, &tid, TOUCH_MOVED, 200, 100);
  CHECK(m->hud_dx[HUD_RACE_RANK] == 100);
  layout_touch(m, &tid, TOUCH_CANCELLED, 0, 0);
  CHECK(m->hud_dx[HUD_RACE_RANK] == 30 && m->hud_dy[HUD_RACE_RANK] == 30);
  CHECK(m->lay_drag == 0 && m->lay_touch == NULL && m->garage_changed == 0);
  CHECK(hud_race_layout_hit(130, 90) == HUD_RACE_RANK);

  /* a tap alone moves nothing, and does not count as a change */
  drag(m, 130, 90, 130, 90);
  CHECK(m->hud_dx[HUD_RACE_RANK] == 30 && m->hud_dy[HUD_RACE_RANK] == 30 && m->garage_changed == 0);

  /* dragged off every edge, a console stays on the screen (but for the margin it comes with, grown with it) */
  for (int g = 0; g < HUD_RACE_GROUPS; ++g) {
    race_defaults();
    box d = scan(g);
    float over_l = d.x0 < 0 ? (float)-d.x0 : 0, over_r = d.x1 > 1024 ? (float)(d.x1 - 1024) : 0;
    fresh(m);
    press(m, LAY_PAGE);
    m->lay_sel = g;
    press(m, LAY_BIGGER);
    static const float far[4][2] = { { 5000, 5000 }, { -5000, -5000 }, { 5000, -5000 }, { -5000, 5000 } };
    for (int i = 0; i < 4; ++i) {
      /* (taken by the panel's bar, which holds the selected group wherever it is) */
      const float *hd = LAY_HANDLE;
      float px = hd[0] + hd[2] / 2, py = hd[1] + hd[3] / 2;
      drag(m, px, py, px + far[i][0], py + far[i][1]);
      for (int j = 0; j < HUD_RACE_GROUPS; ++j) hud_race_layout_set(j, 1, j == g ? (float)m->hud_dx[g] : 5000, j == g ? (float)m->hud_dy[g] : 5000);
      hud_race_layout_set(g, (float)m->hud_k[g] * 0.01f, (float)m->hud_dx[g], (float)m->hud_dy[g]);
      box b = scan_step(g, 4);
      float k = (float)m->hud_k[g] * 0.01f;
      CHECK(b.n > 0 && b.y0 >= -1 && b.y1 <= 768);
      CHECK(b.x0 >= -2 - over_l * k && b.x1 <= 1025 + over_r * k);
    }
  }

  /* the size keys, to the limits (a console only as big as the screen takes it) */
  fresh(m);
  press(m, LAY_PAGE);
  for (int g = 0; g < HUD_RACE_GROUPS; ++g) {
    m->lay_sel = g;
    for (int i = 0; i < 20; ++i) press(m, LAY_BIGGER);
    CHECK(m->hud_k[g] <= (int)(hud_race_layout_max_scale(g) * 100 + 0.5f) && m->hud_k[g] > 100);
    for (int i = 0; i < 20; ++i) press(m, LAY_SMALLER);
    CHECK(m->hud_k[g] == (int)(HUD_KEY_K_MIN * 100 + 0.5f));
  }

  /* RESET GROUP puts back the one console, RESET HUD the four and nothing else */
  fresh(m);
  m->key_k[HUD_KEYS_LEFT] = 120, m->key_dx[HUD_KEYS_LEFT] = 15;
  m->stick_dead = 20, m->stick_fixed = 1;
  press(m, LAY_PAGE);
  for (int g = 0; g < HUD_RACE_GROUPS; ++g) m->hud_k[g] = 130, m->hud_dx[g] = 5 + g, m->hud_dy[g] = -3 - g;
  m->lay_sel = HUD_RACE_TIME;
  press(m, LAY_RESET);
  CHECK(m->hud_k[HUD_RACE_TIME] == 100 && m->hud_dx[HUD_RACE_TIME] == 0 && m->hud_dy[HUD_RACE_TIME] == 0);
  CHECK(m->hud_k[HUD_RACE_MAP] == 130 && m->hud_dx[HUD_RACE_RANK] == 6 && m->hud_dy[HUD_RACE_METER] == -6);
  CHECK(m->key_k[HUD_KEYS_LEFT] == 120 && m->stick_dead == 20 && m->stick_fixed == 1);
  press(m, LAY_RESET_ALL);
  for (int g = 0; g < HUD_RACE_GROUPS; ++g) CHECK(m->hud_k[g] == 100 && m->hud_dx[g] == 0 && m->hud_dy[g] == 0);
  CHECK(m->key_k[HUD_KEYS_LEFT] == 120 && m->key_dx[HUD_KEYS_LEFT] == 15); /* the keys page untouched */
  CHECK(m->stick_dead == 20 && m->stick_fixed == 1);

  /* and RESET KEYS the keys and the stick, not the HUD page */
  for (int g = 0; g < HUD_RACE_GROUPS; ++g) m->hud_k[g] = 130;
  press(m, LAY_PAGE);
  press(m, LAY_RESET_ALL);
  CHECK(m->key_k[HUD_KEYS_LEFT] == 100 && m->key_dx[HUD_KEYS_LEFT] == 0);
  CHECK(m->stick_dead == LAY_STICK_DEAD && m->stick_fixed == 0);
  for (int g = 0; g < HUD_RACE_GROUPS; ++g) CHECK(m->hud_k[g] == 130);

  /* the stick's keys work only on the left keys' group with the stick for the steering: off, they do not
   * even take a finger down */
  fresh(m);
  m->gyro = HUD_STEER_KEYS, m->lay_sel = HUD_KEYS_LEFT;
  for (int b = LAY_DEAD_MINUS; b <= LAY_DEAD_PLUS; ++b) {
    const float *r = LAY_BUTTON[b];
    layout_touch(m, &tid, TOUCH_BEGAN, r[0] + r[2] / 2, r[1] + r[3] / 2);
    CHECK(m->lay_touch == NULL && m->lay_button_down == 0);
    layout_touch(m, &tid, TOUCH_ENDED, r[0] + r[2] / 2, r[1] + r[3] / 2);
  }
  {
    const float *r = LAY_BUTTON[LAY_CENTER];
    layout_touch(m, &tid, TOUCH_BEGAN, r[0] + r[2] / 2, r[1] + r[3] / 2);
    CHECK(m->lay_touch == NULL && m->lay_button_down == 0);
    layout_touch(m, &tid, TOUCH_ENDED, r[0] + r[2] / 2, r[1] + r[3] / 2);
  }
  press(m, LAY_CENTER);
  press(m, LAY_DEAD_PLUS);
  CHECK(m->lay_touch == NULL && m->stick_fixed == 0 && m->stick_dead == LAY_STICK_DEAD);
  m->gyro = HUD_STEER_STICK, m->lay_sel = HUD_KEYS_RIGHT;
  press(m, LAY_CENTER);
  press(m, LAY_DEAD_MINUS);
  CHECK(m->stick_fixed == 0 && m->stick_dead == LAY_STICK_DEAD);
  m->lay_sel = HUD_KEYS_LEFT;
  m->garage_changed = 0;
  press(m, LAY_CENTER);
  CHECK(m->stick_fixed == 1 && m->garage_changed == 1);
  press(m, LAY_CENTER);
  CHECK(m->stick_fixed == 0);
  press(m, LAY_DEAD_PLUS);
  CHECK(m->stick_dead == LAY_STICK_DEAD + 1);
  press(m, LAY_DEAD_MINUS);
  press(m, LAY_DEAD_MINUS);
  CHECK(m->stick_dead == LAY_STICK_DEAD - 1);
  for (int i = 0; i < 80; ++i) press(m, LAY_DEAD_PLUS);
  CHECK(m->stick_dead == 50);
  for (int i = 0; i < 80; ++i) press(m, LAY_DEAD_MINUS);
  CHECK(m->stick_dead == 0);
  press(m, LAY_PAGE); /* not on the HUD page, whose second console is the left keys' number */
  m->lay_sel = HUD_RACE_RANK;
  CHECK(m->lay_sel == HUD_KEYS_LEFT && m->lay_page == 1);
  m->stick_fixed = 0;
  press(m, LAY_CENTER);
  press(m, LAY_DEAD_PLUS);
  CHECK(m->stick_fixed == 0 && m->stick_dead == 0); /* (80 minuses left it at 0) */

  /* the panel's bar takes the selected group from under the panel; a touch on the panel itself is its own */
  fresh(m);
  press(m, LAY_PAGE);
  m->lay_sel = HUD_RACE_TIME; /* its box hangs over the screen's edge as it comes: sizing keeps it put */
  press(m, LAY_BIGGER);
  CHECK(m->hud_k[HUD_RACE_TIME] == 110 && m->hud_dx[HUD_RACE_TIME] == 0 && m->hud_dy[HUD_RACE_TIME] == 0);
  press(m, LAY_RESET);
  m->lay_sel = HUD_RACE_METER;
  const float *hd = LAY_HANDLE;
  drag(m, hd[0] + 5, hd[1] + 5, hd[0] - 95, hd[1] + 5);
  CHECK(m->lay_sel == HUD_RACE_METER && m->hud_dx[HUD_RACE_METER] == -100 && m->hud_dy[HUD_RACE_METER] == 0);
  layout_touch(m, &tid, TOUCH_BEGAN, 300, 480); /* on the panel, off its keys and bar */
  CHECK(m->lay_touch == NULL && m->lay_drag == 0);
  layout_touch(m, &tid, TOUCH_ENDED, 300, 480);

  /* the back key (or DONE) lets go of a finger on a group, put back, and returns to OPTION */
  fresh(m);
  press(m, LAY_PAGE);
  layout_touch(m, &tid, TOUCH_BEGAN, 100, 60);
  layout_touch(m, &tid, TOUCH_MOVED, 150, 90);
  CHECK(m->hud_dx[HUD_RACE_RANK] == 50);
  layout_step(m, 1);
  CHECK(m->screen == MENU_OPTION && m->lay_drag == 0 && m->lay_touch == NULL);
  CHECK(m->hud_dx[HUD_RACE_RANK] == 0 && m->hud_dy[HUD_RACE_RANK] == 0);
  fresh(m);
  press(m, LAY_DONE);
  CHECK(m->screen == MENU_OPTION);
}

int main(void) {
  test_race_layout();
  test_stick();
  test_editor();
  printf(fails ? "hud layout: %d FAILED\n" : "hud layout: ok\n", fails);
  return fails != 0;
}
