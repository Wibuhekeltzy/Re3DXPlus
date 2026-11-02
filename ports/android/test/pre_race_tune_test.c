#include "pre_race_tune.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  pre_race_tune t = {0};
  int p = -1, h = -1;
  /* Both a full-tune setting and a completed story card enter via effective values. */
  for (int game = 0; game < 3; ++game) {
    for (int initial = 10; initial <= 24; ++initial) {
      assert(pre_race_tune_begin(&t, game, 2, 3, initial, 34-initial));
      for (int target = initial; target < 24; ++target) assert(pre_race_tune_step(&t, 1));
      assert(!pre_race_tune_step(&t, 1));
      for (int target = 24; target > 10; --target) assert(pre_race_tune_step(&t, -1));
      assert(!pre_race_tune_step(&t, -1));
      assert(!pre_race_tune_step(&t, 2));
      assert(pre_race_tune_confirm(&t));
      assert(!pre_race_tune_step(&t, 1));
      assert(pre_race_tune_take(&t, game, 2, 3, initial, 34-initial, &p, &h));
      assert(p == 10 && h == 24);
      assert(!pre_race_tune_take(&t, game, 2, 3, initial, 34-initial, &p, &h));
    }
  }
  assert(!pre_race_tune_begin(&t, 3, 0, 0, 24, 10)); /* versus never enters */
  assert(!pre_race_tune_begin(&t, 0, 0, 0, 0, 0));
  assert(!pre_race_tune_begin(&t, 0, 0, 0, 24, 9));
  assert(!pre_race_tune_begin(&t, 0, 0, 0, 25, 9));
  assert(!pre_race_tune_begin(&t, 0, 0, 0, 24, 24));
  assert(pre_race_tune_begin(&t, 0, 1, 2, 24, 10));
  assert(pre_race_tune_step(&t, -1));
  pre_race_tune_clear(&t); /* back/cancel cannot apply */
  assert(!pre_race_tune_confirm(&t));
  p = 7; h = 8;
  assert(!pre_race_tune_take(&t, 0, 1, 2, 24, 10, &p, &h));
  assert(p == 7 && h == 8);
  assert(pre_race_tune_begin(&t, 0, 1, 2, 24, 10));
  assert(pre_race_tune_confirm(&t));
  assert(!pre_race_tune_take(&t, 0, 1, 3, 24, 10, &p, &h));
  assert(p == 7 && h == 8);
  assert(pre_race_tune_begin(&t, 0, 1, 2, 24, 10));
  assert(pre_race_tune_confirm(&t));
  assert(!pre_race_tune_take(&t, 0, 1, 2, 23, 11, &p, &h));
  assert(p == 7 && h == 8);
  assert(pre_race_tune_begin(&t, 0, 1, 2, 24, 10));
  assert(pre_race_tune_confirm(&t));
  assert(!pre_race_tune_begin(&t, 0, 1, 2, 1, 1));
  assert(!pre_race_tune_take(&t, 0, 1, 2, 24, 10, &p, &h));
  puts("pre-race tuning tests passed");
  return 0;
}
