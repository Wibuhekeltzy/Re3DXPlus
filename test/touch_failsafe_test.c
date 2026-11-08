/* The Android main loop's touch failsafe (src/main_android.c: touch_enqueue, touch_take,
 * touch_deliver, touch_cancel_all): fingers in order; a queue that overflows, or that a pause or a
 * lost surface throws away, is "poisoned", and the game thread then cancels every finger it knows
 * (phase 3, at its last place) and ignores a finger it has not seen go down since; a poison that
 * arrives in the middle of a delivery drops the rest; the system's own cancel; a finger's id and
 * place as the game is told them.
 *
 * main_android.c is included whole for its statics; the Android headers it asks for are the empty
 * stubs of test/android (and the JDK's jni.h); app_touch is this test's own, writing down what the
 * game would be told. Built as the Android build builds it (the Vulkan path):
 *
 *   JI=$(dirname $(dirname $(readlink -f $(which javac))))/include
 *   gcc -std=gnu99 -Wall -Wno-unused-function -DRC_FPT=double -DWMMT_EGL -DWMMT_VK \
 *       -Itest/android -Itest -Isrc -I$JI -I$JI/linux test/touch_failsafe_test.c -lm -lpthread \
 *       -Wl,--unresolved-symbols=ignore-all -o touch_failsafe_test && ./touch_failsafe_test
 */
#include "../src/main_android.c"

static int fails;
#define CHECK(c) \
  do { \
    if (!(c)) { \
      printf("FAIL line %d: %s\n", __LINE__, #c); \
      ++fails; \
    } \
  } while (0)

typedef struct { int id, phase; float x, y; } call;
static call calls[4096];
static int ncalls;
static int poison_after; /* the UI thread poisons the queue as the game takes this many calls (0: never) */

void app_touch(void *id, int phase, float x, float y) {
  if (ncalls < 4096) calls[ncalls++] = (call){ (int)(intptr_t)id, phase, x, y };
  if (poison_after && ncalls == poison_after) A.n_touch = 0, A.touch_desync = 1;
}

static void down(int id, int phase, float x, float y) {
  pthread_mutex_lock(&A.mx);
  touch_enqueue((touch_ev){ id, phase, x, y });
  pthread_mutex_unlock(&A.mx);
}

/* what nativePause(true) and nativeSurface(null) do to the queue */
static void poison(void) {
  pthread_mutex_lock(&A.mx);
  A.n_touch = 0, A.touch_desync = 1;
  pthread_mutex_unlock(&A.mx);
}

/* the game thread's frame: the queue taken under the lock, then delivered */
static void frame(void) {
  touch_ev tv[TOUCH_MAX];
  int desync;
  pthread_mutex_lock(&A.mx);
  int n = touch_take(tv, &desync);
  pthread_mutex_unlock(&A.mx);
  touch_reconcile_snapshot(&n, &desync);
  touch_deliver(tv, n, desync);
}

static int active_count(void) {
  int n = 0;
  for (touch_active *p = touch_active_head; p; p = p->next) ++n;
  return n;
}

static void reset(void) {
  poison_after = 0;
  ncalls = 0;
  A.n_touch = A.touch_desync = 0;
  A.touch_k = 1.0f, A.touch_edge = 0;
  while (touch_active_head) {
    touch_active *p = touch_active_head;
    touch_active_head = p->next;
    free(p);
  }
  touch_strict = 0;
}

int main(void) {
  /* a finger's whole life reaches the game in order, its id one more (the game's ids are never 0), its place in the
   * game's layout (the screen's scale and edge) */
  reset();
  A.touch_k = 0.5f, A.touch_edge = 10.0f;
  down(0, 0, 100, 200);
  down(0, 1, 120, 220);
  down(0, 2, 130, 230);
  frame();
  CHECK(ncalls == 3);
  CHECK(calls[0].id == 1 && calls[0].phase == 0 && calls[0].x == 40 && calls[0].y == 100);
  CHECK(calls[1].id == 1 && calls[1].phase == 1 && calls[1].x == 50 && calls[1].y == 110);
  CHECK(calls[2].id == 1 && calls[2].phase == 2 && calls[2].x == 55 && calls[2].y == 115);
  CHECK(active_count() == 0 && touch_strict == 0);
  /* two fingers, interleaved, and a frame with nothing */
  reset();
  down(3, 0, 10, 10);
  down(7, 0, 20, 20);
  down(3, 1, 11, 11);
  frame();
  CHECK(ncalls == 3 && active_count() == 2);
  ncalls = 0;
  frame();
  CHECK(ncalls == 0 && active_count() == 2);
  down(3, 2, 12, 12);
  frame();
  CHECK(ncalls == 1 && active_count() == 1 && calls[0].id == 4);
  down(7, 2, 21, 21);
  frame();
  CHECK(active_count() == 0);

  /* a queue that is full takes TOUCH_MAX events; the one after it poisons it: all of them lost (not just the
   * last), the fingers that were down cancelled where they last were, and nothing more taken until the frame */
  reset();
  down(1, 0, 50, 60);
  down(2, 0, 70, 80);
  frame();
  ncalls = 0;
  for (int i = 0; i < TOUCH_MAX; ++i) down(1, 1, (float)i, (float)i);
  CHECK(A.n_touch == TOUCH_MAX && !A.touch_desync);
  down(1, 1, 999, 999);
  CHECK(A.n_touch == 0 && A.touch_desync == 1);
  down(2, 1, 5, 5); /* (more while it is poisoned: dropped) */
  down(2, 2, 5, 5);
  CHECK(A.n_touch == 0 && A.touch_desync == 1);
  frame();
  CHECK(ncalls == 2 && A.touch_desync == 0 && A.n_touch == 0);
  CHECK(calls[0].id == 2 && calls[0].phase == 3 && calls[0].x == 50 && calls[0].y == 60); /* (finger 1, oldest first) */
  CHECK(calls[1].id == 3 && calls[1].phase == 3 && calls[1].x == 70 && calls[1].y == 80);
  CHECK(active_count() == 0 && touch_strict == 1);
  /* a finger that went down before and is still down (nobody saw it lift) cannot come back: its moves and lift are
   * dropped; a finger that goes down after it does is followed again, to the end */
  ncalls = 0;
  down(1, 1, 52, 62);
  down(1, 2, 52, 62);
  down(9, 0, 300, 310);
  down(9, 1, 301, 311);
  frame();
  CHECK(ncalls == 2 && calls[0].id == 10 && calls[0].phase == 0 && calls[1].id == 10 && calls[1].phase == 1);
  CHECK(active_count() == 1);
  down(9, 2, 302, 312);
  frame();
  CHECK(ncalls == 3 && calls[2].phase == 2 && active_count() == 0);
  /* (the same id again, down once more, is a new finger) */
  ncalls = 0;
  down(1, 0, 5, 5);
  down(1, 2, 5, 5);
  frame();
  CHECK(ncalls == 2 && calls[0].phase == 0 && calls[1].phase == 2);

  /* a pause or a lost surface poisons the queue too (nativePause, nativeSurface): the fingers down are let go of
   * even with nothing queued, at the frame that sees it (or the wait loop's own cancel) */
  reset();
  down(4, 0, 40, 40);
  down(5, 0, 50, 50);
  frame();
  ncalls = 0;
  down(4, 1, 41, 41); /* (queued, but the pause comes first: lost) */
  poison();
  frame();
  CHECK(ncalls == 2 && calls[0].phase == 3 && calls[1].phase == 3 && calls[0].id == 5 && calls[1].id == 6);
  CHECK(calls[0].x == 40 && calls[0].y == 40); /* (the last place the game was told, not the lost move) */
  CHECK(active_count() == 0 && touch_strict == 1);
  /* the wait loop's call, with no frame to follow */
  reset();
  down(4, 0, 40, 40);
  frame();
  ncalls = 0;
  touch_cancel_all();
  CHECK(ncalls == 1 && calls[0].phase == 3 && active_count() == 0 && touch_strict == 1);
  touch_cancel_all(); /* (and again: nothing more to say) */
  CHECK(ncalls == 1);
  /* a poison with nothing down is quiet, and the strictness is on all the same */
  reset();
  poison();
  frame();
  CHECK(ncalls == 0 && touch_strict == 1);

  /* a poison that comes while the game is taking the events (the UI thread, between two of them) drops the rest and
   * lets go of what is down */
  reset();
  down(1, 0, 10, 10);
  down(1, 1, 11, 11);
  down(1, 1, 12, 12);
  down(1, 2, 13, 13);
  poison_after = 1; /* after the first call, the poison */
  {
    touch_ev tv[TOUCH_MAX];
    int desync;
    pthread_mutex_lock(&A.mx);
    int n = touch_take(tv, &desync);
    pthread_mutex_unlock(&A.mx);
    CHECK(n == 4 && !desync);
    touch_deliver(tv, n, desync);
  }
  CHECK(ncalls == 2 && calls[0].phase == 0 && calls[1].phase == 3 && active_count() == 0 && touch_strict == 1);
  CHECK(A.touch_desync == 0 && A.n_touch == 0); /* (the delivery took the poison with it: nothing left to say) */
  poison_after = 0;
  frame();
  CHECK(A.touch_desync == 0 && ncalls == 2);

  /* the cancel is told where the finger last was (a move kept), in the game's layout, and the take itself clears
   * the poison it reports */
  reset();
  A.touch_k = 0.5f, A.touch_edge = 10.0f;
  down(1, 0, 100, 200);
  down(1, 1, 140, 220);
  frame();
  ncalls = 0;
  poison();
  {
    touch_ev tv[TOUCH_MAX];
    int desync = 0;
    pthread_mutex_lock(&A.mx);
    int n = touch_take(tv, &desync);
    pthread_mutex_unlock(&A.mx);
    CHECK(n == 0 && desync == 1 && A.touch_desync == 0 && A.n_touch == 0);
    touch_deliver(tv, n, desync);
  }
  CHECK(ncalls == 1 && calls[0].phase == 3 && calls[0].x == 60 && calls[0].y == 110);

  /* a poison after the take but before the frame is drawn: the frame's own look finds it */
  reset();
  down(2, 0, 20, 20);
  frame();
  ncalls = 0;
  poison();
  int none = 0, poisoned = 0;
  touch_reconcile_snapshot(&none, &poisoned);
  CHECK(poisoned == 1 && A.touch_desync == 0);
  touch_cancel_all();
  CHECK(ncalls == 1 && calls[0].phase == 3);

  /* a second "down" for a finger that is down is passed on, not followed twice; the one lift ends it */
  reset();
  down(6, 0, 1, 1);
  down(6, 0, 2, 2);
  frame();
  CHECK(ncalls == 2 && active_count() == 1);
  down(6, 2, 3, 3);
  frame();
  CHECK(active_count() == 0);

  /* the system's own cancel (phase 3, from MainActivity) ends a finger like a lift, at its place */
  reset();
  down(8, 0, 80, 80);
  down(8, 3, 81, 81);
  frame();
  CHECK(ncalls == 2 && calls[1].phase == 3 && calls[1].x == 81 && active_count() == 0 && touch_strict == 0);
  /* (and a cancelled finger, lifting after, is not down any more: with the failsafe on, that is dropped) */
  reset();
  touch_strict = 1;
  down(8, 0, 80, 80);
  down(8, 3, 81, 81);
  down(8, 2, 81, 81);
  frame();
  CHECK(ncalls == 2 && calls[1].phase == 3 && active_count() == 0);

  /* a lot of fingers one after the other leaves nothing behind (the list is let go of) */
  reset();
  for (int i = 0; i < 1000; ++i) {
    down(i % 10, 0, (float)i, (float)i);
    down(i % 10, 2, (float)i, (float)i);
    frame();
  }
  CHECK(active_count() == 0 && ncalls == 2000);
  reset();
  printf(fails ? "touch failsafe: %d FAILED\n" : "touch failsafe: ok\n", fails);
  return fails != 0;
}
