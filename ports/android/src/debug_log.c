#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "debug_log.h"
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

/* Process lifetime storage avoids freeing memory underneath racing producers. */
typedef struct { uint64_t ns; char json[DEBUG_LOG_EVENT_MAX]; } event;
typedef struct { uint64_t sequence; event value; } queue_slot;
static struct {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    pthread_t thread;
    int state, fd, failed, enabled, sync_pending, sync_deferred;
    uint64_t enqueue, dequeue, producers, sync_target, sync_deferred_target;
    uint64_t dropped, oversized;
    queue_slot q[DEBUG_LOG_QUEUE_CAPACITY];
} g = { .mu = PTHREAD_MUTEX_INITIALIZER };
static pthread_once_t cond_once=PTHREAD_ONCE_INIT;
static int cond_error;
static void init_cond(void) {
#ifdef __APPLE__
    cond_error=pthread_cond_init(&g.cv,NULL);
#else
    pthread_condattr_t attr;
    cond_error=pthread_condattr_init(&attr);
    if (cond_error) return;
    cond_error=pthread_condattr_setclock(&attr,CLOCK_MONOTONIC);
    if (!cond_error) cond_error=pthread_cond_init(&g.cv,&attr);
    pthread_condattr_destroy(&attr);
#endif
}

enum { IDLE, ACTIVE, CLOSING };
static uint64_t now_ns(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    return (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
}
static void dropped(void) { __atomic_fetch_add(&g.dropped, 1, __ATOMIC_RELAXED); }

/* Strict bounded JSON recognizer. No allocation, nesting limited to 32. */
typedef struct { const unsigned char *p, *end; } parser;
static void ws(parser *s) { while (s->p < s->end && (*s->p==' ' || *s->p=='\t' || *s->p=='\r' || *s->p=='\n')) s->p++; }
static int hex(unsigned char c) { return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'); }
static int str(parser *s) {
    if (s->p==s->end || *s->p++!='"') return 0;
    while (s->p<s->end) {
        unsigned c=*s->p++;
        if (c=='"') return 1;
        if (c<0x20) return 0;
        if (c=='\\') {
            if (s->p==s->end) return 0;
            c=*s->p++;
            if (c=='u') { for (int i=0;i<4;i++) if (s->p==s->end || !hex(*s->p++)) return 0; }
            else if (!strchr("\"\\/bfnrt", (int)c)) return 0;
        } else if (c>=0x80) {
            unsigned n, cp, min;
            if (c>=0xc2 && c<=0xdf) { n=1; cp=c&31; min=0x80; }
            else if (c>=0xe0 && c<=0xef) { n=2; cp=c&15; min=0x800; }
            else if (c>=0xf0 && c<=0xf4) { n=3; cp=c&7; min=0x10000; }
            else return 0;
            while (n--) { if (s->p==s->end || (*s->p&0xc0)!=0x80) return 0; cp=(cp<<6)|(*s->p++&63); }
            if (cp<min || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return 0;
        }
    }
    return 0;
}
static int val(parser *s, unsigned depth);
static int compound(parser *s, unsigned depth, int object) {
    unsigned char end=object?'}':']';
    if (depth>32) return 0;
    s->p++; ws(s);
    if (s->p<s->end && *s->p==end) { s->p++; return 1; }
    for (;;) {
        if (object) { if (!str(s)) return 0; ws(s); if (s->p==s->end || *s->p++!=':') return 0; ws(s); }
        if (!val(s,depth+1)) return 0;
        ws(s); if (s->p==s->end) return 0;
        if (*s->p==end) { s->p++; return 1; }
        if (*s->p++!=',') return 0;
        ws(s);
    }
}
static int val(parser *s, unsigned depth) {
    if (s->p==s->end || depth>32) return 0;
    if (*s->p=='{') return compound(s,depth,1);
    if (*s->p=='[') return compound(s,depth,0);
    if (*s->p=='"') return str(s);
    const char *lit = *s->p=='t'?"true":*s->p=='f'?"false":*s->p=='n'?"null":NULL;
    if (lit) { size_t n=strlen(lit); if ((size_t)(s->end-s->p)<n || memcmp(s->p,lit,n)) return 0; s->p+=n; return 1; }
    if (*s->p=='-') s->p++;
    if (s->p==s->end) return 0;
    if (*s->p=='0') s->p++;
    else { if (*s->p<'1'||*s->p>'9') return 0; do { s->p++; } while (s->p<s->end && *s->p>='0' && *s->p<='9'); }
    if (s->p<s->end && *s->p=='.') { s->p++; if (s->p==s->end || *s->p<'0'||*s->p>'9') return 0; do { s->p++; } while (s->p<s->end && *s->p>='0' && *s->p<='9'); }
    if (s->p<s->end && (*s->p=='e'||*s->p=='E')) { s->p++; if (s->p<s->end && (*s->p=='+'||*s->p=='-')) s->p++; if (s->p==s->end || *s->p<'0'||*s->p>'9') return 0; do { s->p++; } while (s->p<s->end && *s->p>='0' && *s->p<='9'); }
    return 1;
}
static int valid(const char *p, size_t n) {
    parser s={(const unsigned char *)p,(const unsigned char *)p+n}; ws(&s);
    if (s.p==s.end || *s.p!='{' || !val(&s,0)) return 0;
    ws(&s); return s.p==s.end;
}
static void mark_failed(void) {
    __atomic_store_n(&g.failed,1,__ATOMIC_RELAXED);
    __atomic_store_n(&g.enabled,0,__ATOMIC_SEQ_CST);
}
int debug_log_enabled(void) {
    return __atomic_load_n(&g.enabled,__ATOMIC_ACQUIRE) &&
           !__atomic_load_n(&g.failed,__ATOMIC_RELAXED);
}
static void output(const char *s, size_t n) {
    if (__atomic_load_n(&g.failed,__ATOMIC_RELAXED)) return;
    while (n) {
        ssize_t k=write(g.fd,s,n);
        if (k<0 && errno==EINTR) continue;
        if (k<=0) { mark_failed(); return; }
        s+=k; n-=(size_t)k;
    }
}
static void sync_file(void) { if (!__atomic_load_n(&g.failed,__ATOMIC_RELAXED)) { int r; do { r=fsync(g.fd); } while (r<0 && errno==EINTR); if (r<0) mark_failed(); } }
int debug_log_failed(void) { return __atomic_load_n(&g.failed,__ATOMIC_RELAXED); }
static void gap(void) {
    uint64_t n=__atomic_exchange_n(&g.dropped,0,__ATOMIC_RELAXED);
    uint64_t oversized=__atomic_exchange_n(&g.oversized,0,__ATOMIC_RELAXED);
    if (n || oversized) { char b[200]; int z=snprintf(b,sizeof b,"{\"type\":\"gap\",\"monotonic_ns\":%" PRIu64 ",\"dropped\":%" PRIu64 ",\"oversized\":%" PRIu64 "}\n",now_ns(),n+oversized,oversized); output(b,(size_t)z); }
}
/* Bounded multi-producer/single-consumer ring. A reservation is published only
 * after its complete payload is copied; a stalled producer delays later slots,
 * never the game thread. No queue lock is held during filesystem I/O. */
#define RESERVATION_RETRIES 128
static int queue_push(const char *json, size_t n, uint64_t ns) {
    uint64_t pos=__atomic_load_n(&g.enqueue,__ATOMIC_RELAXED);
    for (unsigned attempt=0; attempt<RESERVATION_RETRIES; attempt++) {
        queue_slot *slot=&g.q[pos % DEBUG_LOG_QUEUE_CAPACITY];
        uint64_t seq=__atomic_load_n(&slot->sequence,__ATOMIC_ACQUIRE);
        int64_t diff=(int64_t)(seq-pos);
        if (diff==0) {
            if (__atomic_compare_exchange_n(&g.enqueue,&pos,pos+1,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED)) {
                slot->value.ns=ns;
                memcpy(slot->value.json,json,n+1);
                __atomic_store_n(&slot->sequence,pos+1,__ATOMIC_RELEASE);
                return 1;
            }
        } else if (diff<0) return 0;
        else pos=__atomic_load_n(&g.enqueue,__ATOMIC_RELAXED);
    }
    return 0;
}
static int queue_pop(event *out) {
    uint64_t pos=g.dequeue;
    queue_slot *slot=&g.q[pos % DEBUG_LOG_QUEUE_CAPACITY];
    if (__atomic_load_n(&slot->sequence,__ATOMIC_ACQUIRE)!=pos+1) return 0;
    out->ns=slot->value.ns;
    size_t n=strlen(slot->value.json);
    memcpy(out->json,slot->value.json,n+1);
    __atomic_store_n(&slot->sequence,pos+DEBUG_LOG_QUEUE_CAPACITY,__ATOMIC_RELEASE);
    g.dequeue=pos+1;
    return 1;
}
/* Preserve the oldest pending request; later requests coalesce only in the
 * deferred slot. The pair and its transfer are protected by the same mutex. */
static int take_sync_target(uint64_t *target) {
    pthread_mutex_lock(&g.mu);
    int present=g.sync_pending;
    if (present) {
        *target=g.sync_target;
        g.sync_pending=g.sync_deferred;
        if (g.sync_deferred) g.sync_target=g.sync_deferred_target;
        g.sync_deferred=0;
    }
    pthread_mutex_unlock(&g.mu);
    return present;
}
static void *writer(void *unused) {
    (void)unused;
    /* Wait until start has committed its successful state and error reset. */
    pthread_mutex_lock(&g.mu);
    pthread_mutex_unlock(&g.mu);
    char b[DEBUG_LOG_LINE_MAX];
    int z=snprintf(b,sizeof b,"{\"type\":\"session\",\"version\":1,\"monotonic_ns\":%" PRIu64 "}\n",now_ns());
    output(b,(size_t)z);
    uint64_t pending_target=0;
    int pending_sync=0;
    for (;;) {
        event e; int has=queue_pop(&e), end=0, wait_error=0;
        if (!has) {
            pthread_mutex_lock(&g.mu);
            if (g.state==CLOSING && !__atomic_load_n(&g.producers,__ATOMIC_SEQ_CST)) {
                has=queue_pop(&e);
                if (!has) end=1;
            } else {
                /* A 2ms monotonic/relative poll avoids lost notifications. */
#ifdef __APPLE__
                struct timespec relative={0,2000000};
                int r=pthread_cond_timedwait_relative_np(&g.cv,&g.mu,&relative);
#else
                struct timespec until;
                int r;
                if (clock_gettime(CLOCK_MONOTONIC,&until)) r=errno;
                else {
                    until.tv_nsec+=2000000;
                    if (until.tv_nsec>=1000000000) { until.tv_nsec-=1000000000; until.tv_sec++; }
                    r=pthread_cond_timedwait(&g.cv,&g.mu,&until);
                }
#endif
                if (r && r!=ETIMEDOUT) wait_error=1;
            }
            pthread_mutex_unlock(&g.mu);
        }
        if (wait_error) { mark_failed(); struct timespec pause={0,2000000}; nanosleep(&pause,NULL); }
        gap();
        if (has) { z=snprintf(b,sizeof b,"{\"type\":\"event\",\"monotonic_ns\":%" PRIu64 ",\"data\":%s}\n",e.ns,e.json); if (z>0 && (size_t)z<sizeof b) output(b,(size_t)z); else dropped(); }
        if (!pending_sync) pending_sync=take_sync_target(&pending_target);
        if (pending_sync && g.dequeue>=pending_target) {
            sync_file(); pending_sync=0;
        }
        if (end) { sync_file(); close(g.fd); return NULL; }
    }
}
int debug_log_start(int fd) {
    if (fd<0) return -1;
    if (pthread_once(&cond_once,init_cond) || cond_error) return -1;
    pthread_mutex_lock(&g.mu);
    if (g.state!=IDLE) { pthread_mutex_unlock(&g.mu); return -1; }
    g.fd=fd; g.dequeue=0;
    g.sync_pending=g.sync_deferred=0;
    g.sync_target=g.sync_deferred_target=0;
    __atomic_store_n(&g.enqueue,0,__ATOMIC_RELAXED);
    for (uint64_t i=0;i<DEBUG_LOG_QUEUE_CAPACITY;i++) __atomic_store_n(&g.q[i].sequence,i,__ATOMIC_RELAXED);
    __atomic_store_n(&g.dropped,0,__ATOMIC_RELAXED);
    __atomic_store_n(&g.oversized,0,__ATOMIC_RELAXED);
    g.state=ACTIVE;
    if (pthread_create(&g.thread,NULL,writer,NULL)) { g.state=IDLE; pthread_mutex_unlock(&g.mu); return -1; }
    __atomic_store_n(&g.failed,0,__ATOMIC_RELAXED);
    __atomic_store_n(&g.enabled,1,__ATOMIC_RELEASE);
    pthread_mutex_unlock(&g.mu); return 0;
}
void debug_log_emit(const char *fmt, ...) {
    if (!__atomic_load_n(&g.enabled,__ATOMIC_SEQ_CST)) return;
    __atomic_fetch_add(&g.producers,1,__ATOMIC_SEQ_CST);
    if (!__atomic_load_n(&g.enabled,__ATOMIC_SEQ_CST)) goto done;
    if (!fmt) { dropped(); goto done; }
    char b[DEBUG_LOG_EVENT_MAX]; va_list ap; va_start(ap,fmt); int n=vsnprintf(b,sizeof b,fmt,ap); va_end(ap);
    if (n>=0 && (size_t)n>=sizeof b) { __atomic_fetch_add(&g.oversized,1,__ATOMIC_RELAXED); goto done; }
    if (n<0 || memchr(b,'\n',(size_t)n) || !valid(b,(size_t)n)) { dropped(); goto done; }
    if (!queue_push(b,(size_t)n,now_ns())) dropped();
done:
    __atomic_fetch_sub(&g.producers,1,__ATOMIC_SEQ_CST);
}
void debug_log_flush(void) {
    if (!debug_log_enabled() || pthread_mutex_trylock(&g.mu)) return;
    if (g.state==ACTIVE && debug_log_enabled()) {
        uint64_t target=__atomic_load_n(&g.enqueue,__ATOMIC_ACQUIRE);
        if (!g.sync_pending) { g.sync_target=target; g.sync_pending=1; }
        else { g.sync_deferred_target=target; g.sync_deferred=1; }
        pthread_cond_signal(&g.cv);
    }
    pthread_mutex_unlock(&g.mu);
}
void debug_log_close(void) {
    pthread_mutex_lock(&g.mu);
    if (g.state!=ACTIVE) { pthread_mutex_unlock(&g.mu); return; }
    __atomic_store_n(&g.enabled,0,__ATOMIC_SEQ_CST);
    g.state=CLOSING; pthread_cond_signal(&g.cv); pthread_t t=g.thread;
    pthread_mutex_unlock(&g.mu);
    pthread_join(t,NULL);
    pthread_mutex_lock(&g.mu); g.state=IDLE; pthread_mutex_unlock(&g.mu);
}
void debug_log_before_abort(uint32_t address) {
    debug_log_emit("{\"kind\":\"panic\",\"address\":%" PRIu32 "}",address);
    debug_log_flush();
}
