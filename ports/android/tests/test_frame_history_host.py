#!/usr/bin/env python3
"""Real vk_single_targets and outer begin; mocked allocation and extracted end guard."""
from pathlib import Path
import hashlib,os,subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
assert os.environ.get('TMPDIR'), 'TMPDIR must be explicitly set'
p=Path(tempfile.mkdtemp(prefix='frame-history-',dir=os.environ['TMPDIR']))
r=(root/'src/render.c').read_text();v=(root/'src/vkgl.c').read_text()
assert '#define MAX_UNITS 8' in v
def fun(s,anchor):
 a=s.index(anchor);k=s.index('{',a);d=0
 for i in range(k,len(s)):
  if s[i]=='{':d+=1
  elif s[i]=='}':
   d-=1
   if d==0:return s[a:i+1]
 raise ValueError(anchor)
pre=r'''#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
typedef unsigned GLuint; typedef unsigned GLenum; typedef int GLint;
#define WMMT_VK 1
#define GL_NO_ERROR 0
#define GL_OUT_OF_MEMORY 0x505
#define GL_FRAMEBUFFER_BINDING 1
#define GL_TEXTURE0 2
#define GL_FRAMEBUFFER 3
#define GL_DEPTH_ATTACHMENT 4
#define GL_STENCIL_ATTACHMENT 5
#define GL_RENDERBUFFER 6
#define GL_FRAMEBUFFER_COMPLETE 7
#define GL_COLOR_ATTACHMENT0 8
#define GL_TEXTURE_2D 9
#define GL_DEPTH_STENCIL_ATTACHMENT_ 10
typedef struct {int loader;GLenum err;} vctx;
static vctx main_ctx,loader_ctx;static uint64_t main_error_epoch,loader_error_epoch;static vctx *active=&main_ctx;
static vctx *ctx(void){return active;}
static struct {int on,single_attempted,msrt,single_ready,single_frame,ssao_ok; GLuint z_tex,rt_tex,rt_fbo,single_fbo[2],ao_fbo[2],ao_tex[2];} hq;
static struct {GLuint depth,tex;} blur;
static uint64_t frame_start_epoch,frame_history_epoch;static int frame_recording_trusted,frame_history_valid;
static int fault_during_alloc,fail_names,recurse_once;
'''
err='\n'.join(fun(v,a) for a in ('static void set_err(', 'int vkgl_error_pending(', 'uint64_t vkgl_error_epoch(', 'GLenum vkgl_glGetError('))
mid=r'''
#define glGetError vkgl_glGetError
static int vkgl_ssao_depth_supported(int w,int h){(void)w;(void)h;return 1;}
static void glGetIntegerv(int a,GLint*b){(void)a;*b=1;}
static void glActiveTexture(int a){(void)a;}
static GLuint z_texture(int w,int h){(void)w;(void)h;if(fault_during_alloc){fault_during_alloc=0;set_err(GL_OUT_OF_MEMORY);}return 8;}
static GLuint hq_texture(int w,int h){(void)w;(void)h;return 9;}
static GLuint hq_fbo(GLuint t){(void)t;return 10;}
static void glFramebufferRenderbuffer(int a,int b,int c,int d){(void)a;(void)b;(void)c;(void)d;}
static int glCheckFramebufferStatus(int a){(void)a;return GL_FRAMEBUFFER_COMPLETE;}
static void glGenFramebuffers(int n,GLuint*f){for(int i=0;i<n;i++)f[i]=fail_names?0:20+i;}
static void glBindFramebuffer(int a,GLuint b){(void)a;(void)b;}
static void glFramebufferTexture2D(int a,int b,int c,GLuint d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;}
static void ssao_targets(int w,int h){(void)w;(void)h;hq.ssao_ok=1;}
static void glDeleteFramebuffers(int n,const GLuint*f){(void)n;(void)f;}
static void glDeleteTextures(int n,const GLuint*f){(void)n;(void)f;}
static void glBindTexture(int a,GLuint b){(void)a;(void)b;}
'''
single=fun(r,'static void vk_single_targets(int w, int h)')
wrapper=fun(r,'int render_blur_begin(float amount, int w, int h)')
assert 'return render_blur_begin_inner(amount, blur.out_w, blur.out_h);' in r
inner=r'''
static int render_blur_begin_inner(float amount,int w,int h){
 (void)amount;vk_single_targets(w,h);
 if(recurse_once){recurse_once=0;return render_blur_begin_inner(amount,w,h);}
 /* Exact sampling guard from the production inner function. */
 if (vkgl_error_pending() || vkgl_error_epoch() == UINT64_MAX || vkgl_error_epoch() != frame_start_epoch)
   frame_recording_trusted = frame_history_valid = 0;
 return 1;
}
'''
assert 'frame_recording_trusted = frame_history_valid = 0;' in r
end=r'''
static void end_guard(void){
 int history_ok=1;
 if (!frame_recording_trusted || vkgl_error_pending() || vkgl_error_epoch() == UINT64_MAX ||
     vkgl_error_epoch() != frame_start_epoch) history_ok = 0;
 if (vkgl_error_pending() || vkgl_error_epoch() == UINT64_MAX || vkgl_error_epoch() != frame_start_epoch)
   history_ok = 0;
 frame_history_valid = hq.on && history_ok;
 if (frame_history_valid) frame_history_epoch = vkgl_error_epoch();
}
'''
assert 'frame_history_valid = hq.on && history_ok;' in r and 'if (frame_history_valid) frame_history_epoch = vkgl_error_epoch();' in r
test=r'''
static void reset(void){memset(&hq,0,sizeof hq);memset(&blur,0,sizeof blur);memset(&main_ctx,0,sizeof main_ctx);memset(&loader_ctx,0,sizeof loader_ctx);main_error_epoch=loader_error_epoch=0;active=&main_ctx;frame_history_valid=frame_recording_trusted=0;frame_start_epoch=frame_history_epoch=0;fault_during_alloc=fail_names=recurse_once=0;hq.on=1;hq.msrt=1;hq.rt_tex=11;hq.rt_fbo=12;blur.tex=13;}
int main(void){
 reset();set_err(GL_OUT_OF_MEMORY);assert(render_blur_begin(0,1920,1080));assert(!vkgl_error_pending()&&hq.single_ready&&!frame_recording_trusted);end_guard();assert(!frame_history_valid);puts("entry sticky consumed by real vk_single_targets: invalid");
 reset();fault_during_alloc=1;assert(render_blur_begin(0,1920,1080));assert(!vkgl_error_pending()&&!hq.single_ready&&!frame_recording_trusted);end_guard();assert(!frame_history_valid);puts("mid-allocation error consumed by real vk_single_targets: invalid");
 reset();fault_during_alloc=1;recurse_once=1;assert(render_blur_begin(0,1920,1080));assert(frame_start_epoch==0 && vkgl_error_epoch()==1);end_guard();assert(!frame_history_valid);puts("recursive inner retry preserves first outer epoch");
 reset();assert(render_blur_begin(0,1920,1080));end_guard();assert(frame_history_valid);set_err(GL_OUT_OF_MEMORY);assert(glGetError()==GL_OUT_OF_MEMORY);assert(render_blur_begin(0,1920,1080));assert(!frame_history_valid&&frame_recording_trusted);end_guard();assert(frame_history_valid);puts("consumed inter-frame error drops old history for one frame");
 reset();__atomic_store_n(&main_error_epoch,UINT64_MAX,__ATOMIC_RELAXED);assert(render_blur_begin(0,1920,1080));end_guard();assert(!frame_history_valid);puts("saturated epoch is permanently untrusted");
 reset();fail_names=1;assert(render_blur_begin(0,1920,1080));assert(!hq.single_ready);puts("real single target rejects zero framebuffer names");
 return 0;
}
'''
src=pre+'\n'+err+'\n'+mid+'\n'+single+'\n'+inner+'\n'+wrapper+'\n'+end+'\n'+test
out=p/'candidate_v5e_single_fixture.c';exe=p/'candidate_v5e_single_fixture';log=p/'candidate_v5e_single_compile.log'
with out.open('x') as f:f.write(src)
env=dict(os.environ,HOME=str(p),TMPDIR=str(p),TMP=str(p),TEMP=str(p),XDG_CACHE_HOME=str(p),PYTHONDONTWRITEBYTECODE='1')
with log.open('x') as f:subprocess.run(['/usr/bin/cc','-std=c11','-Wall','-Werror=return-type',str(out),'-o',str(exe)],stdout=f,stderr=subprocess.STDOUT,env=env,check=True)
print('fixture sha256',hashlib.sha256(src.encode()).hexdigest(),flush=True)
subprocess.run([str(exe)],env=env,check=True)
