#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "queue.h"
#include "messages.h"
#include "recv_stats.h"

static struct bwstruct estimations;
static pthread_mutex_t bw_lock = PTHREAD_MUTEX_INITIALIZER;

void init_recv_stats(void) {
    SLIST_INIT(&estimations);
}

BWEstimation * bw_find_by_host(struct sockaddr_in from, socklen_t fromlen) {
  BWEstimation * ret;
  SLIST_FOREACH(ret, &estimations, entries) {
    if (ret != NULL) {
      if (ret->fromlen == fromlen) {
        if (memcmp(&ret->from, &from, fromlen) == 0) {
            break;
        }
      }
    }
  }
  return ret;
}

void fragment_received(RecvFragment fragment) {
  pthread_mutex_lock(&bw_lock);
  BWEstimation * host_bw;
  host_bw = bw_find_by_host(fragment.from, fragment.fromlen);
  if (host_bw == NULL) {
    host_bw = (BWEstimation *)malloc(sizeof(BWEstimation));
	if (host_bw == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
    host_bw->from = fragment.from;
    host_bw->fromlen = fragment.fromlen;
    SLIST_INIT(&host_bw->bandwidth);
    host_bw->lastfragment = fragment;
    SLIST_INSERT_HEAD(&estimations, host_bw, entries);
  } else {

    if (host_bw->lastfragment.streamid   == fragment.streamid &&
        host_bw->lastfragment.blockid    == fragment.blockid  &&
        host_bw->lastfragment.fragmentid +1 == fragment.fragmentid &&
        fragment.flags&BLOCK_CONSECUTIVE) {

        BW * band = (BW*)malloc(sizeof(BW));
        band->tv = fragment.tv;

        struct timeval delta;
        timersub(&fragment.tv, &host_bw->lastfragment.tv, &delta);
        double delta_t = delta.tv_sec + (delta.tv_usec / 1000000.0);
        //printf("DELTA_T %f\n", delta_t);

        band->bw = (fragment.buflen) / delta_t;

        SLIST_INSERT_HEAD(&host_bw->bandwidth, band, entries);
    }
    //printf("lastfragment.streamid %i %i fragment.streamid\nlastfragment.blockid %i %i fragment.blockid\nlastfragment.fragmentid %i %i fragment.fragmentid\n",
    //host_bw->lastfragment.streamid, fragment.streamid, host_bw->lastfragment.blockid, fragment.blockid, host_bw->lastfragment.fragmentid +1, fragment.fragmentid);

    host_bw->lastfragment = fragment;
  }
  pthread_mutex_unlock(&bw_lock);
}

struct bwstruct fetch_bw_estimations (void) {
  struct bwstruct ret;
  pthread_mutex_lock(&bw_lock);
  ret = estimations;
  SLIST_INIT(&estimations);
  pthread_mutex_unlock(&bw_lock);
  return ret;
}
