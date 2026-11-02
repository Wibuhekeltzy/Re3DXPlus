/* Host GL mock; includes real cache implementation, not a device rendering test. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/anm.c"
static int uploads, deletes;
void glGenTextures(GLsizei n, GLuint *p) { (void)n; *p=(GLuint)(10+uploads); }
void glBindTexture(GLenum a, GLuint b) { (void)a; (void)b; }
void glTexParameteri(GLenum a, GLenum b, GLint c) { (void)a; (void)b; (void)c; }
void glTexImage2D(GLenum a, GLint b, GLint c, GLsizei w, GLsizei h, GLint d, GLenum e, GLenum f, const void *p) {
  (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; assert(w==1 && h==1 && p); ++uploads;
}
void glDeleteTextures(GLsizei n, const GLuint *p) { (void)n;(void)p;++deletes; }
int main(void) {
  assert(!anm_sprite_texture(-1) && !anm_sprite_texture(0));
  GLuint t[1]={0}; long off[1]={0}; uint32_t zs[1]={0}, used[1]={0};
  float size[1][2]={{1,1}}; int ids[1]={9}; uint16_t cols[1][2]={{0,0}};
  FILE *f=tmpfile(); assert(f); assert(fwrite("abcd",1,4,f)==4); fflush(f);
  A.tex=t; A.offset=off; A.zsize=zs; A.used=used; A.size=size; A.ids=ids; A.cols=cols;
  A.file=f; A.base_sprites=1; A.nsprites=1;
  assert(!anm_sprite_texture(1)); assert(anm_sprite_texture(0)==10 && uploads==1);
  A.clock=100; assert(anm_sprite_texture(0)==10 && used[0]==100 && uploads==1);
  A.clock=299; anm_trim(); assert(deletes==1 && !t[0]);
  assert(anm_sprite_texture(0)==11 && uploads==2 && used[0]==300);
  t[0]=0; off[0]=100; assert(!anm_sprite_texture(0));
  fclose(f); puts("anm borrowed texture mock: passed (no context-loss/device proof)");
}
