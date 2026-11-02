/* Host test: cc -std=c11 audio_arcade_oneshot_test.c -lm -o /tmp/audio_arcade_oneshot_test */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../src/sound.c"
float car_kmh(const car_state *c) { (void)c; return 0; }
int main(void) {
  car_state car = {0}; car_input in = {0};
  snd_set_car(1, 0); assert(valb_sample == S_VALB2);
  snd_set_car(6, 0); assert(valb_sample == S_VALB3);
  snd_set_car(999, 0); assert(valb_sample == S_VALVE);
  SAMPLES[S_VALVE].n = 22050; SAMPLES[S_VALVE].rate = 22050;
  car.se.valb = car.se.backfire = 1; car.se.valb_vol = 0.5f;
  car.se.backfire_vol = 0.8f; car.se.backfire_hz = 11025;
  snd_update(&car, &in, 1.0f/60);
  assert(!car.se.valb && !car.se.backfire && shot_next == 2);
  assert(SHOTS[0].s == &SAMPLES[S_VALVE]);
  assert(fabsf(SHOTS[0].vol - .73f*.73f*.5f) < 1e-6f);
  assert(SHOTS[1].s == &SAMPLES[S_BACKFIRE2] && SHOTS[1].pitch == .5f);
  in.throttle = 1; car.rpm = 6000; car.shift_event = 1;
  snd_update(&car, &in, 1.0f/60); in.throttle = 0;
  snd_update(&car, &in, 1.0f/60); assert(shot_next == 2);
  car.se.valb = car.se.backfire = 1; car.se.backfire_hz = 0;
  snd_update(&car, &in, 1.0f/60);
  assert(shot_next == 3 && SHOTS[2].s == &SAMPLES[S_BACKFIRE1] && SHOTS[2].pitch == 1);
  car.se.backfire = 1; car.se.backfire_vol = NAN;
  snd_update(&car, &in, 1.0f/60); assert(shot_next == 3);
  puts("arcade one-shots passed");
}
