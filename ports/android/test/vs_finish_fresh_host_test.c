#include <time.h>
static double fake_now;
static int fake_clock(clockid_t id, struct timespec *t) {
  (void)id; t->tv_sec=(time_t)fake_now; t->tv_nsec=(long)((fake_now-t->tv_sec)*1000000000.0); return 0;
}
#define clock_gettime fake_clock
#include "../src/car.c"
#undef clock_gettime
#include <assert.h>
#include <stdlib.h>
uint8_t *rc_mem;
uint32_t rc_link_rx;
uint8_t *rc_va_slow(uint32_t a) { (void)a; abort(); }
static void count(unsigned char *p,uint32_t n) { for(int i=0;i<4;i++) p[0x34+i]=(unsigned char)(n>>(8*i)); }
int main(void) {
 unsigned char p[CAR_LINK_PACKET]={0}; rc_mem=calloc(1,0x4000); assert(rc_mem); rc_link_rx=RC_MEM_BASE;
 P.ready=1; P.vs_race=1; P.vs_pcb=0; P.vs[0].pcb=1; fake_now=100;
 assert(!car_link_observation_fresh(1,0)); car_link_deliver(1,NULL,sizeof p); car_link_deliver(1,p,sizeof p-1);
 p[0]=77; car_link_deliver(0,p,sizeof p); car_link_deliver(2,p,sizeof p);
 assert(rc_mem[0]==77 && rc_mem[2*LINK_BUF]==77 && !P.vs_received[0] && !P.vs_received[2] && !P.vs_received[1]);
 P.ready=0; p[0]=88; car_link_deliver(1,p,sizeof p);
 assert(rc_mem[LINK_BUF]==88 && !P.vs_received[1] && !P.vs_count_valid[1]); P.ready=1;
 count(p,0xffffffffu); car_link_deliver(1,p,sizeof p); double obs=P.vs_received[1]; assert(car_link_observation_fresh(1,obs));
 fake_now=100.5; car_link_deliver(1,p,sizeof p); assert(P.vs_received[1]==obs);
 fake_now=101; assert(car_link_observation_fresh(1,obs));
 fake_now=101.1; assert(!car_link_observation_fresh(1,obs)); count(p,0); car_link_deliver(1,p,sizeof p);
 assert(P.vs_received[1]>obs && !car_link_observation_fresh(1,obs)); count(p,0xffffffffu); car_link_deliver(1,p,sizeof p); assert(!P.vs_received[1]);
 count(p,1); fake_now=102; car_link_deliver(1,p,sizeof p); assert(car_link_observation_fresh(1,P.vs_received[1]));
 p[0x30]=42; car_link_deliver(1,p,sizeof p); assert(!P.vs_received[1]);
 count(p,2); car_link_deliver(1,p,sizeof p); assert(P.vs_received[1]==102);
 car_link_fresh(1,0); assert(!car_link_observation_fresh(1,102));
 car_link_car rival={0}; rival.pcb=1; car_link_io io={0}; car_set_versus(0,&rival,1,0,1,0,&io); assert(!P.vs_received[1] && !P.vs_race);
 car_link_deliver(1,p,sizeof p); assert(!P.vs_received[1]); P.vs_race=1; fake_now=103; car_link_deliver(1,p,sizeof p); assert(P.vs_received[1]==103);
 P.ready=0; fake_now=104; count(p,2); car_link_deliver(1,p,sizeof p); assert(P.vs_received[1]==103); free(rc_mem); return 0;
}
