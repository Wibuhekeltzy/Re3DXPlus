#!/usr/bin/env python3
"""Usage: python3 test_story_carry_real_hooks.py PHYS_RC [CAR_C].
Read private generated code without publishing it; run a synthetic host test.
Requires Python 3 and CC (default cc). This is not a device test.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

if len(sys.argv) not in (2, 3):
    raise SystemExit(__doc__)
phys = Path(sys.argv[1]).read_text()
car_path = Path(sys.argv[2]) if len(sys.argv) == 3 else Path(__file__).resolve().parents[1] / 'src/car.c'
car = car_path.read_text()
markers = re.findall(r'(?m)^const volatile uint32_t rc_story_carry_bridge\s*=\s*0x53544331u\s*;', phys)
if len(markers) != 1:
    raise SystemExit('expected exactly one strong bridge marker')
hooks = []
for address, callback in [('8997bc8', 'save'), ('8997edc', 'load')]:
    pattern = (r'(?m)^void f_' + address + r'\(void\)\s*\{\s*'
               + r'(rc_story_carry_' + callback + r'\(\);\s*return;)\s*'
               + r'RC_ENTRY\(0x' + address + r'u\);')
    matches = re.findall(pattern, phys)
    if len(matches) != 1:
        raise SystemExit('missing, duplicated or displaced ' + callback + ' hook')
    hooks.append('void f_' + address + '(void) { ' + matches[0] + ' }')
start = car.index('enum { STORY_CARRY_SIZE =')
end = car.index('\nint car_story_carry_capture(', start)
bridge = car[start:end]
pre = r"""
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#define RC_MEM_BASE 0x08000000u
#define RC_MEM_SIZE 0x01500000u
#define RC_IMG_BASE RC_MEM_BASE
#define RC_IMG_END (RC_MEM_BASE + RC_MEM_SIZE)
#define CAR_RIVALS 3
static uint8_t *rc_mem;
static uint32_t esp,eax;
#define VA(a) (rc_mem + ((a) - RC_MEM_BASE))
static uint32_t rd(uint32_t a){uint32_t x;memcpy(&x,VA(a),4);return x;}
static void wr(uint32_t a,uint32_t x){memcpy(VA(a),&x,4);}
#define M32(a) rd(a)
typedef struct { int character,type,level,series; } car_rival;
typedef struct { int type,gears,spokes; float tyres[6]; } car_spec;
static struct { int vs_on,ten_stage,has_rival,n_rival,manual; car_rival rival[3]; car_spec spec; } P;
"""
main = r"""
static void setup_stack(uint32_t obj){esp=0x08001000u;wr(esp+4,obj);eax=0x12345600u;}
int main(void){
 rc_mem=calloc(1,RC_MEM_SIZE);assert(rc_mem);
 const uint32_t src=0x08002000u,dst=0x08003000u,area=0x08010000u,table=0x08011000u;
 wr(STORY_AREA_TABLE,area);wr(area+0x24,table);wr(area+0x28,1);
 wr(src,7);VA(src)[0x18]=1;wr(src+0x20,area);
 story_carry.old_count=1;story_carry.capturing=1;setup_stack(src);
 f_8997bc8();assert(story_carry.valid && story_carry.save_calls==1 && eax==0x12345601u && esp==0x08001004u);
 assert(story_carry.area[0]==0 && carry_word(story_carry.data+0x20)==0);
 story_carry.capturing=0;story_carry.armed=1;story_carry.initializing=1;story_carry.token=42;
 story_carry.target_stage=7;story_carry.target_series=25;P.has_rival=1;P.n_rival=1;P.rival[0].series=25;
 setup_stack(dst);f_8997edc();assert(story_carry.loaded && eax==0x12345601u && esp==0x08001004u);
 assert(M32(dst)==7 && M32(dst+0x20)==area);
 wr(STORY_AREA_TABLE+4,area);memset(VA(dst),0xa5,STORY_CARRY_SIZE);
 setup_stack(dst);f_8997edc();assert(eax==0x12345600u && esp==0x08001004u);
 assert(!story_carry.token && VA(dst)[0]==0xa5 && VA(dst)[STORY_CARRY_SIZE-1]==0xa5);
 free(rc_mem);puts("real hook fragments and production bridge: synthetic checks passed");return 0;
}
"""
with tempfile.TemporaryDirectory(prefix='story-real-hook-') as d:
    c, exe = Path(d) / 'fixture.c', Path(d) / 'fixture'
    c.write_text(pre + '\n' + bridge + '\n' + '\n'.join(hooks) + '\n' + main)
    env = dict(os.environ, TMPDIR=d, TMP=d, TEMP=d, HOME=d, XDG_CACHE_HOME=d)
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + ['-std=c11', '-Wall', '-Wextra', '-Werror', str(c), '-o', str(exe)], check=True, cwd=d, env=env)
    subprocess.run([str(exe)], check=True, cwd=d, env=env)
print('strong marker and exact Save/Load entry hooks verified; no real device or full generated translation executed')
