import pathlib, subprocess, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class JunctionSamplingTest(unittest.TestCase):
 def test_sampling(self):
  text=(ROOT/'src/car.c').read_text(); code=text[text.index('void car_clear_junction('):text.index("/* The time attack's consoles")]
  prefix=r"""#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "car.h"
#define RC_MEM_BASE 0x1000u
#define RC_MEM_SIZE 0x10000u
#define MACHINE_PTR 0x1010u
static unsigned char mem[RC_MEM_SIZE], *rc_mem=mem;
static unsigned char *VA(uint32_t p) { assert(p>=RC_MEM_BASE && p-RC_MEM_BASE<RC_MEM_SIZE); return mem+p-RC_MEM_BASE; }
static uint32_t M32(uint32_t p) { uint32_t v; memcpy(&v,VA(p),4); return v; }
static float MF(uint32_t p) { float v; memcpy(&v,VA(p),4); return v; }
static void W32(uint32_t p,uint32_t v) { memcpy(VA(p),&v,4); }
static void WF(uint32_t p,float v) { memcpy(VA(p),&v,4); }
#define GLOBAL 0x98028f0u
static uint32_t mm(uint32_t p) { return p==GLOBAL?0x3000:M32(p); }
#undef M32
#define M32(p) mm(p)
static struct { int ready; uint32_t jct_select,jct_decide_line; } P;
static int state=0, ret1=1, ret2=1, invalidline=0, calls, left=5;
static float distance=1234;
static void f_8982f4c(void) { }
static void f_8983cd8(void) { }
static uint32_t call(void (*fn)(void),int n,...) { va_list ap; va_start(ap,n); uint32_t a=va_arg(ap,uint32_t); calls++;
 if (fn==f_8982f4c) { assert(n==1 && a==P.jct_select); W32(a,state); W32(a+4,left); W32(a+8,6); WF(a+12,distance); va_end(ap); return ret1; }
 assert(fn==f_8983cd8 && n==3 && a==P.jct_decide_line);
 assert(va_arg(ap,uint32_t)==a+12 && va_arg(ap,uint32_t)==a+24);
 for(int i=0;i<9;i++) WF(a+i*4,invalidline && i==8 ? NAN : (float)i);
 va_end(ap); return ret2;
}
"""
  main=r"""
int main(void) { car_race r={0}; P.ready=1; P.jct_select=0x1800; P.jct_decide_line=0x1900;
 W32(MACHINE_PTR,0x4000); W32(0x3000,0x3500); W32(0x4000+0x938,0x6000); W32(0x6000,0x6500);
 for(state=0;state<4;state++) { read_junction(&r); assert(r.jct_select_valid && r.jct_state==state); assert(r.jct_line_valid && r.jct_line[2][2]==8); }
 state=4; read_junction(&r); assert(!r.jct_select_valid && r.jct_line_valid);
 state=1; distance=NAN; read_junction(&r); assert(!r.jct_select_valid); distance=INFINITY; read_junction(&r); assert(!r.jct_select_valid);
 distance=1234; left=75; read_junction(&r); assert(!r.jct_select_valid); left=5;
 ret1=0; ret2=0; read_junction(&r); assert(!r.jct_select_valid && !r.jct_line_valid && r.jct_line[2][2]==0);
 ret1=ret2=1; invalidline=1; read_junction(&r); assert(r.jct_select_valid && !r.jct_line_valid);
 invalidline=0; P.jct_select=0x10fffu; calls=0; read_junction(&r); assert(!r.jct_select_valid && !r.jct_line_valid && !calls);
 P.jct_select=0x1800; W32(MACHINE_PTR,0); read_junction(&r); assert(!r.jct_select_valid);
 car_clear_junction(&r); assert(!r.jct_line_valid); return 0; }
"""
  with tempfile.TemporaryDirectory() as d:
   src=pathlib.Path(d)/'junction.c'; exe=pathlib.Path(d)/'junction'; src.write_text(prefix+code+main)
   subprocess.run(['cc','-std=c99','-I',str(ROOT/'src'),str(src),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True)
if __name__=='__main__': unittest.main()
