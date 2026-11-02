#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int fail_rename(const char *a,const char *b) { (void)a; (void)b; return -1; }
#define rename fail_rename
#include "stick_center_store.h"
#undef rename
int main(int argc,char **argv) {
 (void)argc; FILE *f=fopen(argv[1],"wb"); assert(f);
 assert(fputs("STICK_CENTER 1\nmode=0\n",f)>=0); assert(!fclose(f));
 assert(stick_store_save(argv[1],1,1)==STICK_STORE_IO);
 int v=7; assert(stick_store_load(argv[1],&v)==STICK_STORE_OK && v==0);
 puts("stick center atomic failure test passed"); return 0;
}
