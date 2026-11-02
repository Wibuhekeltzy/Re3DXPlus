#ifndef ANDROID_DEBUG_LOG_H
#define ANDROID_DEBUG_LOG_H
#include <stdint.h>
/* start takes ownership of a newly opened writable fd only on success. An
 * already active/closing session is rejected without closing the supplied fd.
 * emit takes a caller-authored constant printf format producing exactly one
 * UTF-8 JSON object without newline. Use numeric fields and fixed strings;
 * never interpolate names, IPs, URIs, user input or other untrusted strings.
 * Example: debug_log_emit("{\"frame\":%u,\"x\":%.3f}", frame, x);
 * Positions can disclose driving trajectories. printf types must match.
 * Events are monotonic-time JSONL envelopes; invalid/oversized events, queue
 * overflow and exhausted atomic reservation retries are counted in gap records. Gap
 * counts do not establish exact loss ordering; capture is not guaranteed.
 * emit and flush never wait for disk or the queue mutex. flush requests an
 * asynchronous queue-sequence barrier: after all slots reserved at its enqueue
 * snapshot have been written, writer calls fsync. The oldest pending target is
 * preserved; intermediate later requests can coalesce into the newest. The
 * request is best
 * effort: trylock contention drops it silently; retry on the next interval.
 * It reports no completion; process death or power loss can precede fsync. Call it
 * sparingly due to fsync cost. A producer stuck after reservation prevents the
 * barrier and close from completing. close can also block on disk I/O.
 * before_abort is ordinary-thread-only, attempts panic and async flush and
 * does not guarantee durability. No signal handler or rotation is provided.
 * Write/fsync errors are sticky and exposed by debug_log_failed(); reset at
 * the next successful start attempt. Output can end in a partial line. Stop
 * producers before close. start writes at the supplied current FD offset;
 * it neither seeks nor truncates, including when Java prewrites a header.
 * Oversize records are dropped, not truncated, and gap.oversized counts them.
 * enabled is an atomic nonblocking hint for skipping sampling/formatting; it
 * becomes false on close or write/fsync failure. It can change immediately
 * after the check and does not guarantee that a subsequent event is accepted.
 */
#define DEBUG_LOG_QUEUE_CAPACITY 256
#define DEBUG_LOG_EVENT_MAX 8192
#define DEBUG_LOG_LINE_MAX 8448
int debug_log_start(int fd);
void debug_log_emit(const char *fmt, ...);
void debug_log_flush(void);
void debug_log_close(void);
int debug_log_failed(void);
int debug_log_enabled(void);
void debug_log_before_abort(uint32_t address);
#endif
