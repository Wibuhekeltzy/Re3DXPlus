import pathlib, subprocess, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class StoryPeriodTest(unittest.TestCase):
 def test_period(self):
  text=(ROOT/'src/hud.c').read_text(); code=text[text.index('#define STORY_CUT_N'):text.index('static void demo_warm_add')]
  prefix=r"""#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "car.h"
#define STORY_CUTIN_RECORD_COUNT 5
#define CUT_Y 0
#define CUT_CELL 10
#define CUT_FULL 20
#define T_CUT_FACE 100
#define T_CUT_TEXT 101
#define SND_TURN3_CUTIN 1
typedef struct { int anim,t; } cut_anime;
typedef struct { int from,to; } anm_swap;
typedef struct { int episode; const char *variant,*event,*key,*way; int face_sprite,text_sprite; } StoryCutinRecord;
static const StoryCutinRecord STORY_CUTIN_RECORDS[]={{1,"","ECutInStart","","Left",1,2},{1,"","ECutInStart","","Left",1,3},{1,"","ECutInRace","10","Left",1,4},{1,"","ECutInWin","","Left",1,5},{1,"","ECutInWin","","Left",1,6}};
static int ten_level=-1,visible=1;
static int demo_cut_anim(int w,int k) { return visible?1:-1; }
static void cut_play(cut_anime *a,int anim,int start,int n) { a->anim=anim; a->t=start; }
static int cut_frame(cut_anime *a) { return 0; }
static void snd_menu(int s) { }
static int anm_sprite(int s) { return visible?s:-1; }
static int anm_frames(int a) { return 200; }
static float anm_sprite_width(int s) { return 20; }
static void anm_draw_ex(int a,int f,float alpha,float x,float y,const anm_swap *s,int n) { }
"""
  main=r"""
int main(void) {
 car_race r={0}; r.play_state=2;
 hud_story_cutin_start(0,0); hud_story_cutin_step(&r,0);
 assert(story_cut.current==0 && story_cut.queued[1] && !story_cut.presented[0]);
 for(int i=0;i<19;i++) hud_story_cutin_step(&r,0);
 assert(story_cut.age==20 && !story_cut.presented[0]);
 visible=0; hud_story_cutin_draw(); assert(!story_cut.presented[0]);
 visible=1; hud_story_cutin_draw(); assert(story_cut.presented[0]);
 hud_story_cutin_period(0,0,1); hud_story_cutin_step(&r,0);
 assert(story_cut.presented[0] && !story_cut.queued[0] && story_cut.current==1);
 r.story_distance_valid=1; r.story_distance=20; hud_story_cutin_step(&r,0);
 assert(story_cut.queued[2]);
 r.over=1; r.won=1; hud_story_cutin_step(&r,0);
 assert(!story_cut.queued[1] && !story_cut.queued[2] && story_cut.current==3);
 for(int i=0;i<19;i++) hud_story_cutin_step(&r,0); hud_story_cutin_draw();
 assert(story_cut.presented[3]);
 hud_story_cutin_period(0,0,1); assert(!story_cut.presented[3]);
 r.over=0; r.story_distance=20; hud_story_cutin_step(&r,0);
 assert(story_cut.current==1 && story_cut.queued[2]);
 hud_story_cutin_period(0,0,0); assert(!story_cut.presented[0]);
 story_cut.presented[0]=1; hud_story_cutin_period(1,0,1); assert(!story_cut.presented[0]);
 hud_story_cutin_start(0,0); story_cut.presented[0]=1;
 hud_story_cutin_period(0,1,1); assert(!story_cut.presented[0]);
 /* A result raised after the last tick is prepared once; both result rows
  * fit in the freeze without depending on a successful draw. */
 hud_story_cutin_start(0,0); r.over=1; r.won=1; visible=0;
 int hold=hud_story_cutin_goal_hold(&r);
 assert(hold==299 && story_cut.current==3 && story_cut.age==1);
 assert(hud_story_cutin_goal_hold(&r)==hold && story_cut.age==1);
 int saw_second_steady=0;
 for(int i=0;i<hold;i++) {
  hud_story_cutin_step(&r,0);
  if(story_cut.current==4 && story_cut.age>=20 && story_cut.age<=139) saw_second_steady++;
 }
 assert(saw_second_steady==120 && story_cut.current<0 && story_cut.qhead==story_cut.qtail);
 hud_story_cutin_start(0,0); r.won=0;
 assert(hud_story_cutin_goal_hold(&r)==180 && story_cut.finished && story_cut.current<0);
 hud_story_cutin_start(-1,0); assert(hud_story_cutin_goal_hold(&r)==180);
 /* A confirmed continuing suspend must not queue the episode's final Win.
  * Previously presented race dialogue remains deduplicated in the next period. */
 r.over=0; r.play_state=2; r.won=0; visible=1;
 hud_story_cutin_start(0,0); hud_story_cutin_step(&r,0);
 for(int i=0;i<19;i++) hud_story_cutin_step(&r,0);
 hud_story_cutin_draw(); assert(story_cut.presented[0]);
 r.over=1; r.won=1; r.play_state=10;
 hud_story_cutin_suspend(); hud_story_cutin_step(&r,0);
 assert(story_cut.finished && story_cut.current<0 && story_cut.qhead==story_cut.qtail);
 assert(!story_cut.queued[3] && !story_cut.queued[4]);
 hud_story_cutin_period(0,0,1); assert(story_cut.presented[0]);
 r.play_state=8; hud_story_cutin_step(&r,0);
 assert(story_cut.current==3 && story_cut.queued[4]);
 /* Excessive synthetic queue is bounded and ignores GPU presentation. */
 story_cut.active=story_cut.finished=1; story_cut.current=3; story_cut.age=1;
 story_cut.qhead=0; story_cut.qtail=4; assert(hud_story_cutin_goal_hold(&r)==600);
 return 0;
}
"""
  with tempfile.TemporaryDirectory() as d:
   src=pathlib.Path(d)/'period.c'; exe=pathlib.Path(d)/'period'; src.write_text(prefix+code+main)
   subprocess.run(['cc','-std=c99','-I',str(ROOT/'src'),str(src),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True)
if __name__=='__main__': unittest.main()
