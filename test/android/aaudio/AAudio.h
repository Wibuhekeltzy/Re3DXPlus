#ifndef STUB_AAUDIO_H
#define STUB_AAUDIO_H
#include <stdint.h>
typedef int32_t aaudio_result_t;
typedef int32_t aaudio_data_callback_result_t;
typedef int32_t aaudio_format_t;
typedef int32_t aaudio_stream_state_t;
typedef struct AAudioStreamStruct AAudioStream;
typedef struct AAudioStreamBuilderStruct AAudioStreamBuilder;
enum { AAUDIO_OK = 0, AAUDIO_ERROR_DISCONNECTED = -899 };
enum { AAUDIO_CALLBACK_RESULT_CONTINUE = 0 };
enum { AAUDIO_FORMAT_PCM_I16 = 1 };
enum { AAUDIO_PERFORMANCE_MODE_LOW_LATENCY = 12 };
enum { AAUDIO_USAGE_GAME = 14 };
enum { AAUDIO_STREAM_STATE_STARTED = 4 };
typedef aaudio_data_callback_result_t (*AAudioStream_dataCallback)(AAudioStream *, void *, void *, int32_t);
typedef void (*AAudioStream_errorCallback)(AAudioStream *, void *, aaudio_result_t);
aaudio_result_t AAudio_createStreamBuilder(AAudioStreamBuilder **);
void AAudioStreamBuilder_setFormat(AAudioStreamBuilder *, aaudio_format_t);
void AAudioStreamBuilder_setChannelCount(AAudioStreamBuilder *, int32_t);
void AAudioStreamBuilder_setSampleRate(AAudioStreamBuilder *, int32_t);
void AAudioStreamBuilder_setPerformanceMode(AAudioStreamBuilder *, int32_t);
void AAudioStreamBuilder_setUsage(AAudioStreamBuilder *, int32_t);
void AAudioStreamBuilder_setDataCallback(AAudioStreamBuilder *, AAudioStream_dataCallback, void *);
void AAudioStreamBuilder_setErrorCallback(AAudioStreamBuilder *, AAudioStream_errorCallback, void *);
aaudio_result_t AAudioStreamBuilder_openStream(AAudioStreamBuilder *, AAudioStream **);
aaudio_result_t AAudioStreamBuilder_delete(AAudioStreamBuilder *);
aaudio_result_t AAudioStream_requestStart(AAudioStream *);
aaudio_result_t AAudioStream_requestPause(AAudioStream *);
aaudio_result_t AAudioStream_close(AAudioStream *);
aaudio_stream_state_t AAudioStream_getState(AAudioStream *);
aaudio_format_t AAudioStream_getFormat(AAudioStream *);
int32_t AAudioStream_getChannelCount(AAudioStream *);
int32_t AAudioStream_getSampleRate(AAudioStream *);
#endif
