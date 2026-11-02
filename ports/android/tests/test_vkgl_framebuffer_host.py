#!/usr/bin/env python3
"""Test production Vulkan FBO completeness with host-side attachment mocks.
This is not real GLES/Vulkan or GPU validation.
"""
from pathlib import Path
import hashlib, subprocess, tempfile
D=Path(tempfile.mkdtemp(prefix='hiace-fbo-test.'))
p=Path(__file__).resolve().parents[1]/'src'/'vkgl.c'
s=p.read_text()
a=s.index('GLenum vkgl_glCheckFramebufferStatus(GLenum target) {'); b=s.index('\n}',a)+2
original=s[a:b]
if 'has_stencil(s->fmt)' not in original:
    raise RuntimeError('stencil attachment format guard missing')
pre=r'''
#include <stdio.h>
typedef unsigned int GLenum;
#define GL_FRAMEBUFFER_COMPLETE 0x8cd5
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8cd7
#define GL_FRAMEBUFFER_UNSUPPORTED 0x8cdd
#define COLOR 1
#define DEPTH16 2
#define DEPTH24_STENCIL8 3
typedef struct {int fmt, samples; void *att;} vimg;
typedef struct {int kind, samples; vimg *image;} vatt;
typedef struct {vatt color, depth, stencil;} vfbo;
static vfbo F; static int use_default;
static vfbo *bound_fbo(GLenum target){(void)target; return use_default ? 0 : &F;}
static vimg *att_image(const vatt *a){return a->image;}
static int is_depth(int f){return f==DEPTH16 || f==DEPTH24_STENCIL8;}
static int has_stencil(int f){return f==DEPTH24_STENCIL8;}
'''
post=r'''
#define CHECK(v) do {if(!(v)){printf("FAILED line %d\n",__LINE__);return 1;}}while(0)
int main(void){
 int valid=1; vimg c={COLOR,1,&valid}, d16={DEPTH16,1,&valid}, ds={DEPTH24_STENCIL8,1,&valid}, ds2={DEPTH24_STENCIL8,1,&valid}, ms={DEPTH24_STENCIL8,4,&valid};
 use_default=1; CHECK(vkgl_glCheckFramebufferStatus(0)==GL_FRAMEBUFFER_COMPLETE);use_default=0;
 F=(vfbo){0}; CHECK(vkgl_glCheckFramebufferStatus(0)==GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
 F=(vfbo){.color={1,0,&c}}; CHECK(vkgl_glCheckFramebufferStatus(0)==GL_FRAMEBUFFER_COMPLETE);
 F=(vfbo){.depth={1,0,&d16}}; CHECK(vkgl_glCheckFramebufferStatus(0)==GL_FRAMEBUFFER_COMPLETE);
 F=(vfbo){.depth={1,0,&ds}}; CHECK(vkgl_glCheckFramebufferStatus(0)==GL_FRAMEBUFFER_COMPLETE);
 F=(vfbo){.color={1,0,&c},.depth={1,0,&ds},.stencil={1,0,&ds}}; CHECK(vkgl_glCheckFramebufferStatus(0)==GL_FRAMEBUFFER_COMPLETE);
 F=(vfbo){.color={1,0,&c},.depth={1,0,&d16},.stencil={1,0,&d16}};
 CHECK(vkgl_glCheckFramebufferStatus(0)==GL_FRAMEBUFFER_UNSUPPORTED);
 F=(vfbo){.color={1,0,&c},.depth={1,0,&ds},.stencil={1,0,&ds2}}; CHECK(vkgl_glCheckFramebufferStatus(0)==GL_FRAMEBUFFER_UNSUPPORTED);
 F=(vfbo){.color={1,0,&c},.depth={1,0,&ds},.stencil={1,0,0}}; CHECK(vkgl_glCheckFramebufferStatus(0)==GL_FRAMEBUFFER_UNSUPPORTED);
 F=(vfbo){.color={1,0,&c},.stencil={1,0,&ds}}; CHECK(vkgl_glCheckFramebufferStatus(0)==GL_FRAMEBUFFER_UNSUPPORTED);
 F=(vfbo){.color={1,0,&c},.depth={1,0,0}}; CHECK(vkgl_glCheckFramebufferStatus(0)==GL_FRAMEBUFFER_UNSUPPORTED);
 F=(vfbo){.color={1,0,&c},.depth={1,0,&ms}}; CHECK(vkgl_glCheckFramebufferStatus(0)==0x8d56);
 puts("PASS: default, empty, color-only, D16-only, D24S8, D16-shared, different-stencil, stencil-only, missing and multisample");
 return 0;
}
'''
env={'PATH':'/usr/bin:/bin','TMPDIR':str(D),'TMP':str(D),'TEMP':str(D),'HOME':str(D),'XDG_CACHE_HOME':str(D),'LC_ALL':'C'}
path=D/'production.c'; path.write_text(pre+original+post)
cmd=['/usr/bin/cc','-std=c11','-Wall','-Wextra','-Werror',str(path),'-o',str(D/'production')]
subprocess.run(cmd,check=True,env=env,cwd=D)
subprocess.run([str(D/'production')],check=True,env=env,cwd=D)
print('source_sha256',hashlib.sha256(s.encode()).hexdigest())
print('output_directory',D)
