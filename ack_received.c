#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>

#include "ack_received.h"

//AckStore * ack_storage = NULL;

static pthread_mutex_t ack_list = PTHREAD_MUTEX_INITIALIZER;

static struct Ack_store peeracklist = ((struct Ack_store){.slh_first = NULL});

static AckStore * highest_seq = NULL;

PeerAckStore * find_peer_by_host(struct sockaddr_in * from) {
    PeerAckStore * ret;
    SLIST_FOREACH(ret, &peeracklist, entries) {
        if (memcmp(&ret->addr, from, sizeof(struct sockaddr_in)) == 0) {
            break;
        }
    }
    return ret;
}

int ack_received(Ack * ack_s, AckReceived * ack_r, struct timeval received, struct sockaddr_in from) {
  int i;
  PeerAckStore * peer;
  struct timeval ack_recv_t = (struct timeval) {.tv_sec = (time_t)ack_r->sec, .tv_usec = (time_t)ack_r->usec};
  pthread_mutex_lock(&ack_list);
  peer = find_peer_by_host(&from);
  if (peer==NULL) {
    peer = (PeerAckStore*)malloc(sizeof(PeerAckStore));
    if (peer == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
    peer->addr = from;
    for (i = 1; i<ACKSTORE_N; i++) {
        peer->ack_store[i].has_data = 0;
        peer->ack_store[i].addr = &peer->addr;
    }
    peer->cur = 0;
    peer->ack_store[0].has_data = 1;
    peer->ack_store[0].addr = &peer->addr;
    timersub(&ack_recv_t, &ack_s->sendtime, &peer->ack_store[0].STT);
    timersub(&received, &ack_s->sendtime, &peer->ack_store[0].RTT);
    peer->ack_store[0].seq = ack_s->seq;
    peer->ack_store[0].sent = ack_s->sendtime;
    peer->ack_store[0].sleeptime = ack_s->sleeptime;
    peer->avgRTT = peer->ack_store[0].RTT;
    peer->minRTT = peer->ack_store[0].RTT;
    peer->errRTT = (struct timeval) {0};
    peer->avgSTT = peer->ack_store[0].STT;
    peer->minSTT = peer->ack_store[0].STT;
    peer->errSTT = (struct timeval) {0};
    peer->total_acked = 1;
    peer->last_acked = 1;
    peer->total_errors = 0;
    peer->last_error = 0;
    if (highest_seq!=NULL) {
        if (highest_seq->seq < ack_s->seq) {
            highest_seq = &peer->ack_store[0];
        }
    }
    SLIST_INSERT_HEAD(&peeracklist, peer, entries);
  } else {

  }
  pthread_mutex_unlock(&ack_list);
  return 1;
}


