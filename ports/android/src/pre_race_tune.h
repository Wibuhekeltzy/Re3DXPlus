/* Session-only race balance, single-player or a VS room.
 * This module does not read, change, or persist garage/story data. */
#ifndef PRE_RACE_TUNE_H
#define PRE_RACE_TUNE_H

typedef struct {
  int state; /* 0 empty, 1 editing, 2 confirmed */
  int game, maker, car;
  int base_power, base_handling;
  int power;
} pre_race_tune;

/* Clear on initialization, cancellation, return to previous menu, or race abandonment. */
void pre_race_tune_clear(pre_race_tune *t);

/* Start fresh. Returns 1 only for a valid already-full 34-point tune, regardless
 * of whether it originated as a full-tune setting or completed story tune.
 * Pass the effective tune for this particular mode, not its stock horsepower.
 * VS accepts partial tunes and preserves their existing point total.
 * A failed begin also clears any prior selection. */
int pre_race_tune_begin(pre_race_tune *t, int game, int maker, int car, int power, int handling);

/* delta is -1 or +1 power point. Returns 1 if changed; handling is the remainder. */
int pre_race_tune_step(pre_race_tune *t, int delta);
/* Confirm the current draft. Returns 1 on success. */
int pre_race_tune_confirm(pre_race_tune *t);

/* Apply a confirmed VS balance without consuming it, for profile and repeated races.
 * Identity and the original effective tune must still match. */
int pre_race_tune_vs_apply(const pre_race_tune *t, int maker, int car, int *power, int *handling);

/* Consume once at actual race creation. Outputs are changed only if identity and
 * the original effective tune still match. Always clears the selection, even on
 * mismatch, so a stale draft can never affect a later race. Never call for VS. */
int pre_race_tune_take(pre_race_tune *t, int game, int maker, int car,
                       int base_power, int base_handling, int *power, int *handling);
#endif
