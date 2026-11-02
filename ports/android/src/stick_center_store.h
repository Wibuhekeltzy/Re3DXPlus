#ifndef STICK_CENTER_STORE_H
#define STICK_CENTER_STORE_H
/* Independent v1 sidecar; serialize writers. Unknown contents are never replaced. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
enum { STICK_STORE_OK, STICK_STORE_MISSING, STICK_STORE_UNKNOWN, STICK_STORE_IO, STICK_STORE_INVALID, STICK_STORE_EDIT_REQUIRED };
static inline int stick_store_load(const char *path, int *mode) {
  char b[64]; size_t n; FILE *f;
  if (!path || !*path || !mode) return STICK_STORE_INVALID;
  *mode=0;
  f=fopen(path,"rb"); if (!f) return errno==ENOENT ? STICK_STORE_MISSING : STICK_STORE_IO;
  n=fread(b,1,sizeof b,f); if (ferror(f)) { fclose(f); return STICK_STORE_IO; }
  if (fclose(f)) return STICK_STORE_IO;
  if (n==22 && !memcmp(b,"STICK_CENTER 1\nmode=0\n",22)) return STICK_STORE_OK;
  if (n==22 && !memcmp(b,"STICK_CENTER 1\nmode=1\n",22)) { *mode=1; return STICK_STORE_OK; }
  return STICK_STORE_UNKNOWN;
}
static inline int stick_store_save(const char *path, int mode, int edited) {
  int old, status, fd, ok; size_t n; char *tmp; FILE *f;
  if (!path || !*path || (mode!=0 && mode!=1)) return STICK_STORE_INVALID;
  if (edited!=1) return STICK_STORE_EDIT_REQUIRED;
  status=stick_store_load(path,&old);
  if (status!=STICK_STORE_OK && status!=STICK_STORE_MISSING) return status;
  n=strlen(path); if (n>(size_t)-1-12) return STICK_STORE_IO;
  tmp=(char *)malloc(n+12); if (!tmp) return STICK_STORE_IO;
  memcpy(tmp,path,n); memcpy(tmp+n,".tmp.XXXXXX",12);
  fd=mkstemp(tmp); if (fd<0) { free(tmp); return STICK_STORE_IO; }
  f=fdopen(fd,"wb"); if (!f) { close(fd); unlink(tmp); free(tmp); return STICK_STORE_IO; }
  ok=fprintf(f,"STICK_CENTER 1\nmode=%d\n",mode)>=0;
  if (ok && fflush(f)) ok=0;
  if (ok && fsync(fd)) ok=0;
  if (fclose(f)) ok=0;
  if (ok && rename(tmp,path)) ok=0;
  if (!ok) unlink(tmp);
  free(tmp); return ok ? STICK_STORE_OK : STICK_STORE_IO;
}
#endif
