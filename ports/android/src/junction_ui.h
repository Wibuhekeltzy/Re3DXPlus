/* Independent junction console. The caller owns scheduling and resource loading. */
#ifndef JUNCTION_UI_H
#define JUNCTION_UI_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
  int32_t left_area, left_kind, left_name_uv, right_area, right_kind, right_name_uv;
} junction_row;
typedef struct {
  junction_row rows[16];
  int state, timer, elapsed, active, visible, flash, initialized, panel_ready;
  int left_area, right_area, left_kind, right_kind, left_name, right_name;
} junction_ui;
/* Exact 384 bytes copied from the verified ELF VA 0x9332e20, little-endian. */
int junction_ui_init(junction_ui *ui, const void *table, size_t length);
void junction_ui_reset(junction_ui *ui);
/* Call once per game tick; paused freezes input and clocks. distance is passed
 * through the getter contract but unused here. suppress_flash is original arg 5. */
void junction_ui_tick(junction_ui *ui, int state, int left_area, int right_area,
                      float distance, int suppress_flash, int paused);
/* Draw using anm; missing panel resources fail closed, flash is optional.
 * Caller must establish GL_FUNC_ADD before drawing (anm uses caller GL state). */
int junction_ui_draw(const junction_ui *ui);
#endif
