/* The For Racing meter's REV lamp (hud.c: lamp_prepare, racing_lamp_lit, hud_set_car's hook,
 * draw_meters' swap, hud_warm's warm-up): the lamp is found among a car's meter parts by its place
 * and kind (491, 283, fixed, not added) and, to the byte, by its pixels (the lamp off of the zone's
 * time of day), and only on the For Racing meter; its lit sprite then takes its place while the
 * arcade's probes hold (the rev rate over .86, CSeq::nCnt % 5 over 1). Any other part, size, art,
 * meter or failed upload leaves the meter as it was.
 *
 * hud.c is included whole (its statics are what is tested), the GL calls are recorded below, and
 * the dial art is a meters.bin written here for the table of test/racing_lamp_meterdb.h. Run it
 * through test/racing_lamp_test.py: that makes the lamp's header (tools/racing_lamp_data.py) from
 * sprites of its own and builds this against a tree that has the table above as src/meterdb.h.
 *
 *   python3 test/racing_lamp_test.py
 */
#include "gl.h"

/* the lamp's upload is told apart from older GL errors: this test's glGetError says what it is told */
static GLenum test_glGetError(void);
#define glGetError test_glGetError

#include "../src/hud.c"

#ifndef RACING_LAMP
#error "no racing_lamp_data.h: run this through test/racing_lamp_test.py"
#endif

static int fails;
#define CHECK(c) \
  do { \
    if (!(c)) { \
      printf("FAIL line %d: %s\n", __LINE__, #c); \
      ++fails; \
    } \
  } while (0)

/* ---- the GL calls the meters reach, recorded ---- */

#define T_TEX 512
#define T_PX (64 * 64 * 4)
static struct {
  int live;
  unsigned w, h;
  unsigned char px[T_PX]; /* what a 64 x 64 texture was given */
} t_tex[T_TEX];
static GLuint t_next = 1, t_bound;
static int t_deleted;
static GLuint t_draws[64]; /* the texture bound at each glDrawArrays */
static int t_ndraws;
static int t_stale; /* GL errors from before: one more each glGetError, till they run out */
static int t_always; /* every glGetError is an error */
static GLenum t_next_error; /* the error the next glGetError tells */
static const unsigned char *t_fail_on; /* a 64 x 64 upload of these pixels leaves an error */

static GLenum test_glGetError(void) {
  if (t_always) return 0x0502;
  if (t_stale > 0) {
    --t_stale;
    return 0x0502;
  }
  GLenum e = t_next_error;
  t_next_error = 0;
  return e;
}

void glGenTextures(GLsizei n, GLuint *ids) {
  for (GLsizei i = 0; i < n; ++i) {
    ids[i] = t_next++;
    if (ids[i] < T_TEX) t_tex[ids[i]].live = 1;
  }
}
void glBindTexture(GLenum target, GLuint id) { (void)target, t_bound = id; }
void glDeleteTextures(GLsizei n, const GLuint *ids) {
  for (GLsizei i = 0; i < n; ++i) {
    if (ids[i] < T_TEX) t_tex[ids[i]].live = 0;
    ++t_deleted;
  }
}
void glTexParameteri(GLenum a, GLenum b, GLint c) { (void)a, (void)b, (void)c; }
void glTexImage2D(GLenum target, GLint level, GLint ifmt, GLsizei w, GLsizei h, GLint border, GLenum fmt, GLenum type,
                  const void *px) {
  (void)target, (void)level, (void)ifmt, (void)border, (void)fmt, (void)type;
  if (t_bound >= T_TEX) return;
  t_tex[t_bound].w = (unsigned)w;
  t_tex[t_bound].h = (unsigned)h;
  if (w == 64 && h == 64) {
    memcpy(t_tex[t_bound].px, px, T_PX);
    if (t_fail_on && !memcmp(px, t_fail_on, T_PX)) t_next_error = 0x0505;
  }
}
void glVertexAttribPointer(GLuint i, GLint n, GLenum t, GLboolean norm, GLsizei stride, const void *p) {
  (void)i, (void)n, (void)t, (void)norm, (void)stride, (void)p;
}
void glDrawArrays(GLenum mode, GLint first, GLsizei n) {
  (void)mode, (void)first, (void)n;
  if (t_ndraws < 64) t_draws[t_ndraws] = t_bound;
  ++t_ndraws;
}
void glEnableVertexAttribArray(GLuint i) { (void)i; }
void glDisableVertexAttribArray(GLuint i) { (void)i; }
void glBlendFunc(GLenum s, GLenum d) { (void)s, (void)d; }
void glUseProgram(GLuint p) { (void)p; }
void glUniformMatrix4fv(GLint l, GLsizei n, GLboolean t, const GLfloat *v) { (void)l, (void)n, (void)t, (void)v; }
void glUniform1i(GLint l, GLint v) { (void)l, (void)v; }
void glUniform4f(GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d) { (void)l, (void)a, (void)b, (void)c, (void)d; }
void glActiveTexture(GLenum t) { (void)t; }
void glEnable(GLenum c) { (void)c; }
void glDisable(GLenum c) { (void)c; }
void glBindBuffer(GLenum t, GLuint b) { (void)t, (void)b; }

float car_kmh(const car_state *c) { return fabsf(c->speed) * 3.6f; }

static int live_textures(void) {
  int n = 0;
  for (int i = 0; i < T_TEX; ++i) n += t_tex[i].live;
  return n;
}

/* ---- the dial art: the lamp's four sprites as the data header has them, the rest filler ---- */

static unsigned char off_m[T_PX], off_n[T_PX], on_m[T_PX], on_n[T_PX];

/* what a part of the fixture's table looks like (test/racing_lamp_meterdb.h says why each one is) */
static void part_art(int car, int zone, int part, unsigned *w, unsigned *h, unsigned char *px) {
  const unsigned char *lamp = NULL;
  *w = *h = 16;
  switch (car) {
  case 0:
    if ((zone == 0 && part == 1) || (zone == 2 && part == 1) || (zone == 4 && part == 0)) lamp = off_m;
    if ((zone == 3 && part == 2) || (zone == 5 && part == 0)) lamp = off_n;
    break;
  case 1:
    if (zone == 2 && part == 1) lamp = off_m;
    if (zone == 3 && part == 1) lamp = off_n;
    break;
  case 2:
    break;
  case 3:
    if (zone == 2 && part == 1) lamp = off_n;
    if (zone == 3 && part == 1) lamp = off_m;
    break;
  case 4:
    if (zone == 2) lamp = off_m;
    break;
  case 5:
    if (zone == 2 && part >= 1) lamp = off_m;
    break;
  }
  if (lamp) {
    *w = *h = 64;
    memcpy(px, lamp, T_PX);
  } else {
    for (int i = 0; i < 16 * 16 * 4; ++i) px[i] = (unsigned char)(car * 31 + zone * 17 + part * 7 + i);
  }
  if (car == 1 && part == 1) px[zone == 2 ? 5000 : T_PX - 1] ^= 0x01; /* one byte off */
  if (car == 2 && part == 1) { /* a sprite of half the size: its pixels the lamp's first ones */
    *w = *h = 32;
    memcpy(px, zone == 2 ? off_m : off_n, 32 * 32 * 4);
  }
}

/* meters.bin as tools/hudpack.py writes it, for the fixture's table: every part its own sprite */
static void write_meters(const char *dir) {
  static unsigned short ids[METER_CARS][METER_ZONES][METERDB_MAX_PARTS];
  static unsigned sw[256], sh[256], so[256];
  static unsigned char blob[1 << 20];
  unsigned count = 0, used = 0;
  memset(ids, 0xff, sizeof ids);
  for (int c = 0; c < METER_CARS; ++c)
    for (int z = 0; z < METER_ZONES; ++z)
      for (int i = 0; i < METERDB[c].n[z]; ++i) {
        unsigned w, h;
        unsigned char px[T_PX];
        part_art(c, z, i, &w, &h, px);
        if (count >= 256 || used + w * h * 4 > sizeof blob) {
          printf("FAIL the fixture's art does not fit\n");
          exit(1);
        }
        ids[c][z][i] = (unsigned short)count;
        sw[count] = w, sh[count] = h, so[count] = used;
        memcpy(blob + used, px, w * h * 4);
        used += w * h * 4;
        ++count;
      }
  char path[1024];
  snprintf(path, sizeof path, "%s/meters.bin", dir);
  FILE *f = fopen(path, "wb");
  if (!f) {
    printf("FAIL cannot write %s\n", path);
    exit(1);
  }
  unsigned n[2] = { METER_CARS, count };
  unsigned base = 12 + (unsigned)sizeof ids + 12 * count;
  fwrite("WMT1", 1, 4, f);
  fwrite(n, 4, 2, f);
  fwrite(ids, sizeof ids, 1, f);
  for (unsigned i = 0; i < count; ++i) {
    unsigned t[3] = { sw[i], sh[i], base + so[i] };
    fwrite(t, 4, 3, f);
  }
  fwrite(blob, 1, used, f);
  fclose(f);
}

/* ---- the checks ---- */

/* the car's meters of a style and time of day, loaded afresh (the same ones again would be kept) */
static void load(int style, int car, int night) {
  hud_set_car(-1, 0);
  hud_set_meter(style);
  hud_set_car(car, night);
}

/* draw the meters at a rev rate and nCnt; the draws are left in t_draws */
static void draw(float rev, int frame) {
  car_state c;
  memset(&c, 0, sizeof c);
  c.rev_rate = rev;
  c.rev_lamp_frame = frame;
  t_ndraws = 0;
  draw_meters(&c);
}

/* is the lamp's part drawn lit: 1 its lit sprite, 0 its own, -1 neither */
static int lamp_lit_at(int slot, float rev, int frame) {
  draw(rev, frame);
  if (slot >= t_ndraws) return -1;
  return t_draws[slot] == racing_lamp_on.tex && racing_lamp_on.tex ? 1 : t_draws[slot] == METER[slot].tex ? 0 : -1;
}

static void test_art(void) {
  CHECK(lamp_inflate(off_m, lamp_off_m, sizeof lamp_off_m));
  CHECK(lamp_inflate(off_n, lamp_off_n, sizeof lamp_off_n));
  CHECK(lamp_inflate(on_m, lamp_on_m, sizeof lamp_on_m));
  CHECK(lamp_inflate(on_n, lamp_on_n, sizeof lamp_on_n));
  /* four sprites of their own: a mix-up of morning and night, or of off and on, shows below */
  CHECK(memcmp(off_m, off_n, T_PX) && memcmp(off_m, on_m, T_PX) && memcmp(off_m, on_n, T_PX));
  CHECK(memcmp(off_n, on_m, T_PX) && memcmp(off_n, on_n, T_PX) && memcmp(on_m, on_n, T_PX));
  /* what is not the lamp's size or not zlib's is refused, not read past */
  unsigned char buf[T_PX];
  CHECK(!lamp_inflate(buf, lamp_off_m, 8));
  CHECK(!lamp_inflate(buf, lamp_off_m, sizeof lamp_off_m - 1));
  static const unsigned char junk[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
  CHECK(!lamp_inflate(buf, junk, sizeof junk));
  /* zlib streams that are well made, of other lengths: 10 zero bytes (inflates fine; too few) and
   * 16385 (does not fit) */
  static const unsigned char ten[] = { 0x78, 0xda, 0x63, 0x60, 0x80, 0x01, 0x00, 0x00, 0x0a, 0x00, 0x01 };
  static const unsigned char many[] = { 0x78, 0xda, 0xed, 0xc1, 0x31, 0x01, 0x00, 0x00, 0x00, 0xc2, 0xa0, 0xf5, 0x4f,
                                        0x6d, 0x0c, 0x1f, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbb, 0x01, 0x40, 0x01, 0x00, 0x01 };
  CHECK(!lamp_inflate(buf, ten, sizeof ten));
  CHECK(!lamp_inflate(buf, many, sizeof many));
}

static void test_probes(void) {
  /* the arcade's: the rev rate strictly over .86 and nCnt % 5 over 1 (lit 3 frames of 5) */
  CHECK(!racing_lamp_lit(0.86f, 2));
  CHECK(!racing_lamp_lit(nextafterf(0.86f, 0.0f), 2));
  CHECK(racing_lamp_lit(nextafterf(0.86f, 1.0f), 2));
  CHECK(!racing_lamp_lit(0.0f, 3) && !racing_lamp_lit(-1.0f, 3));
  CHECK(racing_lamp_lit(1.0f, 3) && racing_lamp_lit(1.5f, 4));
  const int lit[10] = { 0, 0, 1, 1, 1, 0, 0, 1, 1, 1 };
  for (int f = 0; f < 10; ++f) CHECK(racing_lamp_lit(0.9f, f) == lit[f]);
  int n = 0;
  for (int f = 1000; f < 1100; ++f) n += racing_lamp_lit(0.9f, f);
  CHECK(n == 60);
  for (int f = 1000; f < 1100; ++f) CHECK(!racing_lamp_lit(0.5f, f));
}

static void test_found(void) {
  /* the For Racing meter's morning: zone 2, the lamp its second part */
  load(1, 0, 0);
  CHECK(meter_night == 2);
  CHECK(racing_lamp_slot == 1);
  CHECK(racing_lamp_on.tex && racing_lamp_on.w == 64 && racing_lamp_on.h == 64);
  CHECK(racing_lamp_on.tex != METER[1].tex);
  CHECK(t_tex[racing_lamp_on.tex].w == 64 && !memcmp(t_tex[racing_lamp_on.tex].px, on_m, T_PX)); /* the morning's lit lamp */
  CHECK(!memcmp(t_tex[METER[1].tex].px, off_m, T_PX)); /* the dial's own part is the lamp off */
  CHECK(live_textures() == 3 + 1);
  /* the lamp's part, lit or not by the arcade's probes; the others never change */
  CHECK(lamp_lit_at(1, 0.9f, 2) == 1 && t_ndraws == 3);
  CHECK(t_draws[0] == METER[0].tex && t_draws[2] == METER[2].tex);
  CHECK(lamp_lit_at(1, 0.9f, 4) == 1 && lamp_lit_at(1, 0.9f, 3) == 1);
  CHECK(lamp_lit_at(1, 0.9f, 0) == 0 && lamp_lit_at(1, 0.9f, 1) == 0 && lamp_lit_at(1, 0.9f, 5) == 0);
  CHECK(lamp_lit_at(1, 0.86f, 2) == 0 && lamp_lit_at(1, 0.5f, 2) == 0 && lamp_lit_at(1, 0.0f, 2) == 0);
  CHECK(lamp_lit_at(1, nextafterf(0.86f, 1.0f), 3) == 1 && lamp_lit_at(1, 1.4f, 4) == 1);
  CHECK(t_draws[0] == METER[0].tex && t_draws[2] == METER[2].tex);
  int lit = 0;
  for (int f = 0; f < 100; ++f) lit += lamp_lit_at(1, 0.95f, f) == 1;
  CHECK(lit == 60);
  /* the same car and zone again is kept, not loaded twice */
  GLuint next = t_next;
  hud_set_car(0, 0);
  CHECK(t_next == next && racing_lamp_slot == 1 && racing_lamp_on.tex);

  /* the night's: zone 3, the lamp its third part, the night's art */
  load(1, 0, 1);
  CHECK(meter_night == 3);
  CHECK(racing_lamp_slot == 2);
  CHECK(racing_lamp_on.tex && !memcmp(t_tex[racing_lamp_on.tex].px, on_n, T_PX));
  CHECK(!memcmp(t_tex[METER[2].tex].px, off_n, T_PX));
  CHECK(live_textures() == 3 + 1);
  CHECK(lamp_lit_at(2, 0.9f, 2) == 1 && lamp_lit_at(2, 0.9f, 1) == 0 && lamp_lit_at(2, 0.5f, 2) == 0);
  CHECK(t_draws[0] == METER[0].tex && t_draws[1] == METER[1].tex);
}

static void test_not_found(void) {
  /* the Normal and SPECIAL meters have parts at the lamp's place with the lamp's art: not the lamp */
  for (int style = 0; style <= 2; style += 2)
    for (int night = 0; night <= 1; ++night) {
      load(style, 0, night);
      CHECK(racing_lamp_slot == -1 && racing_lamp_on.tex == 0);
      CHECK(live_textures() == METERDB[0].n[2 * style + night]);
      for (int i = 0; i < METERDB[0].n[2 * style + night]; ++i) CHECK(lamp_lit_at(i, 1.0f, 3) == 0);
    }
  /* one byte off, another size, the other time of day's art */
  for (int car = 1; car <= 3; ++car)
    for (int night = 0; night <= 1; ++night) {
      load(1, car, night);
      CHECK(racing_lamp_slot == -1 && racing_lamp_on.tex == 0);
      CHECK(live_textures() == 2);
      CHECK(lamp_lit_at(1, 1.0f, 3) == 0 && lamp_lit_at(0, 1.0f, 3) == 0);
    }
  /* the lamp's art, not the lamp's part: added, a needle, a pixel off */
  load(1, 4, 0);
  CHECK(racing_lamp_slot == -1 && racing_lamp_on.tex == 0);
  CHECK(live_textures() == 5);
  for (int i = 0; i < 5; ++i) CHECK(lamp_lit_at(i, 1.0f, 3) == 0);
  /* a zone with no parts at all */
  load(1, 4, 1);
  draw(1.0f, 3);
  CHECK(racing_lamp_slot == -1 && t_ndraws == 0 && live_textures() == 0);
  /* a car that is not there, and the meters not loaded at all */
  load(1, 99, 0);
  CHECK(racing_lamp_slot == -1 && racing_lamp_on.tex == 0 && live_textures() == 0);
  draw(1.0f, 3);
  CHECK(t_ndraws == 0);
}

static void test_prepare(void) {
  /* lamp_prepare's checks one by one, on a loaded meter whose lamp is cleared again: the arcade's
   * lamp off pixels are the lamp at slot 1 of zone 2, and nothing short of them is */
  load(1, 0, 0);
  racing_lamp_clear();
  CHECK(racing_lamp_slot == -1 && METER[0].tex && METER[1].tex && METER[2].tex && !METER[5].tex);
  int live = live_textures();
  lamp_prepare(off_m, 64, 32, 2, 1); /* sizes */
  lamp_prepare(off_m, 32, 64, 2, 1);
  lamp_prepare(off_m, 0, 0, 2, 1);
  lamp_prepare(off_m, 64, 64, 2, -1); /* places (the far ones are not looked up: they would fault) */
  lamp_prepare(off_m, 64, 64, 2, METERDB_MAX_PARTS);
  lamp_prepare(off_m, 64, 64, 2, -(1 << 28));
  lamp_prepare(off_m, 64, 64, 2, 1 << 28);
  lamp_prepare(off_m, 64, 64, 2, 5);
  lamp_prepare(off_n, 64, 64, 2, 1); /* the other time of day's lamp, the lamp lit */
  lamp_prepare(on_m, 64, 64, 2, 1);
  lamp_prepare(off_m, 64, 64, 3, 1);
  CHECK(racing_lamp_slot == -1 && racing_lamp_on.tex == 0 && live_textures() == live);
  /* the lamp (and what is asked of it) */
  lamp_prepare(off_m, 64, 64, 2, 1);
  CHECK(racing_lamp_slot == 1 && racing_lamp_on.tex && live_textures() == live + 1);
  CHECK(!memcmp(t_tex[racing_lamp_on.tex].px, on_m, T_PX));
  /* found already: the second one asked is not taken (and no texture is made for it) */
  GLuint lamp = racing_lamp_on.tex, next = t_next;
  lamp_prepare(off_m, 64, 64, 2, 0);
  CHECK(racing_lamp_slot == 1 && racing_lamp_on.tex == lamp && t_next == next && live_textures() == live + 1);
  racing_lamp_clear();
  CHECK(!t_tex[lamp].live && live_textures() == live);
  lamp_prepare(off_n, 64, 64, 3, 2); /* the night's, with the night's art */
  CHECK(racing_lamp_slot == 2 && racing_lamp_on.tex && !memcmp(t_tex[racing_lamp_on.tex].px, on_n, T_PX));
  hud_set_car(-1, 0);
  CHECK(live_textures() == 0);
}

static void test_two(void) {
  /* two parts that are the lamp: the first one is, with one lit sprite (not two to lose) */
  load(1, 5, 0);
  CHECK(racing_lamp_slot == 1 && racing_lamp_on.tex);
  CHECK(live_textures() == 3 + 1);
  CHECK(lamp_lit_at(1, 1.0f, 3) == 1 && t_draws[2] == METER[2].tex);
}

static void test_gl_errors(void) {
  /* errors from before the upload are not the upload's */
  t_stale = 3;
  load(1, 0, 0);
  CHECK(racing_lamp_slot == 1 && racing_lamp_on.tex && live_textures() == 4);
  t_stale = 0;
  /* the lit art's upload fails: no lamp, the failed texture let go, the dial drawn as it is */
  int deleted = t_deleted;
  t_fail_on = on_m;
  load(1, 0, 0);
  t_fail_on = NULL;
  t_next_error = 0;
  CHECK(racing_lamp_slot == -1 && racing_lamp_on.tex == 0);
  CHECK(live_textures() == 3 && t_deleted > deleted);
  CHECK(lamp_lit_at(1, 1.0f, 3) == 0 && t_ndraws == 3);
  /* the night's art fails alone: the morning's would not have */
  t_fail_on = on_n;
  load(1, 0, 0);
  CHECK(racing_lamp_slot == 1 && racing_lamp_on.tex && live_textures() == 4);
  load(1, 0, 1);
  CHECK(racing_lamp_slot == -1 && racing_lamp_on.tex == 0 && live_textures() == 3);
  t_fail_on = NULL;
  t_next_error = 0;
  /* a GL that only ever answers with an error: the clearing of the old ones stops, and the lamp is not taken */
  t_always = 1;
  load(1, 0, 0);
  t_always = 0;
  CHECK(racing_lamp_slot == -1 && racing_lamp_on.tex == 0 && live_textures() == 3);
  CHECK(lamp_lit_at(1, 1.0f, 3) == 0);
  /* and the same load once the GL is well again */
  load(1, 0, 0);
  CHECK(racing_lamp_slot == 1 && racing_lamp_on.tex);
}

static void test_reload(void) {
  /* the lit lamp goes with its meters (hud_set_car's own clearing, not load's): to another meter,
   * another time of day, another car, none */
  load(1, 0, 0);
  GLuint lamp = racing_lamp_on.tex;
  CHECK(lamp && t_tex[lamp].live);
  hud_set_meter(0);
  hud_set_car(0, 0);
  CHECK(!t_tex[lamp].live && racing_lamp_on.tex == 0 && racing_lamp_slot == -1);
  hud_set_meter(1);
  hud_set_car(0, 0);
  lamp = racing_lamp_on.tex;
  CHECK(lamp && racing_lamp_slot == 1);
  hud_set_car(0, 1);
  CHECK(!t_tex[lamp].live && racing_lamp_slot == 2 && racing_lamp_on.tex && racing_lamp_on.tex != lamp);
  lamp = racing_lamp_on.tex;
  hud_set_car(1, 1);
  CHECK(!t_tex[lamp].live && racing_lamp_slot == -1 && racing_lamp_on.tex == 0);
  hud_set_car(0, 1);
  lamp = racing_lamp_on.tex;
  CHECK(lamp && racing_lamp_slot == 2);
  hud_set_car(-1, 0);
  CHECK(!t_tex[lamp].live && racing_lamp_slot == -1 && racing_lamp_on.tex == 0 && live_textures() == 0);
}

static void test_warm(void) {
  /* the race title page's warm-up draws the lit lamp once, after the dial's parts: on its budget */
  tex_prog = 1;
  load(1, 0, 0);
  t_ndraws = 0;
  CHECK(hud_warm(1024, 768, 3) == 1); /* the three parts, the lamp left */
  CHECK(t_ndraws == 3 && racing_lamp_warm == 0);
  t_ndraws = 0;
  CHECK(hud_warm(1024, 768, 3) == 0); /* the lamp's: the one more */
  CHECK(t_ndraws == 1 && t_draws[0] == racing_lamp_on.tex && racing_lamp_warm == 1);
  t_ndraws = 0;
  CHECK(hud_warm(1024, 768, 3) == 0 && t_ndraws == 0); /* and once only */
  /* a new lamp is warmed again; no lamp, no draw */
  load(1, 0, 1);
  t_ndraws = 0;
  CHECK(hud_warm(1024, 768, 100) == 0);
  CHECK(t_ndraws == 4 && t_draws[3] == racing_lamp_on.tex);
  load(0, 0, 0);
  t_ndraws = 0;
  CHECK(hud_warm(1024, 768, 100) == 0 && t_ndraws == 2);
  tex_prog = 0;
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : ".";
  test_art();
  write_meters(dir);
  meters_index(dir);
  CHECK(meter_sprites > 0 && meter_table);
  test_probes();
  test_found();
  test_not_found();
  test_prepare();
  test_two();
  test_gl_errors();
  test_reload();
  test_warm();
  if (fails) {
    printf("racing_lamp_test: %d FAILED\n", fails);
    return 1;
  }
  printf("racing_lamp_test: ok\n");
  return 0;
}
