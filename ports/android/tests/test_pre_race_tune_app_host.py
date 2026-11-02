#!/usr/bin/env python3
"""Test extracted race tuning and HUD/audio call sites, not full menu_frame."""
from pathlib import Path
import os, subprocess, tempfile
SRC=Path(__file__).resolve().parents[1]/"src"
s=(SRC/"app.c").read_text()
start=s.index("    const cardb_color *col = chosen_color();",s.index("  if (go) {"))
end=s.index("    /* a race chosen before the course finished loading",start)
select=s[start:end]
assert "garage_save" not in select
assert "car_start(&G.car, course, G.menu.manual, &spec)" in s[end:]
a=s.index("      snd_set_car((unsigned)col->type, power);",end)
b=s.index("\n",s.index("      hud_set_tune(",a))
audio=s[a:b]
pre=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "car.h"
#include "menu.h"
#include "pre_race_tune.h"
#define __ANDROID__ 1
typedef struct { int type,gears,pack; } cardb_color;
static const cardb_color color = { 13, 6, 0 };
static const unsigned char CARDB_TYRES[1][7]={{1,2,3,4,5,6,7}};
static struct { int hp; } CARDB_CARS[7][72];
static struct { menu_state menu; } G;
static int menu_calls,story_calls,seen_power,seen_handling,seen_sound,seen_hp,seen_grade;
static int base_power=24,base_handling=10;
static const cardb_color *chosen_color(void) { return &color; }
void menu_tune(const menu_state *m,int *p,int *h) { (void)m; ++menu_calls; *p=base_power; *h=base_handling; }
void menu_story_tune(const menu_state *m,int *p,int *h) { (void)m; ++story_calls; *p=base_power; *h=base_handling; }
static void snd_set_car(unsigned type,int p) { assert(type==13); seen_sound=p; }
int car_tune_hp(int hp,int p) { seen_power=p; return hp+p; }
int car_tune_grade(int p,int h) { seen_handling=h; return p*100+h; }
static void hud_set_tune(int hp,int grade) { seen_hp=hp; seen_grade=grade; }
static car_spec race(int vs) {
'''
post=r'''
 return spec;
}
static void check(int vs,int p,int h) {
 static menu_state before, after;
 before=G.menu; pre_race_tune_clear(&before.pre_tune);
 car_spec s=race(vs);
 after=G.menu; pre_race_tune_clear(&after.pre_tune);
 assert(memcmp(&before,&after,sizeof before)==0);
 assert(s.power==p && s.handling==h);
 assert(seen_sound==p && seen_power==p && seen_handling==h);
 assert(seen_hp==300+p && seen_grade==p*100+h);
 assert(s.type==13 && s.gears==6 && s.spokes==7 && s.tyres[5]==6);
}
static void prepare(int game,int confirm) {
 memset(&G,0,sizeof G); G.menu.game=game; G.menu.maker=2; G.menu.car=3;
 assert(pre_race_tune_begin(&G.menu.pre_tune,game,2,3,24,10));
 assert(pre_race_tune_step(&G.menu.pre_tune,-1));
 if(confirm) assert(pre_race_tune_confirm(&G.menu.pre_tune));
}
int main(void) {
 CARDB_CARS[2][3].hp=300;
 prepare(MENU_GAME_STORY,1); check(0,23,11); assert(G.menu.pre_tune.state==0);
 check(0,24,10); assert(story_calls==2);
 prepare(MENU_GAME_TA,0); check(0,24,10); assert(G.menu.pre_tune.state==0);
 prepare(MENU_GAME_TA,1); G.menu.car=4; CARDB_CARS[2][4].hp=300;
 check(0,24,10); assert(G.menu.pre_tune.state==0); G.menu.car=3; check(0,24,10);
 prepare(MENU_GAME_TA,1); check(1,24,10); assert(G.menu.pre_tune.state==2);
 check(0,23,11); assert(G.menu.pre_tune.state==0); check(0,24,10);
 assert(menu_calls>0); puts("app pre-race tune extracted call sites passed"); return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="pre-tune-app-host-") as t:
 c=Path(t)/"fixture.c"; exe=Path(t)/"fixture"; c.write_text(pre+select+audio+post)
 subprocess.run([os.environ.get("CC","cc"),"-std=c11","-Wall","-Wextra","-Werror","-DWMMT_PRE_RACE_TEST","-I",str(SRC),str(c),str(SRC/"pre_race_tune.c"),"-o",str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
