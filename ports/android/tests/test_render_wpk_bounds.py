#!/usr/bin/env python3
"""Execute production renderer functions with offline file/GL stubs."""
import pathlib, re, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
src=(root/'src/render.c').read_text()
def function(name):
 m=re.search(r'(?m)^(?:static\s+)?(?:int|void)\s+'+name+r'\s*\([^;]*?\)\s*\{',src)
 if not m: raise AssertionError(name)
 i,depth=m.end(),1
 while depth:
  depth+=(src[i]=='{')-(src[i]=='}'); i+=1
 return src[m.start():i]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "render.h"
typedef unsigned GLuint; typedef int GLint,GLsizei; typedef long GLintptr,GLsizeiptr;
#define GL_ARRAY_BUFFER 1
#define GL_ELEMENT_ARRAY_BUFFER 2
#define GL_STATIC_DRAW 3
#define GL_TRIANGLES 4
#define GL_UNSIGNED_SHORT 5
#define GL_FALSE 0
#define POOL_VERTS 65536u
typedef struct {uint16_t tex,flags;uint32_t vtx_off,vtx_count,idx_off,idx_count;} wpk_batch_rec;
static uint16_t input[4],sent[4];static int uploads,draws,binds,stat_error,scratch_calls,big_mode,read_calls,fail_after;static long fake_size=1000;
static int fake_fstat(int fd,struct stat*st){(void)fd;if(stat_error)return -1;memset(st,0,sizeof *st);st->st_size=fake_size;return 0;}
#define fstat fake_fstat
static int read_at(int fd,void *dst,size_t n,long off){(void)fd;if(big_mode && off>=2){if(++read_calls==fail_after)return -1;memset(dst,0,n);return 0;}if(off==2){if(n>sizeof input)return -1;memcpy(dst,input,n);}else memset(dst,0,n);return 0;}
static void *scratch(wpk_scene*s,size_t n){scratch_calls++;if(n>s->scratch_size){free(s->scratch);s->scratch=malloc(n);s->scratch_size=n;}return s->scratch;}
static size_t cube_upload(wpk_scene*s,uint32_t i){(void)s;(void)i;return 1;}
static size_t tex_upload(wpk_scene*s,uint32_t i){(void)s;(void)i;return 1;}
static size_t ao_upload(wpk_scene*s){(void)s;return 1;}
static void file_close(wpk_scene*s){(void)s;}
static void glGenBuffers(int n,uint32_t*p){(void)n;*p=1;}
static void glBindBuffer(int a,uint32_t b){(void)a;(void)b;}
static void glBufferData(int a,GLsizeiptr b,const void*c,int d){(void)a;(void)b;(void)c;(void)d;}
static void glBufferSubData(int a,GLintptr b,GLsizeiptr n,const void*p){(void)b;uploads++;if(a==GL_ELEMENT_ARRAY_BUFFER && n<=(GLsizeiptr)sizeof sent)memcpy(sent,p,n);}
static void glUniformMatrix4fv(GLint a,int b,int c,const float*d){(void)a;(void)b;(void)c;(void)d;}
static int box_outside(const mat4*m,const float*a,const float*b){(void)m;(void)a;(void)b;return 0;}
static void bind_batch(const wpk_scene*s,const wpk_batch*b,GLuint*p){wpk_pool*pl=&s->pools[b->pool];binds++;if(!pl->vao)pl->vao=pl->vbo?2:99;*p=pl->vao;}
static void glDrawElements(int a,GLsizei b,int c,const void*d){(void)a;(void)b;(void)c;(void)d;draws++;}
static GLint shadow_mvp;
'''
body=r'''
int main(void){
 wpk_chunk c={0};c.group=WPK_SHADOW;c.first=1;c.n=0;
 wpk_scene s={0};wpk_batch b[1]={{0}};wpk_pool ps={.vbo=1};mat4 m={0};b[0].count=3;s.chunks=&c;s.n_chunk=1;s.n_batch=1;s.batches=b;s.pools=&ps;
 draw_volume(&s,&m);assert(!draws&&!binds);c.n=1;draw_volume(&s,&m);assert(!draws&&!binds);
 c.first=UINT32_MAX;c.n=2;draw_volume(&s,&m);assert(!draws&&!binds);c.first=0;c.n=1;draw_volume(&s,&m);assert(draws==1&&binds==1);
 uint16_t i=65535;assert(wpk_indices_valid(&i,1,65536,0));i=0;assert(wpk_indices_valid(&i,1,1,65535));i=1;assert(!wpk_indices_valid(&i,1,1,65535));
 assert(!wpk_indices_valid(&i,1,1,0));assert(!wpk_indices_valid(&i,1,65537,0));assert(!wpk_indices_valid(&i,1,1,UINT32_MAX));
 assert(wpk_index_buffer_valid(2,1));assert(!wpk_index_buffer_valid(UINT32_MAX-1,1));assert(!wpk_index_buffer_valid(0,UINT32_MAX));
 assert(wpk_range_valid(10,10,0));assert(!wpk_range_valid(10,11,0));assert(wpk_range_valid(10,2,8));assert(!wpk_range_valid(10,2,9));assert(!wpk_range_valid(UINT64_MAX,UINT64_MAX-1,2));
 wpk_batch_rec r={.vtx_off=1,.vtx_count=1,.idx_off=2,.idx_count=1};wpk_pool pool={0};wpk_batch batch={0};wpk_scene q={0};
 pool.vfill=65535*24;pool.vbytes=65536*24;pool.ibytes=2;
 q.fd=1;q.stride=24;q.rec_size=sizeof r;q.n_batch=1;q.batch_tbl=malloc(sizeof r);memcpy(q.batch_tbl,&r,sizeof r);q.batches=&batch;q.pools=&pool;
 input[0]=1;uploads=0;assert(wpk_upload(&q,100));assert(batch.count==0&&uploads==0&&pool.vfill==65535*24);q.chunks=&c;q.n_chunk=1;draw_volume(&q,&m);assert(pool.vao==0);free(q.scratch);
 memset(&q,0,sizeof q);memset(&batch,0,sizeof batch);q.fd=1;q.stride=24;q.rec_size=sizeof r;q.n_batch=1;q.batch_tbl=malloc(sizeof r);memcpy(q.batch_tbl,&r,sizeof r);q.batches=&batch;q.pools=&pool;
 input[0]=0;uploads=0;assert(wpk_upload(&q,100));assert(batch.count==1&&sent[0]==65535&&uploads==2);q.chunks=&c;q.n_chunk=1;draw_volume(&q,&m);assert(pool.vao==2);free(q.scratch);
 for(int z=0;z<3;z++) { memset(&q,0,sizeof q);memset(&batch,0,sizeof batch);pool.vfill=0;q.fd=1;q.stride=24;q.rec_size=sizeof r;q.n_batch=1;q.batch_tbl=malloc(sizeof r);memcpy(q.batch_tbl,&r,sizeof r);q.batches=&batch;q.pools=&pool;
   fake_size=z==0?2:1000;stat_error=z==1;if(z==2){((wpk_batch_rec*)q.batch_tbl)->idx_off=999;}
   scratch_calls=uploads=0;assert(wpk_upload(&q,100));assert(!batch.count&&!scratch_calls&&!uploads); }
 memset(&q,0,sizeof q);memset(&batch,0,sizeof batch);memset(&pool,0,sizeof pool);r.idx_count=32769;pool.vbytes=24;pool.ibytes=r.idx_count*2;q.fd=1;q.stride=24;q.rec_size=sizeof r;q.n_batch=1;q.batch_tbl=malloc(sizeof r);memcpy(q.batch_tbl,&r,sizeof r);q.batches=&batch;q.pools=&pool;
 big_mode=1;stat_error=0;fake_size=100000;uploads=0;assert(wpk_upload(&q,100));assert(batch.count==32769&&uploads==3&&q.scratch_size==24+65536);free(q.scratch);
 memset(&q,0,sizeof q);memset(&batch,0,sizeof batch);memset(&pool,0,sizeof pool);pool.vbytes=24;pool.ibytes=r.idx_count*2;q.fd=1;q.stride=24;q.rec_size=sizeof r;q.n_batch=1;q.batch_tbl=malloc(sizeof r);memcpy(q.batch_tbl,&r,sizeof r);q.batches=&batch;q.pools=&pool;
 read_calls=0;fail_after=4;uploads=0;assert(wpk_upload(&q,100));assert(batch.count==0&&pool.ifill==0&&uploads==2);free(q.scratch);
 puts("renderer WPK bounds: PASS");return 0;
}
'''
code=prefix+'\n'+'\n'.join(function(x) for x in ('wpk_chunk_valid','wpk_range_valid','wpk_index_buffer_valid','wpk_indices_valid','wpk_upload','draw_volume'))+'\n'+body
with tempfile.TemporaryDirectory() as d:
 c=pathlib.Path(d)/'test.c';exe=pathlib.Path(d)/'test';c.write_text(code)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'src'),str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)

over_prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include "render.h"
#undef __APPLE__
typedef unsigned GLuint;typedef int GLint,GLsizei;
#define OVER_PITCH 0.61f
#define OVER_ROOF 14.0f
#define OVER_REACH 500.0f
#define OVER_NEAR 4.0f
#define OVER_TAN 1.73f
#define OVER_SIZE 256
#define OVER_UNIT 3
#define GL_FRAMEBUFFER_BINDING 1
#define GL_VIEWPORT 2
#define GL_FRAMEBUFFER 3
#define GL_TRUE 1
#define GL_FALSE 0
#define GL_COLOR_BUFFER_BIT 1
#define GL_DEPTH_BUFFER_BIT 2
#define GL_TEXTURE0 10
#define GL_DEPTH_TEST 11
#define GL_LEQUAL 12
#define GL_CULL_FACE 13
#define GL_BLEND 14
#define GL_TEXTURE_2D 15
#define GL_TRIANGLES 16
#define GL_UNSIGNED_SHORT 17
#define GL_SCISSOR_TEST 18
static struct {GLuint prog,fbo,tex; GLint mvp,cut;int state;} over={.state=1};
static GLuint white_tex=1;static int calls;
static int over_init(void){return 0;}
static void glGetIntegerv(int x,int*p){p[0]=0;if(x==GL_VIEWPORT)p[1]=p[2]=p[3]=0;}
static void glBindFramebuffer(int a,unsigned b){(void)a;(void)b;}
static void glViewport(int a,int b,int c,int d){(void)a;(void)b;(void)c;(void)d;}
static void glClearColor(float a,float b,float c,float d){(void)a;(void)b;(void)c;(void)d;}
static void glDepthMask(int a){(void)a;}
static void glClear(int a){(void)a;}
static void glUseProgram(unsigned a){(void)a;}
static void glUniformMatrix4fv(int a,int b,int c,const float*d){(void)a;(void)b;(void)c;(void)d;}
static void glActiveTexture(int a){(void)a;}
static void glEnable(int a){(void)a;}
static void glDisable(int a){(void)a;}
static void glDepthFunc(int a){(void)a;}
static void glUniform1f(int a,float b){(void)a;(void)b;}
static void glBindTexture(int a,unsigned b){(void)a;(void)b;}
static void glBindVertexArrayOES(unsigned a){(void)a;}
static void bind_batch(const wpk_scene*s,const wpk_batch*b,GLuint*p){(void)s;(void)b;*p=1;}
static void glDrawElements(int a,int b,int c,const void*d){(void)a;(void)b;(void)c;(void)d;calls++;}
static int box_outside(const mat4*a,const float*b,const float*c){(void)a;(void)b;(void)c;return 0;}
static float box_distance(const float*a,const float*b,const float*c){(void)a;(void)b;(void)c;return 0;}
static void glScissor(int a,int b,int c,int d){(void)a;(void)b;(void)c;(void)d;}
static void warm_draw(const wpk_scene*s,const wpk_batch*b,uint32_t t,uint32_t i){(void)t;(void)i;assert(b->count);assert(b>=s->batches && b<s->batches+s->n_batch);calls++;}
'''
over_body=r'''
int main(void){
 wpk_scene s={0};wpk_chunk ch={0};wpk_batch b[1]={{0}};render_params p={0};mat4 car=mat4_identity();
 s.n_chunk=1;s.n_batch=1;s.chunks=&ch;s.batches=b;ch.hi[1]=100;b[0].count=3;b[0].hi[1]=100;b[0].tex=0xffff;
 const wpk_scene *sc[]={&s};
 ch.first=1;ch.n=0;assert(render_overhead(sc,1,&car,&p)==0&&!calls);
 ch.n=1;assert(render_overhead(sc,1,&car,&p)==0&&!calls);
 ch.first=UINT32_MAX;ch.n=2;assert(render_overhead(sc,1,&car,&p)==0&&!calls);
 ch.first=0;ch.n=1;assert(render_overhead(sc,1,&car,&p)==1&&calls==1);
 unsigned char warmed=0;s.warm=&warmed;s.warm_chunk=-1;
 ch.first=UINT32_MAX;ch.n=2;assert(warm_batches(&s,NULL,0,1,NULL)==0&&warmed&&calls==1);
 warmed=0;ch.first=1;ch.n=0;assert(warm_batches(&s,NULL,0,1,NULL)==0&&warmed&&calls==1);
 warmed=0;ch.first=0;ch.n=1;assert(warm_batches(&s,NULL,0,1,NULL)==1&&calls==2);
 wpk_batch mix[4]={{0}};mix[1].count=3;mix[3].count=6;mix[1].tex=mix[3].tex=0xffff;
 s.batches=mix;s.n_batch=4;s.next_batch=4;s.warm_chunk=-1;warmed=0;ch.first=0;ch.n=4;
 assert(wpk_cold_near(&s,NULL,0,NULL)==1);
 assert(warm_batches(&s,NULL,0,0,NULL)==0 && !warmed && calls==2);
 assert(warm_batches(&s,NULL,0,1,NULL)==1 && !warmed && calls==3 && s.warm_batch==2);
 assert(wpk_cold_near(&s,NULL,0,NULL)==1);
 assert(warm_batches(&s,NULL,0,1,NULL)==1 && warmed && calls==4);
 assert(wpk_cold_near(&s,NULL,0,NULL)==0 && warm_batches(&s,NULL,0,2,NULL)==0);
 warmed=0;s.warm_chunk=-1;ch.first=UINT32_MAX;ch.n=2;
 assert(wpk_cold_near(&s,NULL,0,NULL)==0);
 puts("renderer overhead and warm bounds: PASS");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 c=pathlib.Path(d)/'over.c';exe=pathlib.Path(d)/'over'
 c.write_text(over_prefix+'\n'+function('wpk_chunk_valid')+'\n'+function('chunk_in_reach')+'\n'+function('warm_batches')+'\n'+function('wpk_cold_near')+'\n'+function('render_overhead')+'\n'+over_body)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'src'),str(c),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
