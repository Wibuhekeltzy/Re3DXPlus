/* The KEY LAYOUT's shift keys (hud.h HUD_EDIT_SHIFT, menu.h key_shift_*): taken, dragged, sized,
 * split off and put back, and kept on the screen. menu.c is included whole for its static
 * editor (layout_touch, layout_step), hud.c is linked; the GL and sound calls are left
 * unresolved, as none is reached. The headers the data tools write (tools/gen_from_original.sh)
 * must be in src/ first.
 *
 *   gcc -std=gnu99 -DRC_FPT=double -Isrc -Itest test/shift_layout_test.c src/hud.c -lm \
 *       -Wl,--unresolved-symbols=ignore-all -o shift_layout_test && ./shift_layout_test
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

/* the shift pair's box on the screen (the layout's 706..802 x 610..754, anchored at the screen's
 * bottom right, 1024 x 768, no edge): k times its size and moved (dx, dy) */
static void shift_box(int k_percent, int dx, int dy, float *x0, float *y0, float *x1, float *y1) {
  float k = (float)k_percent * 0.01f;
  *x0 = 1024 + (float)dx + (706 - 1024) * k;
  *y0 = 768 + (float)dy + (610 - 768) * k;
  *x1 = *x0 + 96 * k;
  *y1 = *y0 + 144 * k;
}

static menu_state M;
static int tid;

static void fresh(menu_state *m) {
  memset(m, 0, sizeof *m);
  for (int g = 0; g < 3; ++g) m->key_k[g] = 100;
  m->key_shift_k = 100;
  layout_enter(m);
}

static void tap_button(menu_state *m, int b) {
  m->lay_tap = b + 1;
  layout_step(m, 0);
}

static void drag(menu_state *m, float x0, float y0, float x1, float y1) {
  layout_touch(m, &tid, TOUCH_BEGAN, x0, y0);
  layout_touch(m, &tid, TOUCH_MOVED, (x0 + x1) / 2, (y0 + y1) / 2);
  layout_touch(m, &tid, TOUCH_ENDED, x1, y1);
}

int main(void) {
  menu_state *m = &M;
  for (int gyro = 0; gyro <= 1; ++gyro) {
    fresh(m);
    m->gyro = gyro ? HUD_STEER_GYRO : 0;
    layout_enter(m);
    float sx = gyro ? 860 : 754, sy = 644; /* the shift pair's middle; the throttle's is (966, 650) */

    /* following the right keys, the shift keys are a group of their own to take, the pedals still the right group's */
    CHECK(hud_layout_hit(gyro, sx, sy) == HUD_EDIT_SHIFT);
    CHECK(hud_layout_hit(gyro, 966, 650) == HUD_KEYS_RIGHT);

    /* a tap takes them without splitting them off */
    drag(m, sx, sy, sx, sy);
    CHECK(m->lay_sel == HUD_EDIT_SHIFT);
    CHECK(m->key_shift_sep == 0);
    CHECK(m->lay_drag == 0);

    /* a drag splits them off, from where they were; the right keys stay */
    drag(m, sx, sy, sx - 200, sy - 100);
    CHECK(m->key_shift_sep == 1);
    CHECK(m->key_shift_k == 100 && m->key_shift_dx == -200 && m->key_shift_dy == -100);
    CHECK(m->key_k[HUD_KEYS_RIGHT] == 100 && m->key_dx[HUD_KEYS_RIGHT] == 0 && m->key_dy[HUD_KEYS_RIGHT] == 0);
    CHECK(hud_layout_hit(gyro, sx - 200, sy - 100) == HUD_EDIT_SHIFT);
    CHECK(hud_layout_hit(gyro, sx, sy) != HUD_EDIT_SHIFT); /* nothing left where they were */
    CHECK(hud_layout_hit(gyro, 966, 650) == HUD_KEYS_RIGHT);
    CHECK(m->garage_changed == 1);

    /* sized on their own */
    m->lay_sel = HUD_EDIT_SHIFT;
    tap_button(m, LAY_BIGGER);
    CHECK(m->key_shift_k == 110 && m->key_k[HUD_KEYS_RIGHT] == 100);
    tap_button(m, LAY_SMALLER);
    tap_button(m, LAY_SMALLER);
    CHECK(m->key_shift_k == 90 && m->key_k[HUD_KEYS_RIGHT] == 100);

    /* the panel's last key puts them back with the right keys, and splits them off again from where the right keys are */
    tap_button(m, LAY_SHIFT);
    CHECK(m->key_shift_sep == 0 && m->lay_sel == HUD_EDIT_SHIFT);
    CHECK(hud_layout_hit(gyro, sx, sy) == HUD_EDIT_SHIFT);
    m->key_k[HUD_KEYS_RIGHT] = 120, m->key_dx[HUD_KEYS_RIGHT] = -30, m->key_dy[HUD_KEYS_RIGHT] = -20;
    tap_button(m, LAY_SHIFT);
    CHECK(m->key_shift_sep == 1);
    CHECK(m->key_shift_k == 120 && m->key_shift_dx == -30 && m->key_shift_dy == -20);

    /* sized while still with the right keys: split off, starting from the right keys' size */
    tap_button(m, LAY_RESET);
    CHECK(m->key_shift_sep == 0 && m->key_shift_k == 100 && m->key_shift_dx == 0 && m->key_shift_dy == 0);
    CHECK(m->key_k[HUD_KEYS_RIGHT] == 100 && m->key_dx[HUD_KEYS_RIGHT] == 0);
    m->key_k[HUD_KEYS_RIGHT] = 130;
    m->lay_sel = HUD_EDIT_SHIFT;
    tap_button(m, LAY_SMALLER);
    CHECK(m->key_shift_sep == 1 && m->key_shift_k == 120 && m->key_k[HUD_KEYS_RIGHT] == 130);

    /* the size keys stop at the limits, split or not */
    for (int i = 0; i < 20; ++i) tap_button(m, LAY_BIGGER);
    CHECK(m->key_shift_k == (int)(HUD_KEY_K_MAX * 100 + 0.5f));
    for (int i = 0; i < 20; ++i) tap_button(m, LAY_SMALLER);
    CHECK(m->key_shift_k == (int)(HUD_KEY_K_MIN * 100 + 0.5f));

    /* dragged off every edge, they stay on the screen at their size */
    tap_button(m, LAY_RESET);
    m->lay_sel = HUD_EDIT_SHIFT;
    tap_button(m, LAY_BIGGER);
    tap_button(m, LAY_BIGGER);
    const float far[4][2] = { { 5000, 5000 }, { -5000, -5000 }, { 5000, -5000 }, { -5000, 5000 } };
    CHECK(m->key_shift_sep == 1 && m->key_shift_k == 120);
    for (int i = 0; i < 4; ++i) {
      float a, b, c, d; /* their middle now (a finger can only take them there) */
      shift_box(m->key_shift_k, m->key_shift_dx, m->key_shift_dy, &a, &b, &c, &d);
      float px = (a + c) / 2, py = (b + d) / 2;
      drag(m, px, py, px + far[i][0], py + far[i][1]);
      float x0, y0, x1, y1;
      shift_box(m->key_shift_k, m->key_shift_dx, m->key_shift_dy, &x0, &y0, &x1, &y1);
      CHECK(x0 >= -1.0f && y0 >= -1.0f && x1 <= 1025.0f && y1 <= 769.0f);
    }

    /* split off in place, nothing has moved; moved away, nothing is left where they were */
    tap_button(m, LAY_RESET);
    CHECK(hud_layout_hit(gyro, sx, sy) == HUD_EDIT_SHIFT);
    tap_button(m, LAY_SHIFT);
    CHECK(m->key_shift_sep == 1 && hud_layout_hit(gyro, sx, sy) == HUD_EDIT_SHIFT);
    m->key_shift_dx = -300, m->key_shift_dy = -300;
    layout_apply(m);
    CHECK(hud_layout_hit(gyro, sx, sy) == -1);
  }

  /* the options file's fourth line (app.c): the layout state survives layout_apply for the HUD */
  fresh(m);
  m->key_shift_sep = 1, m->key_shift_k = 130, m->key_shift_dx = -40, m->key_shift_dy = -60;
  layout_apply(m);
  CHECK(hud_layout_hit(0, 1024 - 40 + (706 + 48 - 1024) * 1.3f, 768 - 60 + (610 + 72 - 768) * 1.3f) == HUD_EDIT_SHIFT);

  printf(fails ? "shift layout: %d FAILED\n" : "shift layout: ok\n", fails);
  return fails != 0;
}
