/* Standalone UDP protocol smoke test; run from repository root:
 * cc -std=c11 -Iports/android/src ports/android/test/net_protocol_v1_test.c \
 *   ports/android/src/net.c ports/android/src/vs_host.c -o /tmp/net_protocol_v1_test
 * /tmp/net_protocol_v1_test
 * Binds UDP 27015 locally. Does not exercise Android, GameKit, loss or adversarial traffic. */
#include "net.h"
#include "vs_host.h"
#include "car.h"
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

int gk_send(int peer, const void *b, int len) { (void)peer; (void)b; (void)len; return -1; }
int gk_recv(void *b, int max, int *peer) { (void)b; (void)max; (void)peer; return 0; }
static int delivered, delivered_pcb;
void car_link_deliver(int pcb, const unsigned char *p, int n) {
  assert(n == CAR_LINK_PACKET && p[0] == 0x5a);
  delivered++; delivered_pcb = pcb;
}
static void put32(unsigned char *p, unsigned v) {
  for (int i = 0; i < 4; i++) p[i] = (unsigned char)(v >> (8*i));
}
static void settle(void) { struct timeval tv = {0, 20000}; select(0, NULL, NULL, NULL, &tv); }
static int external_server;
static void host_poll(double now, const char *name) { settle(); if (!external_server) vs_host_poll(now, name); settle(); }
static int receive_kind(int fd, int kind) {
  unsigned char b[2048]; int n;
  while ((n = (int)recv(fd, b, sizeof b, 0)) > 0)
    if (n >= 4 && b[0] == 'W' && b[1] == 'V' && b[2] == 1 && b[3] == kind) return n;
  return 0;
}
int main(int argc, char **argv) {
  (void)argv; external_server = argc > 1;
  if (!external_server) assert(vs_host_open(0, 100) == 0);
  assert(net_open("127.0.0.1", NET_PORT, 100) == 0);
  int fd = socket(AF_INET, SOCK_DGRAM, 0); assert(fd >= 0);
  assert(fcntl(fd, F_SETFL, O_NONBLOCK) == 0);
  struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(NET_PORT);
  assert(inet_pton(AF_INET, "127.0.0.1", &a.sin_addr) == 1);
  assert(connect(fd, (struct sockaddr *)&a, sizeof a) == 0);
  net_poll(1, 0, "owner", NULL, 0); host_poll(1, "room"); net_poll(1.1, 0, "owner", NULL, 0);
  assert(net_lobby_now()->connected && net_lobby_now()->n == 1);
  unsigned char hello[26] = {'W','V',1,1}; put32(hello+4, 200); memcpy(hello+9, "guest", 5);
  assert(send(fd, hello, sizeof hello, 0) == sizeof hello);
  host_poll(1.2, "room"); net_poll(1.3, 0, "owner", NULL, 0);
  assert(net_lobby_now()->n == 2 && receive_kind(fd, 2));
  net_start(3, 13, 7); host_poll(1.4, "room"); net_poll(1.5, 1, "owner", NULL, 0);
  net_go go; assert(net_take_go(&go) && go.n == 2 && go.pcb == 0 && go.ramp == 3 && go.km == 13 && go.song == 7);
  assert(receive_kind(fd, 4));
  unsigned char race[9 + CAR_LINK_PACKET] = {'W','V',1,5}; put32(race+4, go.race); race[8] = 1; race[9] = 0x5a;
  assert(send(fd, race, sizeof race, 0) == sizeof race); host_poll(1.6, "room"); net_race_receive();
  assert(delivered == 1 && delivered_pcb == 1);
  unsigned char d2[4] = {'W','V',2,7};
  while (receive_kind(fd, 4)) {} /* drain duplicated GO */
  assert(send(fd, d2, sizeof d2, 0) == sizeof d2); host_poll(1.7, "room");
  unsigned char b[2048]; assert(recv(fd, b, sizeof b, 0) < 0);
  if (!external_server) {
    unsigned char d1[4] = {'W','V',1,7}; assert(send(fd, d1, sizeof d1, 0) == sizeof d1);
    host_poll(1.8, "room"); assert(receive_kind(fd, 8) == 25);
  }
  net_close(); close(fd); vs_host_close(); puts("WV1 UDP smoke test passed"); return 0;
}
