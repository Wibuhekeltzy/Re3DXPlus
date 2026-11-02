#include "pre_race_tune.h"
#include "car.h"
#include "menu.h"
#include <string.h>

void pre_race_tune_clear(pre_race_tune *t) {
  if (t) memset(t, 0, sizeof *t);
}

static int valid_full(int power, int handling) {
  const int min = CAR_TUNE_TOTAL - CAR_TUNE_MAX;
  return power >= min && power <= CAR_TUNE_MAX && handling >= min && handling <= CAR_TUNE_MAX &&
         power + handling == CAR_TUNE_TOTAL;
}

int pre_race_tune_begin(pre_race_tune *t, int game, int maker, int car, int power, int handling) {
  if (!t) return 0;
  pre_race_tune_clear(t);
  if (maker < 0 || car < 0 ||
      (game == MENU_GAME_VS ? (power < 0 || power > CAR_TUNE_MAX || handling < 0 || handling > CAR_TUNE_MAX)
                            : ((game != MENU_GAME_STORY && game != MENU_GAME_TEN && game != MENU_GAME_TA) || !valid_full(power, handling)))) return 0;
  t->state = 1;
  t->game = game; t->maker = maker; t->car = car;
  t->base_power = power; t->base_handling = handling; t->power = power;
  return 1;
}

int pre_race_tune_step(pre_race_tune *t, int delta) {
  if (!t || t->state != 1 || (delta != -1 && delta != 1)) return 0;
  int power = t->power + delta;
  int handling = t->base_power + t->base_handling - power;
  if (t->game == MENU_GAME_VS ? (power < 0 || power > CAR_TUNE_MAX || handling < 0 || handling > CAR_TUNE_MAX)
                              : !valid_full(power, handling)) return 0;
  t->power = power;
  return 1;
}

int pre_race_tune_confirm(pre_race_tune *t) {
  if (!t || t->state != 1) return 0;
  int handling = t->base_power + t->base_handling - t->power;
  if (t->game == MENU_GAME_VS ? (t->power < 0 || t->power > CAR_TUNE_MAX || handling < 0 || handling > CAR_TUNE_MAX)
                              : !valid_full(t->power, handling)) return 0;
  t->state = 2;
  return 1;
}

int pre_race_tune_vs_apply(const pre_race_tune *t, int maker, int car, int *power, int *handling) {
  if (!t || !power || !handling || power == handling || t->state != 2 || t->game != MENU_GAME_VS ||
      t->maker != maker || t->car != car || t->base_power != *power || t->base_handling != *handling) return 0;
  int h = t->base_power + t->base_handling - t->power;
  if (t->power < 0 || t->power > CAR_TUNE_MAX || h < 0 || h > CAR_TUNE_MAX) return 0;
  *power = t->power; *handling = h;
  return 1;
}

int pre_race_tune_take(pre_race_tune *t, int game, int maker, int car,
                       int base_power, int base_handling, int *power, int *handling) {
  if (!t) return 0;
  int ok = t->state == 2 && t->game == game && t->maker == maker && t->car == car &&
           t->base_power == base_power && t->base_handling == base_handling &&
           valid_full(base_power, base_handling) && valid_full(t->power, CAR_TUNE_TOTAL - t->power) &&
           power && handling && power != handling;
  if (ok) { *power = t->power; *handling = CAR_TUNE_TOTAL - t->power; }
  pre_race_tune_clear(t);
  return ok;
}
