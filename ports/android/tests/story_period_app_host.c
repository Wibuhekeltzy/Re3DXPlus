#include <assert.h>
#include <stdio.h>
#include "menu.h"
#define STORYRACE_SELECTS 8
#define FINISH_GOAL_WAIT 90
static const int STORYRACE_FADE[8][5]={{0}};
static const struct {int select;} RIVALDB[]={{2},{3}};
typedef struct {int won; float gap;} race_t;
static struct {menu_state menu; int rival,story_race,story_cut_pending,story_cut_select,story_cut_loop,story_cut_period,vs_race; struct { struct { int unused; } race; } car;} G;
static int next_result, got_select,got_loop,got_cont;
static int off_calls, clear_calls, closed_calls, carry_cancel_calls;
static void story_carry_cancel(void){carry_cancel_calls++;}
static void vs_off(void){off_calls++;}
static void car_clear_junction(void *p){assert(p==&G.car.race);clear_calls++;}
void menu_vs_closed(menu_state *m,const char *note){assert(m==&G.menu && note);closed_calls++;}
static int story_next_race(int p,int r){(void)p;(void)r;return next_result;}
static void story_race_commit(int a,int b){(void)a;(void)b;}
void menu_finish_open(menu_state *m,int g,const menu_result *p){(void)m;(void)g;(void)p;}
static void hud_story_cutin_period(int s,int l,int c){got_select=s;got_loop=l;got_cont=c;}
/* GO */
/* FINISH */
static void back_next(menu_state *m) {
/* BACK */
}
static void finish_race(int won,int requested_next) {race_t v={won,0},*race=&v; int goal=1; next_result=requested_next;
/* END */
}
static void start_race(int ten,int vs) {
/* START */
}
static void pause_leave(void) {
/* PAUSE */
}
static void reset(void){G=(typeof(G)){0};carry_cancel_calls=0;G.rival=0;G.menu.game=MENU_GAME_STORY;G.menu.story_pick=2;G.menu.st_div=5;}
static void win_next(void){finish_race(1,1);assert(G.story_cut_pending&&G.story_cut_select==2&&G.story_cut_loop==1&&G.story_cut_period==1&&G.story_race==1);assert(G.menu.finish_story==3);back_next(&G.menu);assert(G.menu.screen==MENU_PRE&&G.menu.pre_quiet);}
int main(void){
 reset();win_next();start_race(0,0);assert(got_select==2&&got_loop==1&&got_cont==1&&!G.story_cut_pending);start_race(0,0);assert(!got_cont);
 reset();win_next();go(&G.menu,MENU_PRE);assert(!G.menu.pre_quiet);start_race(0,0);assert(!got_cont&&!G.story_cut_pending);
 reset();win_next();G.menu.st_div=10;start_race(0,0);assert(!got_cont&&!G.story_cut_pending);
 reset();win_next();G.rival=1;start_race(0,0);assert(!got_cont&&!G.story_cut_pending);
 reset();win_next();G.story_race=0;start_race(0,0);assert(!got_cont&&!G.story_cut_pending);
 reset();win_next();start_race(0,1);assert(got_select==-1&&!got_cont&&!G.story_cut_pending);
 reset();win_next();G.menu.game=MENU_GAME_TA;start_race(0,0);assert(got_select==-1&&!got_cont&&!G.story_cut_pending);
 reset();win_next();start_race(1,0);assert(got_select==-1&&!got_cont&&!G.story_cut_pending);
 reset();win_next();pause_leave();assert(carry_cancel_calls==1&&!G.story_cut_pending&&!G.story_race);start_race(0,0);assert(!got_cont);
 reset();win_next();G.vs_race=1;pause_leave();assert(!G.story_cut_pending && !G.story_race && !G.vs_race && off_calls==1 && clear_calls==1 && closed_calls==1);
 reset();G.story_cut_pending=1;finish_race(0,1);assert(!G.story_cut_pending&&!G.story_race&&G.menu.finish_story==2);
 reset();G.story_cut_pending=1;finish_race(1,-1);assert(!G.story_cut_pending&&!G.story_race&&G.menu.finish_story==1);
 reset();G.rival=1;finish_race(1,1);assert(!G.story_cut_pending&&!G.story_race);
 reset();win_next();G.menu.st_div=10;start_race(0,0);G.menu.st_div=5;start_race(0,0);assert(!got_cont);
 puts("story period extracted host tests passed");return 0;}
