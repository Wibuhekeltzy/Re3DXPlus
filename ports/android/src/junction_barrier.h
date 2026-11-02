/* Approximate visual candidate; never collision or route decisions. */
#ifndef JUNCTION_BARRIER_H
#define JUNCTION_BARRIER_H
typedef struct { float v[3]; } junction_barrier_point;
typedef struct { junction_barrier_point start, end; int kind, display_state; } junction_barrier_side;
typedef struct { junction_barrier_side left, right; int ready, manager_state; } junction_barrier;
/* Call reset first. Coordinates already share a caller-selected coordinate system;
 * no unit conversion is performed. On entry to state 1 set up (vec1,vec3),
 * (vec3,vec2). State 2/3 changes display_state, not the setup kind; other
 * states hide/reset. setup is not repeated while state 1 remains unchanged.
 * Invalid geometry, nonfinite distance, and missing prerequisites reset safely.
 * Returns 1 only if descriptors are available. This candidate does not implement
 * original update animation, physics, or timing. */
void junction_barrier_reset(junction_barrier *b);
int junction_barrier_tick(junction_barrier *b, int state, int left_kind, int right_kind,
                          const junction_barrier_point *vec1, const junction_barrier_point *vec2,
                          const junction_barrier_point *vec3, float distance);
#include "render.h"
/* Draws the frame sprite (arcade ID 0x569) only; arrows, flashing and fade
 * are intentionally absent. World endpoints are as passed to tick. world_vp
 * is the column-major projection*view matrix for that exact coordinate space.
 * native_to_world converts the original Lua plate height (85) into those units;
 * caller must obtain this factor from the same conversion used for RC vectors.
 * eye is world-space and controls far-to-near ordering. Call with the original
 * anm_load GL context current in the transparent pass, after opaque geometry;
 * Drawing selects texture unit 0, additive blend equation and vertex attributes
 * 0..2; render_sprites_by_colour leaves blend on, depth writes off, cull on.
 * Caller must reset pass state before subsequent draws. Does not prove
 * original winding/culling, alpha channel mask, pixel appearance or animation.
 * Borrowed texture is reacquired per call. Returns quads submitted (0 or 2). */
int junction_barrier_draw(const junction_barrier *b, const mat4 *world_vp,
                          const float eye[3], float native_to_world, render_params *p);
#endif
