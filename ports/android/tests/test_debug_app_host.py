#!/usr/bin/env python3
"""Exercise the real diagnostic helpers and extracted physics loop on a host."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SRC = Path(__file__).resolve().parents[1] / 'src'

class HostTest(unittest.TestCase):
    def test_sampling_and_snapshots(self):
        source = (SRC / 'app.c').read_text()
        helpers = source[source.index('static uint64_t debug_render_seq, debug_sim_seq;'):source.index('\n#endif\n\n/* Published backfire')]
        frame = source[source.index('void app_frame(float dt) {'):]
        start = frame.index('  for (int i = 0; i < steps; ++i) {', frame.index('int frozen ='))
        loop = frame[start:frame.index('  if (!G.paused && !frozen && !movie) snd_update', start)]
        self.assertLess(loop.index('debug_before = debug_capture()'), loop.index('car_update(&G.car, &in)'))
        self.assertLess(loop.index('car_update(&G.car, &in)'), loop.index('debug_step("before"'))
        self.assertIn('const wpk_scene *s[1 + CAR_RIVALS + CAR_TRAFFIC]', source)
        main = (SRC / 'main_android.c').read_text()
        self.assertIn('debug_present_seq % 12 == 0', main)
        self.assertIn('e.phase == 0 || e.phase == 2 || e.phase == 3', main)
        self.assertNotIn('"x"', main[main.index('static void touch_deliver'):main.index('/* TOUCH_FAILSAFE_END */')])
        prefix = r'''
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>
#include "car.h"
#define __ANDROID__ 1
#define WMMT_DEBUG_LOG 1
#define MENU_GAME_STORY 3
#define TEN_WHITE_HOLD 10
#define TEN_WHITE_FADE 10
#define SND_BEATEN 1
static struct {
 struct { int game, screen, story_pick; } menu;
 car_state car;
 int in_menu, paused, ten_freeze, race_frames, story_race, race_nrv;
 int race_course, track_city, ten_white, n_ten, ten_beaten_on, ten_bgm_off;
 float movie_len; double race_time;
 car_rival race_rv[CAR_RIVALS]; int rival_pack[CAR_RIVALS], ten_pack[CAR_TEN_RIVALS];
} G;
static int test_clock_fail, test_clock_calls;
static int test_clock_gettime(clockid_t id, struct timespec *ts) {
 (void)id; if (test_clock_fail) return -1;
 ts->tv_sec = 1; ts->tv_nsec = ++test_clock_calls; return 0;
}
#define clock_gettime test_clock_gettime
static int debug_log_enabled(void) { return 1; }
static void debug_log_emit(const char *fmt, ...) __attribute__((format(printf,1,2)));
static void debug_log_emit(const char *fmt, ...) { va_list ap; va_start(ap,fmt); vprintf(fmt,ap); va_end(ap); putchar('\n'); }
'''
        suffix = r'''
static int scenario, tick;
void car_update(car_state *c, const car_input *in) {
 (void)in; ++tick; c->pos[0] += 1;
 if (scenario == 1) { c->wall_hit = tick == 3 ? 1 : 0; }
 if (scenario == 2) { c->wall_hit = tick % 2; c->impact = tick % 2; c->effect.spark = tick % 2; }
 if (scenario == 3 && tick == 7) ++c->gear;
 if (scenario == 4 && tick == 12) { c->rpm = NAN; c->race.remain_valid = 1; c->race.remain = INFINITY; }
 if (scenario == 6 && tick == 7) c->rpm = NAN;
 if (scenario == 6 && tick == 248) c->rpm = 1000;
}
static void backfire_tick(void) {}
static void spark_step(int i, const float *b, const car_effect *e) {(void)i;(void)b;(void)e;}
static void hud_ten_cutin(const car_race *r,int s) {(void)r;(void)s;}
static void hud_story_cutin_step(const car_race *r,int h) {(void)r;(void)h;}
static void snd_menu(int s) {(void)s;}
static void run_steps(int steps, car_input in) {
''' + loop + r'''
}
int main(int argc, char **argv) {
 (void)argc; scenario = argv[1][0]-'0'; car_input in = {0}; debug_race_init();
 for (int i=0; i<300; ++i) {
   if (scenario == 5) {
     if (i == 11) { G.car.n_traffic=0; }
     if (i == 23) { G.car.n_traffic=2; G.car.traffic[0].body[12]=80; G.car.traffic[1].body[12]=29; G.car.traffic[1].model=2; G.car.rival_shown[0]=1; G.car.rival_body[0][12]=27; G.rival_pack[0]=42; }
     if (i == 35) { G.car.n_traffic=1; G.car.traffic[0].body[12]=NAN; G.car.rival_shown[0]=0; }
     if (i == 47) { G.car.pos[0]=NAN; G.car.n_traffic=999; G.car.rival_shown[0]=1; }
     if (i == 59) { G.car.pos[0]=0; G.car.n_traffic=999; G.car.traffic[0].body[12]=NAN; G.car.rival_shown[0]=0; }
   }
   test_clock_fail = scenario == 4 && i == 11;
   debug_frame(1.0f/60,1.0f/60); in.shift_up = scenario == 3 && i == 3;
   run_steps(scenario == 7 ? (i % 4 == 0 ? 0 : i % 4 == 3 ? 1 : 2) : 1,in);
 }
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            shutil.copyfile(SRC / 'car.h', root / 'car.h')
            (root / 'fixture.c').write_text(prefix + helpers + suffix)
            exe = root / 'fixture'
            subprocess.run([os.environ.get('CC','cc'), '-std=c11','-Wall','-Wextra','-Wformat=2','-Werror','-I',str(root),str(root/'fixture.c'),'-o',str(exe),'-lm'],check=True)
            outputs = [subprocess.check_output([str(exe),str(n)],text=True) for n in range(8)]
            baseline = (prefix + helpers + suffix).replace('return seq % 12 == 0;', 'return seq != 0;').replace('debug_sim_seq % 12 == 0', 'debug_sim_seq != 0')
            (root / 'baseline.c').write_text(baseline)
            subprocess.run([os.environ.get('CC','cc'), '-std=c11','-Wall','-Wextra','-Wformat=2','-Werror','-I',str(root),str(root/'baseline.c'),'-o',str(root/'baseline'),'-lm'],check=True)
            baseline_output = subprocess.check_output([str(root/'baseline'),'0'],text=True)
        runs = [[json.loads(line) for line in text.splitlines()] for text in outputs]
        normal = runs[0]
        self.assertEqual([x['render_seq'] for x in normal if x['event']=='frame'], list(range(12,301,12)))
        sim = [x for x in normal if x['event']=='sim']
        self.assertEqual(len(sim), 50)
        self.assertEqual([x['sim_seq'] for x in sim[::2]], list(range(12,301,12)))
        for before, after in zip(sim[::2],sim[1::2]):
            self.assertEqual((before['phase'],after['phase']),('before','after'))
            self.assertEqual(before['sim_seq'],after['sim_seq'])
            self.assertEqual(after['pos'][0],before['pos'][0]+1)
            self.assertEqual(before['sample_monotonic_ns'], 1000000000 + 2 * before['sim_seq'] - 1)
            self.assertEqual(after['sample_monotonic_ns'], before['sample_monotonic_ns'] + 1)
        self.assertEqual([x['sim_seq'] for x in normal if x['event']=='nearby'],list(range(12,301,12)))
        collision = [x for x in runs[1] if x['event']=='sim']
        edge = [x for x in collision if x['sim_seq']==3]
        self.assertEqual((edge[0]['wall_hit'],edge[1]['wall_hit']),(0,1))
        noisy = [x for x in runs[2] if x['event']=='sim']
        self.assertLess(len(noisy), 300 * 2)
        noisy_seqs = {x['sim_seq'] for x in noisy}
        self.assertTrue(set(range(1,62)).issubset(noisy_seqs))
        self.assertNotIn(62, noisy_seqs)
        self.assertNotIn(239, noisy_seqs)
        self.assertTrue(set(range(241,301)).issubset(noisy_seqs))
        anomaly_seqs = {x['sim_seq'] for x in runs[6] if x['event']=='sim'}
        self.assertIn(7, anomaly_seqs); self.assertIn(248, anomaly_seqs)
        multi = [x for x in runs[7] if x['event']=='sim']
        self.assertEqual([x['sim_seq'] for x in multi[::2]], list(range(12,376,12)))
        self.assertTrue(any(x['step_index']==1 for x in multi))
        self.assertTrue(any(x['step_index']==0 for x in multi))
        self.assertTrue(any(x['sim_seq']==4 and x['shift_up']==1 for x in runs[3] if x['event']=='sim'))
        self.assertTrue(any(x['sim_seq']==7 for x in runs[3] if x['event']=='sim'))
        self.assertLess(len(outputs[0].encode()), len(outputs[2].encode()))
        nonfinite = next(x for x in runs[4] if x['event']=='sim' and x['sim_seq']==12 and x['phase']=='after')
        self.assertIsNone(nonfinite['rpm']); self.assertIsNone(nonfinite['remain'])
        self.assertIsNone(nonfinite['sample_monotonic_ns'])
        self.assertIsNone(next(x for x in runs[4] if x['event']=='sim' and x['sim_seq']==12 and x['phase']=='before')['sample_monotonic_ns'])
        nearby = {x['sim_seq']: x for x in runs[5] if x['event']=='nearby'}
        self.assertEqual((nearby[12]['traffic_index'], nearby[12]['rival_index']), (-1, -1))
        self.assertEqual(nearby[12]['traffic_rel'], [None, None, None])
        self.assertEqual((nearby[24]['traffic_count'],nearby[24]['traffic_index'],nearby[24]['traffic_model']), (2,1,2))
        self.assertEqual((nearby[24]['traffic_rel'],nearby[24]['traffic_distance']), ([5,0,0],5))
        self.assertEqual((nearby[24]['rival_index'],nearby[24]['rival_model'],nearby[24]['rival_distance']), (0,42,3))
        self.assertEqual((nearby[36]['traffic_index'],nearby[36]['rival_index']), (-1,-1))
        self.assertIsNone(nearby[36]['traffic_distance'])
        self.assertEqual((nearby[48]['traffic_count'],nearby[48]['traffic_index'],nearby[48]['rival_index']), (12,-1,-1))
        self.assertEqual(nearby[60]['traffic_count'],12)
        self.assertNotEqual(nearby[60]['traffic_index'],-1)
        events5 = runs[5]
        for seq, record in nearby.items():
            idx = events5.index(record)
            before, after = events5[idx-2:idx]
            self.assertEqual((before['event'],before['phase'],after['event'],after['phase']), ('sim','before','sim','after'))
            self.assertEqual((before['sim_seq'],after['sim_seq'],record['sim_seq']), (seq,seq,seq))
            self.assertEqual((before['render_seq'],after['render_seq'],record['render_seq']), (seq,seq,seq))
        allowed = {('event','frame'),('event','sim'),('event','nearby'),('event','race_init'),('phase','before'),('phase','after')}
        for event in normal:
            for key, value in event.items():
                if isinstance(value,str): self.assertIn((key,value),allowed)
        self.assertLess(len(outputs[0]),len(baseline_output)/10)
        print('normal_bytes_300_frames=',len(outputs[0].encode()), 'baseline_bytes=',len(baseline_output.encode()), 'normal_events=',len(normal))

if __name__ == '__main__': unittest.main()
