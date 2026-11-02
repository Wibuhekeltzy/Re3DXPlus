#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/menu.h"
static struct { menu_state menu; char save_dir[1100]; } G;
#include "key_layout_save_under_test.inc"
int main(void) {
  char dir[]="/tmp/wmmt-key-XXXXXX"; assert(mkdtemp(dir)); strcpy(G.save_dir,dir);
  for(int g=0;g<3;g++) G.menu.key_k[g]=G.menu.key_base_k[g]=100;
  G.menu.key_base_dx[2]=1900; G.menu.key_dx[2]=100;
  G.menu.key_dx[1]=55; G.menu.key_layout_dirty=1u<<1;
  strcpy(G.save_dir,"/definitely-missing-wmmt-dir"); assert(!key_layout_save());
  assert(G.menu.key_layout_dirty==(1u<<1) && G.menu.key_base_dx[2]==1900);
  strcpy(G.save_dir,dir); assert(key_layout_save()); assert(!G.menu.key_layout_dirty && G.menu.key_base_dx[2]==1900);
  char p[1200]; snprintf(p,sizeof p,"%s/key_layout",dir); FILE *f=fopen(p,"r"); assert(f);
  int k[3],x[3],y[3]; assert(fscanf(f,"key_layout 1 %d %d %d %d %d %d %d %d %d",&k[0],&x[0],&y[0],&k[1],&x[1],&y[1],&k[2],&x[2],&y[2])==9); fclose(f);
  assert(x[1]==55 && x[2]==1900); unlink(p); rmdir(dir); puts("legacy group save ok");
}
