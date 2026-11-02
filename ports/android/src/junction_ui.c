#include "junction_ui.h"
#include "anm.h"
#include <string.h>

/* menupack's UV_BASE namespace; resource loader must publish these cells. */
enum { UV_BASE = 102000 };
static uint32_t le32(const unsigned char *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
int junction_ui_init(junction_ui *u, const void *bytes, size_t length) {
  if (!u || !bytes || length != 16 * 24) return 0;
  junction_ui tmp; memset(&tmp, 0, sizeof tmp);
  const unsigned char *p = bytes;
  for (int i=0;i<16;i++) {
    junction_row *r=&tmp.rows[i];
    r->left_area=(int32_t)le32(p); r->left_kind=(int32_t)le32(p+4);
    r->left_name_uv=(int32_t)le32(p+8); r->right_area=(int32_t)le32(p+12);
    r->right_kind=(int32_t)le32(p+16); r->right_name_uv=(int32_t)le32(p+20); p+=24;
    if (r->left_kind<0 || r->left_kind>3 || r->right_kind<0 || r->right_kind>3 ||
        r->left_name_uv<0 || r->left_name_uv>=276 || r->right_name_uv<0 || r->right_name_uv>=276) return 0;
  }
  tmp.initialized=1; *u=tmp; return 1;
}
void junction_ui_reset(junction_ui *u) {
  if (!u) return;
  junction_row rows[16]; int initialized=u->initialized; memcpy(rows,u->rows,sizeof rows); memset(u,0,sizeof *u); memcpy(u->rows,rows,sizeof rows); u->initialized=initialized;
}
static void select_areas(junction_ui *u, int l, int r) {
  u->left_area=l; u->right_area=r; u->left_kind=1; u->right_kind=3; u->left_name=u->right_name=252;
  for (int i=0;i<16;i++) if (u->rows[i].left_area==l && u->rows[i].right_area==r) {
    u->left_kind=u->rows[i].left_kind; u->left_name=u->rows[i].left_name_uv;
    u->right_kind=u->rows[i].right_kind; u->right_name=u->rows[i].right_name_uv; break;
  }
}
void junction_ui_tick(junction_ui *u, int state, int l, int r, float distance, int suppress_flash, int paused) {
  (void)distance;
  if (!u || !u->initialized || paused) return;
  if (state<0 || state>3) { junction_ui_reset(u); return; }
  int changed=(u->state!=state);
  if (changed) {
    int previous=u->state;
    u->state=state;
    if (state==1) { u->active=1; u->visible=1; u->panel_ready=0; u->timer=80; u->elapsed=0; u->flash=0; }
    else if ((state==2 || state==3) && u->active && (previous==1 || u->panel_ready)) {
      /* Conservative port fallback: fast confirmation can arrive before the
       * 80-tick prompt completes. Resolve the current ordered pair now; original
       * early-confirmation rendering has not been dynamically verified. */
      if (previous==1) { select_areas(u,l,r); u->panel_ready=1; }
      u->visible=1; u->timer=75; u->elapsed=0; u->flash=!suppress_flash;
    } else if (state==0) {
      /* Port fallback: a bounded exit when the source supplies no close time.
       * A prompt without a resolved panel is hidden immediately. */
      u->active=0; u->flash=0;
      if (u->visible && u->panel_ready) u->timer=15;
      else { u->visible=0; u->timer=0; }
    } else { u->visible=0; u->flash=0; }
  }
  /* Do not retain a stale ordered pair if caller data changes mid-state. This
   * refresh is defensive port behavior, not a claim about arcade timing. */
  if (state>=1 && state<=3 && u->active && u->panel_ready &&
      (u->left_area!=l || u->right_area!=r)) select_areas(u,l,r);
  if (state==1 && u->timer>0) {
    if (--u->timer==0) { select_areas(u,l,r); u->panel_ready=1; u->elapsed=0; return; }
  } else if (state==2 || state==3 || state==0) {
    if (u->timer>0 && --u->timer==0) { u->active=0; u->visible=0; u->flash=0; }
  }
  /* Start at the original setAnime frame; update/draw ordering remains a port
   * assumption pending runtime comparison with the arcade. */
  if (u->visible && !changed && u->elapsed<1000000) ++u->elapsed;
}
static int pair(anm_swap *s, int from, int uv) {
  s->from=anm_sprite(from); s->to=anm_sprite(UV_BASE+uv); return s->from>=0 && s->to>=0;
}
int junction_ui_draw(const junction_ui *u) {
  if (!u || !u->visible) return 0;
  int a; anm_swap s[4];
  if (u->state==1 && u->timer>0) {
    a=anm_find("cons_g_bunki_mes_select");
    if (a<0 || !pair(s,0x1450,246) || anm_frames(a)<=0) return 0;
    anm_draw_ex(a,u->elapsed%anm_frames(a),1,0,0,s,1); return 1;
  }
  if (u->panel_ready && (u->state==1 || u->state==2 || u->state==3 || u->state==0)) {
    a=anm_find("cons_g_bunki_select");
    if (a<0 || anm_frames(a)<200 || !pair(s,0x144e,u->left_name) || !pair(s+1,0x179d,u->right_name) ||
        !pair(s+2,0xdc4,u->left_kind==1 ? 249 : 248) || !pair(s+3,0xdc6,u->right_kind==2 ? 250 : 251)) return 0;
    int frame=u->state==2 ? 60+u->elapsed : u->state==3 ? 160+u->elapsed : u->elapsed%60;
    if (u->state==2 && frame>159) frame=159;
    if (u->state==3 && frame>199) frame=199;
    int f=-1, ff=0;
    if (u->flash) {
      f=anm_find("cons_g_bunki_flash");
      if (f<0 || anm_frames(f)<200) f=-1; /* optional overlay: keep the route panel */
      else { ff=(u->state==2?1:101)+u->elapsed; if(ff>(u->state==2?100:199)) ff=u->state==2?100:199; }
    }
    anm_draw_ex(a,frame,1,0,0,s,4);
    if (f>=0) anm_draw(f,ff,1);
    return 1;
  }
  return 0;
}
