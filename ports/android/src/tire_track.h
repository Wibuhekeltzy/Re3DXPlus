#ifndef RE3DXPLUS_TIRE_TRACK_H
#define RE3DXPLUS_TIRE_TRACK_H
/* Header-only CPU geometry; no GL, vehicle state, or allocation.
 * Four FIFO rings, 15 pairs each. Matrix is column-major. Vertices are
 * T+h/u=1 then T-h/u=0 (original manager 0x85f856d–0x85f8602).
 * Alpha never decays and transparent pairs still grow.
 * Non-original conservative safety policy: reject non-finite data, alpha
 * outside [0,1], or input magnitudes above 1e6. Rejection breaks the strip;
 * strip_start tells a consumer never to connect across invalid input. Reset
 * clears rings but preserves enabled; enable clears and sets enabled.
 */
#include <math.h>
#include <stddef.h>
#include <string.h>
#define TIRE_TRACK_WHEELS 4
#define TIRE_TRACK_PAIRS 15
#define TIRE_TRACK_MAX_INPUT 1000000.0f
#define TIRE_TRACK_MAX_V 1.0e30f

typedef struct TireTrackVertex { float x, y, z, u, v, alpha; } TireTrackVertex;
typedef struct TireTrackPair { TireTrackVertex vertex[2]; unsigned char strip_start; } TireTrackPair;
typedef struct TireTrackWheel {
    TireTrackPair pairs[TIRE_TRACK_PAIRS];
    unsigned int head, count, segment_count;
    float v, center[3];
} TireTrackWheel;
typedef struct TireTrack { TireTrackWheel wheel[TIRE_TRACK_WHEELS]; int enabled; } TireTrack;
static inline void tire_track_init(TireTrack *t) { if (t) memset(t, 0, sizeof(*t)); }
static inline void tire_track_reset(TireTrack *t) { if (t) memset(t->wheel, 0, sizeof(t->wheel)); }
static inline void tire_track_enable(TireTrack *t, int enabled) {
    if (!t) return;
    tire_track_reset(t); t->enabled = !!enabled;
}
/* Index 0 is the oldest pair. NULL for invalid wheel/index. */
static inline const TireTrackPair *tire_track_pair(const TireTrack *t, unsigned int wheel, unsigned int index) {
    const TireTrackWheel *w;
    if (!t || wheel >= TIRE_TRACK_WHEELS) return NULL;
    w = &t->wheel[wheel];
    if (index >= w->count) return NULL;
    return &w->pairs[(w->head + index) % TIRE_TRACK_PAIRS];
}
/* Return 1 on append, 0 for disabled or unsafe input. Old data survives an
 * unsafe input, but the following pair starts a new strip. V is not clipped;
 * exceeding the generous V limit rejects the input and breaks continuity. */
static inline int tire_track_append(TireTrack *t, unsigned int wheel, const float m[16], float alpha, float width, float height) {
    TireTrackWheel *w; TireTrackPair p;
    double c[3], h[3], next_v, dx, dy, dz;
    unsigned int i, slot;
    if (!t || wheel >= TIRE_TRACK_WHEELS || !t->enabled) return 0;
    w = &t->wheel[wheel];
    if (!m || !isfinite(alpha) || alpha < 0.0f || alpha > 1.0f ||
        !isfinite(width) || !isfinite(height) ||
        fabsf(width) > TIRE_TRACK_MAX_INPUT || fabsf(height) > TIRE_TRACK_MAX_INPUT) goto invalid;
    for (i = 0; i < 16; ++i)
        if (!isfinite(m[i]) || fabsf(m[i]) > TIRE_TRACK_MAX_INPUT) goto invalid;
    c[0] = m[12]; c[1] = (double)m[13] - height; c[2] = m[14];
    for (i = 0; i < 3; ++i) h[i] = -0.5 * (double)width * m[i];
    next_v = w->v;
    if (w->segment_count >= 2) {
        dx = c[0] - w->center[0]; dy = c[1] - w->center[1]; dz = c[2] - w->center[2];
        next_v += sqrt(dx * dx + dy * dy + dz * dz) * 0.208333328366;
    }
    if (!isfinite(next_v) || next_v > TIRE_TRACK_MAX_V) goto invalid;
    memset(&p, 0, sizeof(p)); p.strip_start = (unsigned char)(w->segment_count == 0);
    p.vertex[0].x = (float)(c[0] + h[0]); p.vertex[0].y = (float)(c[1] + h[1]); p.vertex[0].z = (float)(c[2] + h[2]);
    p.vertex[1].x = (float)(c[0] - h[0]); p.vertex[1].y = (float)(c[1] - h[1]); p.vertex[1].z = (float)(c[2] - h[2]);
    p.vertex[0].u = 1.0f; p.vertex[1].u = 0.0f;
    p.vertex[0].v = p.vertex[1].v = (float)next_v;
    p.vertex[0].alpha = p.vertex[1].alpha = alpha;
    if (w->count == TIRE_TRACK_PAIRS) { slot = w->head; w->head = (w->head + 1) % TIRE_TRACK_PAIRS; }
    else { slot = (w->head + w->count) % TIRE_TRACK_PAIRS; ++w->count; }
    w->pairs[slot] = p; w->v = (float)next_v;
    for (i = 0; i < 3; ++i) w->center[i] = (float)c[i];
    if (w->segment_count < 2) ++w->segment_count;
    return 1;
invalid:
    w->segment_count = 0; w->v = 0.0f; return 0;
}
#endif
