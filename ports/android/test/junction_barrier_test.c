#include "../src/junction_barrier.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../src/gl.h"
static render_vertex captured[8];
static int submissions, depth_enabled, missing;
static char trace[128]; static size_t trace_len;
static void mark(char c) { assert(trace_len+1<sizeof trace); trace[trace_len++]=c; trace[trace_len]=0; }
int anm_sprite(int id) { assert(id==0x569); mark('S'); return missing ? -1 : 7; }
unsigned int anm_sprite_texture(int id) { assert(id==7); mark('X'); return 23; }
void glActiveTexture(GLenum unit) { assert(unit==GL_TEXTURE0); mark('T'); }
void glBlendEquation(GLenum mode) { assert(mode==GL_FUNC_ADD); mark('B'); }
void glEnable(GLenum what) { assert(what==GL_DEPTH_TEST); depth_enabled++; mark('D'); }
void glEnableVertexAttribArray(GLuint index) { assert(index<3); mark((char)('0'+index)); }
void render_sprites_by_colour(const mat4 *vp, const render_vertex *v, int n, unsigned int t, render_params *p) {
  assert(vp && p && n==2 && t==23); mark('R'); memcpy(captured,v,sizeof captured); submissions++;
}
int main(void) {
  junction_barrier b; junction_barrier_reset(&b);
  junction_barrier_point a={{-3,1,2}}, c={{5,1,2}}, m={{1,1,2}}, bad={{NAN,0,0}};
  assert(!junction_barrier_tick(&b,2,0,0,0,0,0,3));
  assert(junction_barrier_tick(&b,1,12,13,&a,&c,&m,3));
  assert(b.left.start.v[0]==-3 && b.left.end.v[0]==1 && b.right.start.v[0]==1 && b.right.end.v[0]==5);
  assert(b.left.kind==12 && b.right.kind==13);
  assert(junction_barrier_tick(&b,1,99,99,0,0,0,2));
  assert(b.left.kind==12 && b.right.kind==13 && b.left.display_state==1);
  assert(junction_barrier_tick(&b,2,0,0,0,0,0,2));
  assert(b.left.display_state==2 && b.right.display_state==3 && b.left.kind==12 && b.right.kind==13);
  assert(junction_barrier_tick(&b,3,0,0,0,0,0,1));
  assert(b.left.display_state==3 && b.right.display_state==2 && b.left.kind==12 && b.right.kind==13);
  assert(!junction_barrier_tick(&b,0,0,0,0,0,0,0) && !b.ready);
  assert(!junction_barrier_tick(&b,1,0,0,&bad,&c,&m,1) && !b.ready);
  assert(!junction_barrier_tick(&b,1,0,0,&m,&c,&m,1) && !b.ready);
  assert(!junction_barrier_tick(&b,1,0,0,&a,&m,&m,1) && !b.ready);
  assert(!junction_barrier_tick(&b,1,0,0,&a,&a,&m,1) && !b.ready);
  assert(!junction_barrier_tick(&b,1,0,0,&a,&c,&m,INFINITY) && !b.ready);
  assert(!junction_barrier_tick(0,1,0,0,&a,&c,&m,0));
  assert(junction_barrier_tick(&b,1,0,0,&a,&c,&m,0));
  assert(junction_barrier_tick(&b,2,0,0,0,0,0,0));
  assert(junction_barrier_tick(&b,1,4,5,&b.left.start,&b.right.end,&b.left.end,0));
  assert(b.left.start.v[0]==-3 && b.right.end.v[0]==5 && b.left.kind==4 && b.right.kind==5);
  mat4 vp=mat4_identity(); render_params p={0}; float eye[3]={0,0,0};
  assert(junction_barrier_draw(&b,&vp,eye,0.01f,&p)==2 && submissions==1 && depth_enabled==1);
  assert(strcmp(trace,"STXBD012R")==0);
  assert(captured[0].pos[0]==1 && captured[1].pos[0]==5); /* right is farther */
  assert(fabsf(captured[0].pos[1]-1.85f)<0.0001f && captured[3].pos[1]==1);
  assert(captured[0].uv[0]==0.5f && captured[1].uv[0]==0.75f);
  assert(captured[4].uv[0]==0 && captured[5].uv[0]==0.25f);
  assert(captured[0].uv[1]==1 && captured[3].uv[1]==0);
  assert(junction_barrier_tick(&b,2,0,0,0,0,0,0));
  assert(junction_barrier_draw(&b,&vp,eye,0.01f,&p)==2);
  assert(captured[0].uv[0]==0.75f && captured[4].uv[0]==0);
  int n=submissions; missing=1; assert(!junction_barrier_draw(&b,&vp,eye,1,&p)); missing=0;
  assert(!junction_barrier_draw(&b,&vp,eye,0,&p));
  assert(!junction_barrier_draw(&b,&vp,eye,INFINITY,&p));
  vp.m[2]=NAN; assert(!junction_barrier_draw(&b,&vp,eye,1,&p)); vp.m[2]=0;
  eye[0]=NAN; assert(!junction_barrier_draw(&b,&vp,eye,1,&p)); eye[0]=0;
  b.left.end.v[0]=b.left.start.v[0]; b.left.end.v[2]=b.left.start.v[2];
  assert(!junction_barrier_draw(&b,&vp,eye,1,&p) && submissions==n);
  puts("junction barrier geometry mock passed (no real GL or device)");
  return 0;
}
