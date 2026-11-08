/* The Android build's 60 Hz clock (src/timing60.h: the pacer's deadlines, the simulation's
 * steps) and the audio stream's retry policy (src/sound.h snd_retry), both header-only: how many
 * logic steps a 60, 90, 120 and 144 Hz display's frames make in a second, with jitter, stalls and
 * bad times, the first frames and a pause of a race, the pacer's phase kept across wakeup jitter
 * and rebased after a stall, the tries and waits of the audio retry.
 *
 *   gcc -std=gnu99 -Wall -Isrc test/timing60_test.c -lm -o timing60_test && ./timing60_test
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/sound.h"
#include "../src/timing60.h"

static int fails;
#define CHECK(c) \
  do { \
    if (!(c)) { \
      printf("FAIL line %d: %s\n", __LINE__, #c); \
      ++fails; \
    } \
  } while (0)

/* the steps a display of hz makes in `seconds` of frames, each dt apart (jittered by up to `jitter` of dt); the
 * time that really went by is given back, the steps being right when they are that time's sixtieths, to a step */
static int run(double hz, double seconds, double jitter, unsigned seed, int *max_run_of_zero, int *max_steps,
               double *elapsed) {
  timing60_sim s;
  timing60_sim_reset(&s);
  srand(seed);
  double dt = 1.0 / hz, t = 0;
  int total = 0, zeros = 0, frames = (int)(seconds * hz + 0.5);
  *max_run_of_zero = *max_steps = 0;
  for (int i = 0; i < frames; ++i) {
    double d = dt * (1.0 + jitter * ((double)rand() / RAND_MAX * 2.0 - 1.0));
    t += d;
    int n = timing60_sim_steps(&s, d);
    total += n;
    if (n > *max_steps) *max_steps = n;
    zeros = n ? 0 : zeros + 1;
    if (zeros > *max_run_of_zero) *max_run_of_zero = zeros;
  }
  *elapsed = t;
  return total;
}

static void test_sim(void) {
  int zeros, most;
  double elapsed;
  /* ten seconds of frames make six hundred steps, whatever the display's rate: not one frame per vsync */
  static const double hz[] = { 60, 72, 90, 120, 144, 165 };
  for (unsigned i = 0; i < sizeof hz / sizeof *hz; ++i) {
    int n = run(hz[i], 10.0, 0, 1, &zeros, &most, &elapsed);
    CHECK(fabs(n - elapsed * 60.0) < 1.01 && fabs(elapsed - 10.0) < 0.1);
    CHECK(most <= 1); /* never two steps at once on a steady display at 60 or above */
    CHECK(zeros <= (int)(hz[i] / 60.0 + 1.0));
  }
  /* with the time between frames jittered by 15%, the steps are still the time's sixtieths, to a step */
  for (unsigned i = 0; i < sizeof hz / sizeof *hz; ++i)
    for (unsigned seed = 1; seed <= 5; ++seed) {
      int n = run(hz[i], 10.0, 0.15, seed, &zeros, &most, &elapsed);
      CHECK(fabs(n - elapsed * 60.0) < 1.01);
      CHECK(most <= 2);
    }
  /* a 30 Hz display (or a frame twice as long) makes two steps a frame */
  int n = run(30, 10.0, 0, 1, &zeros, &most, &elapsed);
  CHECK(n == 600 && most == 2 && zeros == 0);
  /* what the iOS build counts, a step to the vsync, would make 1200 of a 120 Hz display's ten seconds: not here */
  n = run(120, 10.0, 0, 1, &zeros, &most, &elapsed);
  CHECK(n < 700);

  /* a frame at exactly 1/60 s is one step, every time (the sum of rounding errors never skips one) */
  timing60_sim s;
  timing60_sim_reset(&s);
  int skipped = 0;
  for (int i = 0; i < 100000; ++i) skipped += timing60_sim_steps(&s, 1.0 / 60.0) != 1;
  CHECK(skipped == 0);

  /* bad times make no step and break nothing; a long stall is held to a tenth of a second (6 steps) */
  timing60_sim_reset(&s);
  CHECK(timing60_sim_steps(&s, NAN) == 0);
  CHECK(timing60_sim_steps(&s, INFINITY) == 6 || timing60_sim_steps(&s, INFINITY) == 0);
  timing60_sim_reset(&s);
  CHECK(timing60_sim_steps(&s, -1.0) == 0);
  CHECK(timing60_sim_steps(&s, 0.0) == 0);
  CHECK(timing60_sim_steps(&s, 5.0) == 6);
  CHECK(timing60_sim_steps(&s, 0.1) == 6);
  CHECK(timing60_sim_steps(&s, 1.0 / 60.0) == 1);
  CHECK(s.fraction >= 0 && s.fraction < 1);

  /* the fraction is carried: three frames of 0.4 steps make one step, on the third */
  timing60_sim_reset(&s);
  CHECK(timing60_sim_steps(&s, 0.4 / 60.0) == 0);
  CHECK(timing60_sim_steps(&s, 0.4 / 60.0) == 0);
  CHECK(timing60_sim_steps(&s, 0.4 / 60.0) == 1);

  /* the race's own: nothing in a pause or the first frames, the carried fraction let go of */
  timing60_sim_reset(&s);
  CHECK(timing60_sim_steps(&s, 0.9 / 60.0) == 0 && s.fraction > 0.8);
  CHECK(timing60_race_steps(&s, 1.0 / 60.0, 1, 0) == 0 && s.fraction == 0);
  CHECK(timing60_race_steps(&s, 1.0 / 60.0, 0, 1) == 0 && s.fraction == 0);
  CHECK(timing60_race_steps(&s, 1.0 / 60.0, 0, 0) == 1);
  CHECK(timing60_race_steps(&s, 3.0 / 60.0, 0, 0) == 3);
}

static void test_pacer(void) {
  const int64_t P = WMMT_PERIOD_NS;
  timing60_pacer p;
  timing60_pacer_reset(&p);
  /* the first deadline is now: no wait */
  int64_t t = 1000000000000LL;
  CHECK(timing60_deadline(&p, t) == t);
  CHECK(timing60_deadline(&p, t + 5) == t); /* (asking again changes nothing) */
  /* frames that wake up a little late keep the phase: the deadlines stay P apart from the first */
  int64_t first = t;
  int64_t jitter[] = { 0, 300000, 1200000, 50000, 2800000, 10000, 700000, 0, 1500000, 90000 };
  for (int i = 0; i < 10; ++i) {
    int64_t d = timing60_deadline(&p, first + (int64_t)i * P);
    CHECK(d == first + (int64_t)i * P);
    int64_t woke = d + jitter[i];
    timing60_started(&p, woke);
    CHECK(p.next == first + (int64_t)(i + 1) * P); /* the next deadline: one period on, jitter forgotten */
  }
  /* a wakeup more than half a period late skips the deadline it already missed: no burst to catch up */
  timing60_pacer_reset(&p);
  timing60_deadline(&p, first);
  int64_t woke = first + P * 3 / 4; /* 12.5 ms late */
  timing60_started(&p, woke);
  CHECK(p.next == first + 2 * P && p.next > woke + P / 2 - 1);
  /* a stall of four periods or more: rebased on the moment it ended, the frames not replayed */
  timing60_pacer_reset(&p);
  timing60_deadline(&p, first);
  woke = first + 10 * P;
  timing60_started(&p, woke);
  CHECK(p.next == woke + P);
  /* the pacer's rate: over 600 frames of exactly on-time wakeups, 10 seconds pass */
  timing60_pacer_reset(&p);
  int64_t d0 = timing60_deadline(&p, first), d = d0;
  for (int i = 0; i < 600; ++i) {
    d = timing60_deadline(&p, d);
    timing60_started(&p, d);
  }
  CHECK(llabs((long long)(p.next - d0) - 600LL * P) <= 1);
}

static void test_retry(void) {
  snd_retry r;
  /* the first try is at once */
  snd_retry_begin(&r, 100.0);
  CHECK(snd_retry_due(&r, 100.0, 0) && !snd_retry_due(&r, 99.0, 0) && !snd_retry_due(&r, 100.0, 1));
  /* a try that works: nothing more pending, the attempt counted */
  snd_retry_result(&r, 100.0, 1);
  CHECK(!r.pending && r.attempts == 1 && !snd_retry_due(&r, 200.0, 0));
  /* a try that fails: the next one five seconds on, twice, then no more */
  snd_retry_begin(&r, 0.0);
  snd_retry_result(&r, 0.0, 0);
  CHECK(r.pending && r.attempts == 1 && !snd_retry_due(&r, 4.9, 0) && snd_retry_due(&r, 5.0, 0));
  snd_retry_result(&r, 5.0, 0);
  CHECK(r.pending && r.attempts == 2 && !snd_retry_due(&r, 9.9, 0) && snd_retry_due(&r, 10.0, 0));
  snd_retry_result(&r, 10.0, 0);
  CHECK(!r.pending && r.attempts == 3 && !snd_retry_due(&r, 1000.0, 0));
  /* a stream that went away (a pulled jack) before it was stable: a try in five seconds, the tries it took
   * already counted (it counts as one used if none was); a try that works is the end of it */
  snd_retry_begin(&r, 0.0);
  snd_retry_result(&r, 0.0, 1);
  snd_retry_interrupt(&r, 50.0);
  CHECK(r.pending && r.attempts == 1 && !snd_retry_due(&r, 54.9, 0) && snd_retry_due(&r, 55.0, 0));
  snd_retry_result(&r, 55.0, 0); /* that one fails, the next, five seconds on, fails too: three */
  CHECK(r.pending && r.attempts == 2 && snd_retry_due(&r, 60.0, 0));
  snd_retry_result(&r, 60.0, 0);
  CHECK(!r.pending && r.attempts == 3 && !snd_retry_due(&r, 1000.0, 0));
  snd_retry_interrupt(&r, 70.0); /* a stream gone with the tries spent: none more */
  CHECK(!r.pending && r.attempts == 3);
  /* a stream gone that never had a try counted (none made yet): that is one */
  snd_retry_begin(&r, 0.0);
  r.attempts = 0, r.pending = 0;
  snd_retry_interrupt(&r, 3.0);
  CHECK(r.pending && r.attempts == 1 && snd_retry_due(&r, 8.0, 0));
  /* stable for ten seconds of being started: the tries are given back; a stop on the way, not */
  snd_retry_begin(&r, 0.0);
  snd_retry_result(&r, 0.0, 1);
  snd_retry_stability(&r, 1.0, 1);
  snd_retry_stability(&r, 10.9, 1);
  CHECK(r.attempts == 1);
  snd_retry_stability(&r, 11.0, 1);
  CHECK(r.attempts == 0);
  snd_retry_result(&r, 20.0, 1);
  snd_retry_stability(&r, 21.0, 1);
  snd_retry_stability(&r, 25.0, 0); /* not started: the count of the ten seconds starts again */
  snd_retry_stability(&r, 26.0, 1);
  snd_retry_stability(&r, 35.0, 1);
  CHECK(r.attempts == 1);
  snd_retry_stability(&r, 36.0, 1);
  CHECK(r.attempts == 0);
  /* a pause holds a due try */
  snd_retry_begin(&r, 0.0);
  CHECK(!snd_retry_due(&r, 1.0, 1) && snd_retry_due(&r, 1.0, 0));
}

int main(void) {
  test_sim();
  test_pacer();
  test_retry();
  printf(fails ? "timing60: %d FAILED\n" : "timing60: ok\n", fails);
  return fails != 0;
}
