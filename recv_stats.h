#ifndef RECV_STATS_H
#define RECV_STATS_H

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/stat.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include "queue.h"

typedef struct RecvFragment {
  struct sockaddr_in from; //shall we cope with ipv6 and use sockaddr_storage??
  socklen_t fromlen;
  uint16_t streamid;
  uint32_t blockid;
  uint16_t fragmentid;
  size_t buflen;
  struct timeval tv;
  unsigned char flags;
} RecvFragment;

typedef struct BW {
  double bw;
  struct timeval tv;
  MYSLIST_ENTRY(BW) entries;
} BW;

MYSLIST_HEAD(bandwidths, BW);

typedef struct BWEstimation {
  struct sockaddr_in from;
  socklen_t fromlen;
  struct bandwidths bandwidth;
  RecvFragment lastfragment;
  MYSLIST_ENTRY(BWEstimation) entries;
} BWEstimation;

MYSLIST_HEAD(bwstruct, BWEstimation);

void init_recv_stats(void);
void fragment_received(RecvFragment fragment);
struct bwstruct fetch_bw_estimations (void);

#endif
