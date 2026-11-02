#!/usr/bin/env python3
"""Extract v5c vkgl functions and prove epoch invariants under CPU-only mocks."""
from pathlib import Path
import re,hashlib,subprocess,os
import tempfile
root=Path(__file__).resolve().parents[1]
assert os.environ.get('TMPDIR'), 'TMPDIR must be explicitly set'
p=Path(tempfile.mkdtemp(prefix='vkgl-epoch-',dir=os.environ['TMPDIR']))
s=(root/'src/vkgl.c').read_text()
assert re.search(r'^#define MAX_UNITS 8$',s,re.M)
def fun(src,anchor):
 a=src.index(anchor); k=src.index('{',a); d=0
 for i in range(k,len(src)):
  if src[i]=='{':d+=1
  elif src[i]=='}':
   d-=1
   if not d:return src[a:i+1]
 raise ValueError(anchor)
fns=[fun(s,x) for x in ['static vctx *ctx(void)','static void set_err(', 'void *vkgl_preview_save(', 'void vkgl_preview_restore(', 'int vkgl_error_pending(', 'uint64_t vkgl_error_epoch(', 'GLenum vkgl_glGetError(', 'void vkgl_glActiveTexture(']]
pre=r'''#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef unsigned GLenum;typedef unsigned GLuint;
#define GL_NO_ERROR 0
#define GL_TEXTURE0 0x84C0
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_OPERATION 0x0502
#define MAX_UNITS 8
#define MAX_SAMP 8
#define UINT64_MAX_TEST UINT64_MAX
typedef struct {int datum;} vvao;
typedef struct {int loader;GLenum err;int unit;vvao vao0;} vctx;
typedef struct {unsigned char *shadow;int block,samp_unit[MAX_SAMP],dirty;} vprog;
static uint64_t main_error_epoch,loader_error_epoch;
static vctx main_ctx,loader_ctx;static _Thread_local vctx *cur_tls;
static unsigned char shadowbytes[4];static vprog prog={shadowbytes,4,{0},0};
static vprog *prog_get(GLuint id){return id==1?&prog:NULL;}
static vvao *cur_vao(vctx *v){return &v->vao0;}
typedef struct {vctx state;vvao vao;GLuint program;unsigned char *shadow;int size;int samplers[MAX_SAMP];} vkgl_preview_snapshot;
'''
test=r'''
int main(void){
 assert(vkgl_error_epoch()==0 && !vkgl_error_pending());
 vkgl_glActiveTexture(GL_TEXTURE0+7);assert(main_ctx.unit==7 && vkgl_error_epoch()==0);
 vkgl_glActiveTexture(GL_TEXTURE0+8);assert(main_ctx.unit==7 && vkgl_error_pending() && vkgl_error_epoch()==1);
 vkgl_glActiveTexture(GL_TEXTURE0+8);assert(main_ctx.unit==7 && vkgl_error_epoch()==2 && vkgl_glGetError()==GL_INVALID_ENUM);
 assert(!vkgl_error_pending() && vkgl_error_epoch()==2);
 void *snapshot=vkgl_preview_save(1);assert(snapshot);set_err(GL_INVALID_OPERATION);assert(vkgl_error_epoch()==3);vkgl_preview_restore(snapshot);
 assert(!vkgl_error_pending() && vkgl_error_epoch()==3);puts("two errors while sticky + preview restore cannot roll epoch back");
 loader_ctx.loader=1;cur_tls=&loader_ctx;set_err(GL_INVALID_OPERATION);assert(vkgl_error_epoch()==1);assert(vkgl_glGetError()==GL_INVALID_OPERATION);
 cur_tls=NULL;assert(vkgl_error_epoch()==3 && !vkgl_error_pending());puts("main and loader error epochs isolated");
 __atomic_store_n(&main_error_epoch,UINT64_MAX-1,__ATOMIC_RELAXED);set_err(GL_INVALID_ENUM);assert(vkgl_error_epoch()==UINT64_MAX);vkgl_glGetError();set_err(GL_INVALID_ENUM);assert(vkgl_error_epoch()==UINT64_MAX);puts("saturation remains UINT64_MAX; legal unit7/illegal8 verified");
 __atomic_store_n(&main_error_epoch,0,__ATOMIC_RELAXED);__atomic_store_n(&loader_error_epoch,0,__ATOMIC_RELAXED);
 memset(&main_ctx,0,sizeof main_ctx);memset(&loader_ctx,0,sizeof loader_ctx);cur_tls=NULL;main_ctx.loader=1;set_err(GL_INVALID_OPERATION);assert(vkgl_error_epoch()==1 && main_error_epoch==1 && loader_error_epoch==0);
 void *cross=vkgl_preview_save(1);assert(cross);cur_tls=&loader_ctx;loader_ctx.loader=0;set_err(GL_INVALID_OPERATION);assert(vkgl_error_epoch()==1 && main_error_epoch==1);vkgl_preview_restore(cross);assert(loader_ctx.loader==1);set_err(GL_INVALID_OPERATION);assert(vkgl_error_epoch()==2 && main_error_epoch==1);puts("bucket follows pointer despite swapped loader flags and cross-context restore");
 vctx other={0};cur_tls=&other;set_err(GL_INVALID_ENUM);assert(other.err==GL_INVALID_ENUM && vkgl_error_epoch()==UINT64_MAX && main_error_epoch==1 && loader_error_epoch==2);puts("unknown context fails closed, without incrementing known buckets");
 return 0;
}
'''
src=pre+'\n'+'\n'.join(fns)+'\n'+test
out=p/'candidate_v5f_epoch_fixture.c';exe=p/'candidate_v5f_epoch_fixture';log=p/'candidate_v5f_epoch_compile.log'
with out.open('x') as f:f.write(src)
env=dict(os.environ,HOME=str(p),TMPDIR=str(p),TMP=str(p),TEMP=str(p),XDG_CACHE_HOME=str(p),PYTHONDONTWRITEBYTECODE='1')
with log.open('x') as f:subprocess.run(['/usr/bin/cc','-std=c11','-Wall','-Werror=return-type',str(out),'-o',str(exe)],stdout=f,stderr=subprocess.STDOUT,env=env,check=True)
print('fixture sha256',hashlib.sha256(src.encode()).hexdigest(),flush=True)
subprocess.run([str(exe)],env=env,check=True)
