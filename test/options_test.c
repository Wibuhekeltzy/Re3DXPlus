/* The OPTION screen's save_dir/option file (app.c options_save, options_load): the stick's
 * dead zone and centre ("stick") and the HUD page's four consoles ("hud") written and read back
 * with the lines before them, a file of an older version (fewer lines) read with the new fields
 * left as they were, bad numbers held to their limits, a short line ignored, and no write while
 * nothing has changed. app.c is included whole for its statics; the GL, sound and car calls it
 * reaches are left unresolved, as none is run. The headers the data tools write
 * (tools/gen_from_original.sh) must be in src/ first.
 *
 *   gcc -std=gnu99 -DRC_FPT=double -DGL_ZERO=0 -DGL_ONE_MINUS_DST_COLOR=0x0307 -Isrc -Itest \
 *       test/options_test.c -lm -Wl,--unresolved-symbols=ignore-all -o options_test && ./options_test
 */
#include "../src/gl.h"
/* the declaration-only GL stub (test/glstub.h) has not the framebuffer calls app.c makes (none is run) */
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_RENDERBUFFER 0x8D41
#define GL_RGB565 0x8D62
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_STENCIL_BUFFER_BIT 0x0400
#define GL_NO_ERROR 0
void glFinish(void);
void glFlush(void);
void glBindFramebuffer(GLenum, GLuint);
void glGenFramebuffers(GLsizei, GLuint *);
void glGenRenderbuffers(GLsizei, GLuint *);
void glBindRenderbuffer(GLenum, GLuint);
void glRenderbufferStorage(GLenum, GLenum, GLsizei, GLsizei);
void glFramebufferRenderbuffer(GLenum, GLenum, GLenum, GLuint);
void glCopyTexSubImage2D(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);

#include "../src/app.c"

#include <unistd.h>

static int fails;
#define CHECK(c) \
  do { \
    if (!(c)) { \
      printf("FAIL line %d: %s\n", __LINE__, #c); \
      ++fails; \
    } \
  } while (0)

static char dir[1024];

static void put(const char *text) {
  char path[1100];
  snprintf(path, sizeof path, "%s/option", dir);
  FILE *f = fopen(path, "w");
  if (!f) return;
  fputs(text, f);
  fclose(f);
}

/* the options as the menu starts with them */
static void defaults(void) {
  memset(&G.menu, 0, sizeof G.menu);
  for (int g = 0; g < 3; ++g) G.menu.key_k[g] = 100;
  for (int g = 0; g < HUD_RACE_GROUPS; ++g) G.menu.hud_k[g] = 100;
  G.menu.key_shift_k = 100;
  G.menu.stick_dead = 8;
  G.menu.gyro_sens = 5;
}

static int exists(void) {
  char path[1100];
  snprintf(path, sizeof path, "%s/option", dir);
  return access(path, F_OK) == 0;
}

static void drop(void) {
  char path[1100];
  snprintf(path, sizeof path, "%s/option", dir);
  unlink(path);
}

int main(void) {
  const char *tmp = getenv("TMPDIR");
  snprintf(dir, sizeof dir, "%s/optionsXXXXXX", tmp && *tmp ? tmp : "/tmp");
  if (!mkdtemp(dir)) {
    printf("no temporary directory\n");
    return 1;
  }
  snprintf(G.save_dir, sizeof G.save_dir, "%s", dir);

  /* everything written is read back */
  defaults();
  G.menu.gyro = HUD_STEER_STICK, G.menu.gyro_sens = 7, G.menu.pedal_slide = 1;
  G.menu.key_k[0] = 110, G.menu.key_dx[1] = -25, G.menu.key_dy[2] = 30;
  G.menu.key_shift_sep = 1, G.menu.key_shift_k = 130, G.menu.key_shift_dx = -40, G.menu.key_shift_dy = -60;
  G.menu.story_zone = 1, G.menu.story_test = 3, G.menu.ta_traffic = 1;
  G.menu.stick_dead = 17, G.menu.stick_fixed = 1;
  static const int hk[4] = { 110, 120, 130, 140 }, hx[4] = { 1, -2, 3, -4 }, hy[4] = { 5, -6, 7, -8 };
  for (int g = 0; g < 4; ++g) G.menu.hud_k[g] = hk[g], G.menu.hud_dx[g] = hx[g], G.menu.hud_dy[g] = hy[g];
  G.options_global = 0;
  options_save();
  CHECK(exists() && G.options_global == 1);
  {
    char path[1100], text[2048] = "";
    snprintf(path, sizeof path, "%s/option", dir);
    FILE *f = fopen(path, "r");
    size_t n = f ? fread(text, 1, sizeof text - 1, f) : 0;
    if (f) fclose(f);
    text[n] = 0;
    CHECK(strstr(text, "\nstory 1\nstick 17 1\nhud 110 1 5 120 -2 -6 130 3 7 140 -4 -8\n") != NULL);
  }
  defaults();
  CHECK(options_load() == 1);
  CHECK(G.menu.gyro == HUD_STEER_STICK && G.menu.gyro_sens == 7 && G.menu.pedal_slide == 1);
  CHECK(G.menu.key_k[0] == 110 && G.menu.key_dx[1] == -25 && G.menu.key_dy[2] == 30);
  CHECK(G.menu.key_shift_sep == 1 && G.menu.key_shift_k == 130 && G.menu.key_shift_dx == -40 && G.menu.key_shift_dy == -60);
  CHECK(G.menu.story_zone == 1 && G.menu.story_test == 3 && G.menu.ta_traffic == 1);
  CHECK(G.menu.stick_dead == 17 && G.menu.stick_fixed == 1);
  for (int g = 0; g < 4; ++g) CHECK(G.menu.hud_k[g] == hk[g] && G.menu.hud_dx[g] == hx[g] && G.menu.hud_dy[g] == hy[g]);

  /* nothing changed, nothing written; a change in any one new field is */
  drop();
  options_save();
  CHECK(!exists());
  G.menu.stick_fixed = 0;
  options_save();
  CHECK(exists());
  drop();
  G.menu.hud_dy[3] = -9;
  options_save();
  CHECK(exists());
  drop();
  G.menu.hud_k[0] = 150;
  options_save();
  CHECK(exists());
  drop();
  G.menu.stick_dead = 18;
  options_save();
  CHECK(exists());

  /* a file of the version before (the lines to "story"): the new fields stay as they were */
  put("option 0 5 1 1 0 1 1 1 1 0 1 1 1 0\nkeys 100 0 0 100 0 0 100 0 0\ntest 0\ntraffic 0\nshift 0 100 0 0\nstory 1\n");
  defaults();
  G.menu.stick_dead = 23, G.menu.stick_fixed = 1;
  G.menu.hud_k[2] = 120, G.menu.hud_dx[2] = 9;
  CHECK(options_load() == 1);
  CHECK(G.menu.story_zone == 1);
  CHECK(G.menu.stick_dead == 23 && G.menu.stick_fixed == 1 && G.menu.hud_k[2] == 120 && G.menu.hud_dx[2] == 9);

  /* and the one before that (no "story" line either), and a file with only the first line */
  put("option 1 5 1 1 0 1 1 1 1 0 1 1 1 0\nkeys 100 0 0 100 0 0 100 0 0\ntest 0\ntraffic 0\nshift 0 100 0 0\n");
  defaults();
  CHECK(options_load() == 1 && G.menu.gyro == 1 && G.menu.stick_dead == 8 && G.menu.hud_k[0] == 100);
  put("option 2 5 1 1 0 1 1 1 1 0 1 1 1 0\n");
  defaults();
  CHECK(options_load() == 1 && G.menu.gyro == 2 && G.menu.stick_dead == 8 && G.menu.hud_k[3] == 100);

  /* bad numbers are held to their limits (or left at the default) */
  put("option 0 5 1 1 0 1 1 1 1 0 1 1 1 0\nkeys 100 0 0 100 0 0 100 0 0\ntest 0\ntraffic 0\nshift 0 100 0 0\nstory 0\n"
      "stick 99 7\nhud 5 5000 -5000 500 2000 -2000 60 -2000 2000 160 0 0\n");
  defaults();
  CHECK(options_load() == 1);
  CHECK(G.menu.stick_dead == 8 && G.menu.stick_fixed == 0); /* (7 is not 1: floating) */
  CHECK(G.menu.hud_k[0] == 60 && G.menu.hud_dx[0] == 0 && G.menu.hud_dy[0] == 0);
  CHECK(G.menu.hud_k[1] == 160 && G.menu.hud_dx[1] == 2000 && G.menu.hud_dy[1] == -2000);
  CHECK(G.menu.hud_k[2] == 60 && G.menu.hud_dx[2] == -2000 && G.menu.hud_dy[2] == 2000);
  CHECK(G.menu.hud_k[3] == 160 && G.menu.hud_dx[3] == 0 && G.menu.hud_dy[3] == 0);
  put("option 0 5 1 1 0 1 1 1 1 0 1 1 1 0\nkeys 100 0 0 100 0 0 100 0 0\ntest 0\ntraffic 0\nshift 0 100 0 0\nstory 0\n"
      "stick -3 1\n");
  defaults();
  CHECK(options_load() == 1 && G.menu.stick_dead == 8 && G.menu.stick_fixed == 1);
  put("option 0 5 1 1 0 1 1 1 1 0 1 1 1 0\nkeys 100 0 0 100 0 0 100 0 0\ntest 0\ntraffic 0\nshift 0 100 0 0\nstory 0\n"
      "stick 50 0\n");
  defaults();
  CHECK(options_load() == 1 && G.menu.stick_dead == 50 && G.menu.stick_fixed == 0);

  /* a "hud" line cut short is ignored whole */
  put("option 0 5 1 1 0 1 1 1 1 0 1 1 1 0\nkeys 100 0 0 100 0 0 100 0 0\ntest 0\ntraffic 0\nshift 0 100 0 0\nstory 0\n"
      "stick 12 1\nhud 110 1 5 120 -2 -6 130 3 7 140 -4\n");
  defaults();
  CHECK(options_load() == 1 && G.menu.stick_dead == 12 && G.menu.stick_fixed == 1);
  for (int g = 0; g < 4; ++g) CHECK(G.menu.hud_k[g] == 100 && G.menu.hud_dx[g] == 0 && G.menu.hud_dy[g] == 0);

  /* no file, no options */
  drop();
  CHECK(options_load() == 0);

  rmdir(dir);
  printf(fails ? "options: %d FAILED\n" : "options: ok\n", fails);
  return fails != 0;
}
