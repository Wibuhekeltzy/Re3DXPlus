#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static atomic_int block_writes, entered, inject_eintr, short_writes, inject_error;
static atomic_int test_sync_enabled, written_seq, first_sync_seq, sync_count, realtime_used, wait_observed, sync_reissue;
void debug_log_flush(void);
static int test_clock_gettime(clockid_t id, struct timespec *t) { if (id==CLOCK_REALTIME) atomic_store(&realtime_used,1); return clock_gettime(id,t); }
#ifdef __APPLE__
static int test_wait_relative(pthread_cond_t *c,pthread_mutex_t *m,const struct timespec *t) { assert(t->tv_sec==0 && t->tv_nsec==2000000); atomic_store(&wait_observed,1); return pthread_cond_timedwait_relative_np(c,m,t); }
#define pthread_cond_timedwait_relative_np test_wait_relative
#else
static int test_setclock(pthread_condattr_t *a,clockid_t id) { assert(id==CLOCK_MONOTONIC); atomic_store(&wait_observed,1); return pthread_condattr_setclock(a,id); }
#define pthread_condattr_setclock test_setclock
#endif
static int test_fsync(int fd) { if (atomic_load(&test_sync_enabled)) { if (atomic_fetch_add(&sync_count,1)==0) { atomic_store(&first_sync_seq,atomic_load(&written_seq)); if (atomic_exchange(&sync_reissue,0)) debug_log_flush(); } } return fsync(fd); }
static ssize_t test_write(int fd,const void *p,size_t n) {
    atomic_store(&entered,1);
    while (atomic_load(&block_writes)) { struct timespec t={0,1000000}; nanosleep(&t,0); }
    if (atomic_exchange(&inject_eintr,0)) { errno=EINTR; return -1; }
    if (atomic_load(&inject_error)) { errno=ENOSPC; return -1; }
    if (atomic_load(&short_writes) && n>3) n=3;
    ssize_t result=write(fd,p,n);
    if (result==(ssize_t)n && atomic_load(&test_sync_enabled) && n<512) { char b[512]; memcpy(b,p,n); b[n]=0; char *field=strstr(b,"\"seq\":"); int seq; if (field && sscanf(field,"\"seq\":%d",&seq)==1) atomic_store(&written_seq,seq); }
    return result;
}
#define write test_write
#define fsync test_fsync
#define clock_gettime test_clock_gettime
#include "../src/debug_log.c"
#undef write
#undef fsync
#undef clock_gettime

static int file(char *path) { strcpy(path,"/tmp/debug-log-test-XXXXXX"); int fd=mkstemp(path); assert(fd>=0); return fd; }
static char *contents(const char *path) {
    FILE *f=fopen(path,"rb"); assert(f); assert(!fseek(f,0,SEEK_END)); long n=ftell(f); assert(n>=0); rewind(f);
    char *s=calloc((size_t)n+1,1); assert(s); assert(fread(s,1,(size_t)n,f)==(size_t)n); fclose(f); return s;
}
static void wait_entered(void) { for (int i=0;i<2000 && !atomic_load(&entered);i++) { struct timespec t={0,1000000}; nanosleep(&t,0); } assert(atomic_load(&entered)); }
static void *producer(void *arg) { long id=(long)arg; for (int i=0;i<400;i++) debug_log_emit("{\"thread\":%ld,\"index\":%d}",id,i); return NULL; }
static void *closer(void *unused) { (void)unused; debug_log_close(); return NULL; }
int main(void) {
    assert(!debug_log_enabled());
    char path[80], other[80]; int fd=file(path), duplicate=file(other);
    const char header[]="{\"event\":\"session\",\"session_id\":\"abc\"}\n";
    assert(write(fd,header,sizeof header-1)==sizeof header-1);
    atomic_store(&block_writes,1); atomic_store(&entered,0);
    assert(debug_log_start(fd)==0); wait_entered(); assert(debug_log_enabled());
    assert(debug_log_start(duplicate)==-1); assert(fcntl(duplicate,F_GETFD)>=0); close(duplicate); unlink(other);
    for (unsigned i=0;i<DEBUG_LOG_QUEUE_CAPACITY+20;i++) debug_log_emit("{\"n\":%u}",i);
    debug_log_emit("{bad}"); debug_log_emit("{\"v\":%9000d}",1); debug_log_before_abort(123);
    atomic_store(&inject_eintr,1); atomic_store(&short_writes,1); atomic_store(&block_writes,0);
    debug_log_close(); assert(!debug_log_enabled()); assert(fcntl(fd,F_GETFD)==-1);
    char *s=contents(path); assert(!strncmp(s,header,sizeof header-1)); assert(strstr(s,"\"type\":\"session\"")); assert(strstr(s,"\"type\":\"gap\"")); assert(strstr(s,"\"oversized\":1"));
    char *p=s; for (unsigned i=0;i<DEBUG_LOG_QUEUE_CAPACITY;i++) { char needle[32]; snprintf(needle,sizeof needle,"\"n\":%u}",i); p=strstr(p,needle); assert(p); p+=strlen(needle); }
    free(s); unlink(path); atomic_store(&short_writes,0);
    fd=file(path); atomic_store(&entered,0); atomic_store(&block_writes,1); assert(debug_log_start(fd)==0); wait_entered();
    debug_log_emit("{\"v\":%5000d}",1);
    debug_log_before_abort(123); atomic_store(&block_writes,0);
    pthread_t t[4]; for (long i=0;i<4;i++) assert(!pthread_create(&t[i],0,producer,(void *)i));
    for (int i=0;i<4;i++) pthread_join(t[i],0); debug_log_flush(); debug_log_close();
    s=contents(path); assert(strstr(s,"\"type\":\"session\"")); assert(strstr(s,"\"kind\":\"panic\",\"address\":123"));
    unsigned long long lost=0; int events=0, last[4]={-1,-1,-1,-1};
    for (char *line=s; *line;) {
        char *end=strchr(line,'\n'); assert(end); assert(valid(line,(size_t)(end-line)));
        char *thread=strstr(line,"\"thread\":");
        if (thread && thread<end) { int id, index; assert(sscanf(thread,"\"thread\":%d,\"index\":%d",&id,&index)==2); assert(id>=0 && id<4 && index>last[id]); last[id]=index; events++; }
        char *gap_field=strstr(line,"\"dropped\":");
        if (gap_field && gap_field<end) { unsigned long long n; assert(sscanf(gap_field,"\"dropped\":%llu",&n)==1); lost+=n; }
        line=end+1;
    }
    assert((unsigned long long)events+lost==1600);
    assert(!debug_log_failed()); free(s); unlink(path);
    atomic_store(&inject_error,1); fd=file(path); assert(debug_log_start(fd)==0);
    for (int i=0;i<2000 && !debug_log_failed();i++) { struct timespec pause={0,1000000}; nanosleep(&pause,0); }
    assert(debug_log_failed()); assert(!debug_log_enabled()); debug_log_close(); assert(debug_log_failed()); assert(fcntl(fd,F_GETFD)==-1); unlink(path); atomic_store(&inject_error,0);
    fd=file(path); assert(debug_log_start(fd)==0); assert(debug_log_enabled()); assert(!debug_log_failed());
    for (long i=0;i<4;i++) assert(!pthread_create(&t[i],0,producer,(void *)i));
    debug_log_close(); for (int i=0;i<4;i++) pthread_join(t[i],0);
    assert(!debug_log_enabled()); unlink(path);
    /* Simulate a producer preempted between reservation and publication. */
    fd=file(path); assert(debug_log_start(fd)==0);
    __atomic_fetch_add(&g.producers,1,__ATOMIC_SEQ_CST);
    uint64_t pos=__atomic_fetch_add(&g.enqueue,1,__ATOMIC_RELAXED);
    queue_slot *slot=&g.q[pos%DEBUG_LOG_QUEUE_CAPACITY];
    assert(__atomic_load_n(&slot->sequence,__ATOMIC_ACQUIRE)==pos);
    pthread_t closing; assert(!pthread_create(&closing,0,closer,0));
    for (int i=0;i<2000 && debug_log_enabled();i++) { struct timespec pause={0,1000000}; nanosleep(&pause,0); }
    assert(!debug_log_enabled());
    slot->value.ns=now_ns(); strcpy(slot->value.json,"{\"reserved\":1}");
    __atomic_store_n(&slot->sequence,pos+1,__ATOMIC_RELEASE);
    __atomic_fetch_sub(&g.producers,1,__ATOMIC_SEQ_CST);
    pthread_join(closing,0); s=contents(path); assert(strstr(s,"\"reserved\":1")); free(s); unlink(path);
    /* Flush must reach its captured queue position, even with later backlog. */
    fd=file(path); atomic_store(&test_sync_enabled,1); atomic_store(&sync_count,0); atomic_store(&written_seq,0); atomic_store(&sync_reissue,1);
    atomic_store(&entered,0); atomic_store(&block_writes,1); assert(debug_log_start(fd)==0); wait_entered();
    for (int i=1;i<=10;i++) debug_log_emit("{\"seq\":%d}",i);
    pthread_mutex_lock(&g.mu); assert(!g.sync_pending); debug_log_flush(); assert(!g.sync_pending); pthread_mutex_unlock(&g.mu);
    debug_log_flush();
    for (int i=11;i<=20;i++) debug_log_emit("{\"seq\":%d}",i);
    debug_log_flush(); /* A and B both committed before the writer reads either. */
    pthread_mutex_lock(&g.mu); assert(g.sync_pending && g.sync_target==10 && g.sync_deferred && g.sync_deferred_target==20); pthread_mutex_unlock(&g.mu);
    atomic_store(&block_writes,0);
    for (int i=0;i<2000 && atomic_load(&sync_count)<2;i++) { struct timespec pause={0,1000000}; nanosleep(&pause,0); }
    assert(atomic_load(&sync_count)>=2); assert(atomic_load(&first_sync_seq)==10);
    debug_log_close(); atomic_store(&test_sync_enabled,0); unlink(path);
    /* B targets a reserved but unpublished slot; A must still fsync. */
    fd=file(path); atomic_store(&test_sync_enabled,1); atomic_store(&sync_count,0); atomic_store(&written_seq,0); atomic_store(&sync_reissue,0);
    atomic_store(&entered,0); atomic_store(&block_writes,1); assert(debug_log_start(fd)==0); wait_entered();
    for (int i=1;i<=10;i++) debug_log_emit("{\"seq\":%d}",i);
    debug_log_flush();
    __atomic_fetch_add(&g.producers,1,__ATOMIC_SEQ_CST);
    pos=__atomic_fetch_add(&g.enqueue,1,__ATOMIC_RELAXED); slot=&g.q[pos%DEBUG_LOG_QUEUE_CAPACITY];
    assert(__atomic_load_n(&slot->sequence,__ATOMIC_ACQUIRE)==pos);
    debug_log_flush(); atomic_store(&block_writes,0);
    for (int i=0;i<2000 && !atomic_load(&sync_count);i++) { struct timespec pause={0,1000000}; nanosleep(&pause,0); }
    assert(atomic_load(&sync_count)==1 && atomic_load(&first_sync_seq)==10);
    slot->value.ns=now_ns(); strcpy(slot->value.json,"{\"seq\":11}");
    __atomic_store_n(&slot->sequence,pos+1,__ATOMIC_RELEASE); __atomic_fetch_sub(&g.producers,1,__ATOMIC_SEQ_CST);
    for (int i=0;i<2000 && atomic_load(&sync_count)<2;i++) { struct timespec pause={0,1000000}; nanosleep(&pause,0); }
    assert(atomic_load(&sync_count)>=2); debug_log_close(); atomic_store(&test_sync_enabled,0); unlink(path);
    assert(!atomic_load(&realtime_used)); assert(atomic_load(&wait_observed));
    puts("debug_log host tests passed"); return 0;
}
