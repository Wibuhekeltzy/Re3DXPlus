#!/usr/bin/env python3
"""Compile real vkgl context/state/query functions with minimal host types."""
from pathlib import Path
import subprocess
import tempfile

s = (Path(__file__).resolve().parents[1] / 'src/vkgl.c').read_text()
def extract(anchor):
    a = s.index(anchor); b = s.index('{', a); depth = 0
    for i in range(b, len(s)):
        if s[i] == '{': depth += 1
        elif s[i] == '}':
            depth -= 1
            if not depth: return s[a:i+1]
    raise ValueError(anchor)
a = s.index('typedef struct {', s.index("/* ---- a thread's GL state"))
b = s.index('} vctx;', a) + len('} vctx;')
names = ['static void ctx_init(', 'static int *cap_of(', 'void vkgl_glEnable(',
         'void vkgl_glDisable(', 'void vkgl_glDepthFunc(', 'void vkgl_glDepthMask(',
         'void vkgl_glColorMask(', 'void vkgl_glScissor(', 'void vkgl_glViewport(',
         'void vkgl_glGetIntegerv(', 'void vkgl_glGetBooleanv(']
values = {'GL_FALSE':0,'GL_TRUE':1,'GL_ZERO':0,'GL_ONE':1,'GL_NO_ERROR':0,
 'GL_LESS':0x201,'GL_GREATER':0x204,'GL_ALWAYS':0x207,'GL_KEEP':0x1e00,
 'GL_BACK':0x405,'GL_CCW':0x901,'GL_FUNC_ADD':0x8006,'GL_BLEND':0xbe2,
 'GL_DEPTH_TEST':0xb71,'GL_SCISSOR_TEST':0xc11,'GL_CULL_FACE':0xb44,
 'GL_STENCIL_TEST':0xb90,'GL_FRAMEBUFFER_BINDING':0x8ca6,'GL_ACTIVE_TEXTURE':0x84e0,
 'GL_TEXTURE0':0x84c0,'GL_VIEWPORT':0xba2,'GL_SCISSOR_BOX':0xc10,'GL_DEPTH_FUNC':0xb74,
 'GL_MAX_SAMPLES_':0x8d57,'GL_BLEND_SRC_RGB':0x80c9,'GL_BLEND_SRC_ALPHA':0x80cb,
 'GL_BLEND_DST_RGB':0x80c8,'GL_BLEND_DST_ALPHA':0x80ca,'GL_CURRENT_PROGRAM':0x8b8d,
 'GL_ARRAY_BUFFER_BINDING':0x8894,'GL_DEPTH_BITS':0xd56,'GL_STENCIL_BITS':0xd57,
 'GL_RED_BITS':0xd52,'GL_GREEN_BITS':0xd53,'GL_BLUE_BITS':0xd54,'GL_ALPHA_BITS':0xd55,
 'GL_MAX_TEXTURE_SIZE':0xd33,'GL_MAX_TEXTURE_IMAGE_UNITS':0x8872,
 'GL_DEPTH_WRITEMASK':0xb72,'GL_COLOR_WRITEMASK':0xc23}
pre = """#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef unsigned GLenum, GLuint; typedef int GLint, GLsizei; typedef unsigned char GLboolean; typedef float GLfloat;
#define MAX_UNITS 8
#define MAX_ATTR 16
typedef struct { int used; } vvao;
""" + ''.join('#define %s %d\n' % x for x in values.items()) + s[a:b] + """
static vctx main_ctx, loader_ctx; static _Thread_local vctx *cur_tls;
static struct { int max_samples; struct { struct { unsigned maxImageDimension2D; } limits; } props; } D;
""" + extract('static vctx *ctx(void)') + '\n'
test = r"""
static void check(GLenum p, int expected) { GLboolean b=99; vkgl_glGetBooleanv(p,&b); assert(b==expected); }
static void depth(int expected) { GLint v=-1; vkgl_glGetIntegerv(GL_DEPTH_FUNC,&v); assert(v==expected); }
int main(void) {
 ctx_init(&main_ctx,0); ctx_init(&loader_ctx,1);
 check(GL_SCISSOR_TEST,0); check(GL_DEPTH_TEST,0); depth(GL_LESS); check(GL_DEPTH_WRITEMASK,1);
 vkgl_glEnable(GL_SCISSOR_TEST); vkgl_glEnable(GL_DEPTH_TEST); vkgl_glDepthFunc(GL_GREATER);
 check(GL_SCISSOR_TEST,1); check(GL_DEPTH_TEST,1); depth(GL_GREATER);
 vkgl_glDisable(GL_SCISSOR_TEST); check(GL_SCISSOR_TEST,0); check(GL_DEPTH_TEST,1);
 cur_tls=&loader_ctx; check(GL_SCISSOR_TEST,0); check(GL_DEPTH_TEST,0); depth(GL_LESS);
 vkgl_glEnable(GL_SCISSOR_TEST); vkgl_glDisable(GL_DEPTH_TEST); vkgl_glDepthFunc(GL_ALWAYS);
 cur_tls=0; check(GL_SCISSOR_TEST,0); check(GL_DEPTH_TEST,1); depth(GL_GREATER);
 vkgl_glDisable(GL_DEPTH_TEST); check(GL_DEPTH_TEST,0);
 cur_tls=&loader_ctx; check(GL_SCISSOR_TEST,1); check(GL_DEPTH_TEST,0); depth(GL_ALWAYS);
 vkgl_glDisable(GL_SCISSOR_TEST); check(GL_SCISSOR_TEST,0);
 vkgl_glDepthMask(0); check(GL_DEPTH_WRITEMASK,0); vkgl_glDepthMask(4); check(GL_DEPTH_WRITEMASK,1);
 GLboolean m[4]={9,9,9,9}; vkgl_glColorMask(1,0,1,0); vkgl_glGetBooleanv(GL_COLOR_WRITEMASK,m);
 assert(m[0]==1 && m[1]==0 && m[2]==1 && m[3]==0);
 GLint box[4]; vkgl_glScissor(1,2,3,4); vkgl_glGetIntegerv(GL_SCISSOR_BOX,box);
 assert(box[0]==1 && box[1]==2 && box[2]==3 && box[3]==4);
 vkgl_glViewport(5,6,7,8); vkgl_glGetIntegerv(GL_VIEWPORT,box);
 assert(box[0]==5 && box[1]==6 && box[2]==7 && box[3]==8);
 GLint v=7; vkgl_glGetIntegerv(0xdead,&v); assert(v==0); check(0xdead,0); assert(ctx()->err==0);
 puts("vkgl state query host test passed"); return 0;
}
"""
with tempfile.TemporaryDirectory(prefix='vkgl-state-query-') as tmp:
    cfile=Path(tmp)/'test.c'; exe=Path(tmp)/'test'
    cfile.write_text(pre+'\n'+'\n'.join(extract(n) for n in names)+'\n'+test)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(cfile),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
