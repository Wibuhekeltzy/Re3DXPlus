#ifndef STUB_NWJNI_H
#define STUB_NWJNI_H
#include <jni.h>
typedef struct ANativeWindow ANativeWindow;
ANativeWindow *ANativeWindow_fromSurface(JNIEnv *, jobject);
void ANativeWindow_release(ANativeWindow *);
int ANativeWindow_getWidth(ANativeWindow *);
int ANativeWindow_getHeight(ANativeWindow *);
#endif
