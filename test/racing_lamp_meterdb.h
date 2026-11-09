/* The meter table the REV lamp's test builds hud.c with (test/racing_lamp_test.py puts it where
 * src/meterdb.h goes, in a tree of its own): the generated one is the disc's, and the lamp's test
 * needs parts of known places, kinds and art. The same shape as tools/meterdb.py writes. Zones are
 * 2 * the card's meter + night: 0 1 Normal, 2 3 For Racing, 4 5 SPECIAL. The comments say what each
 * car's parts are for; test/racing_lamp_test.c gives them their art. */
#ifndef METERDB_H
#define METERDB_H

enum { METER_FIXED, METER_SPEED, METER_REV, METER_BOOST };
typedef struct {
  float x, y, a0, a1;
  int input, add;
} meterdb_part;

#define METERDB_MAX_PARTS 9
#define METER_STYLES 3
#define METER_ZONES (2 * METER_STYLES)
typedef struct {
  int car;
  int hp, gears;
  int n[METER_ZONES];
  meterdb_part parts[METER_ZONES][METERDB_MAX_PARTS];
} meterdb_car;

static const meterdb_car METERDB[6] = {
  /* 0: the lamp where it should be: the For Racing meter's morning (zone 2: its second part) and
   * night (zone 3: its third); the same place and art in the Normal meter's morning (zone 0) and in
   * both SPECIAL ones (zones 4 and 5) are not the lamp */
  { 0, 300, 6, { 2, 0, 3, 3, 1, 1 },
    { { { 300, 300, 0, 0, METER_FIXED, 0 }, { 491, 283, 0, 0, METER_FIXED, 0 } },
      { { 0 } },
      { { 400, 300, 0, 0, METER_FIXED, 0 }, { 491, 283, 0, 0, METER_FIXED, 0 }, { 490, 300, -90, 270, METER_REV, 0 } },
      { { 490, 300, -90, 270, METER_REV, 0 }, { 400, 300, 0, 0, METER_FIXED, 0 }, { 491, 283, 0, 0, METER_FIXED, 0 } },
      { { 491, 283, 0, 0, METER_FIXED, 0 } },
      { { 491, 283, 0, 0, METER_FIXED, 0 } } } },
  /* 1: the lamp's place, its pixels one byte off the arcade's (both times of day) */
  { 1, 300, 6, { 0, 0, 2, 2, 0, 0 },
    { { { 0 } },
      { { 0 } },
      { { 400, 300, 0, 0, METER_FIXED, 0 }, { 491, 283, 0, 0, METER_FIXED, 0 } },
      { { 400, 300, 0, 0, METER_FIXED, 0 }, { 491, 283, 0, 0, METER_FIXED, 0 } } } },
  /* 2: the lamp's place, a sprite of another size */
  { 2, 300, 6, { 0, 0, 2, 2, 0, 0 },
    { { { 0 } },
      { { 0 } },
      { { 400, 300, 0, 0, METER_FIXED, 0 }, { 491, 283, 0, 0, METER_FIXED, 0 } },
      { { 400, 300, 0, 0, METER_FIXED, 0 }, { 491, 283, 0, 0, METER_FIXED, 0 } } } },
  /* 3: the lamp's place, the other time of day's art (the night's lamp in the morning, and back) */
  { 3, 300, 6, { 0, 0, 2, 2, 0, 0 },
    { { { 0 } },
      { { 0 } },
      { { 400, 300, 0, 0, METER_FIXED, 0 }, { 491, 283, 0, 0, METER_FIXED, 0 } },
      { { 400, 300, 0, 0, METER_FIXED, 0 }, { 491, 283, 0, 0, METER_FIXED, 0 } } } },
  /* 4: the lamp's art in parts that are not the lamp: added on, a needle's input, a pixel off
   * either way */
  { 4, 300, 6, { 0, 0, 5, 0, 0, 0 },
    { { { 0 } },
      { { 0 } },
      { { 491, 283, 0, 0, METER_FIXED, 1 }, { 491, 283, 0, 0, METER_SPEED, 0 }, { 490, 283, 0, 0, METER_FIXED, 0 },
        { 491, 284, 0, 0, METER_FIXED, 0 }, { 491, 283, 0, 0, METER_REV, 0 } } } },
  /* 5: two parts that are the lamp: the first one is (one lit sprite, not two) */
  { 5, 300, 6, { 0, 0, 3, 0, 0, 0 },
    { { { 0 } },
      { { 0 } },
      { { 400, 300, 0, 0, METER_FIXED, 0 }, { 491, 283, 0, 0, METER_FIXED, 0 }, { 491, 283, 0, 0, METER_FIXED, 0 } } } },
};

#endif
