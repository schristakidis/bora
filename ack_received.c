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

// trip: 1 = STT 2 = RTT
struct timeval compute_average (AckStore * ack_store, int trip) {
  struct timeval ret = (struct timeval) {0};
  int i;
  int64_t sec = 0;
  int64_t usec = 0;
  for (i=0;  i<ACKSTORE_N; i++) {
    if (trip == 1 && ack_store[i].has_data == 1) {
      sec += ack_store[i].STT.tv_sec;
      usec += ack_store[i].STT.tv_usec;
    } else if(trip == 2 && ack_store[i].has_data == 1) {
      sec += ack_store[i].RTT.tv_sec;
      usec += ack_store[i].RTT.tv_usec;
    }
  }
  if (i<1) {
    return ret;
  }
  ret.tv_sec = sec/i;
  ret.tv_usec = usec/i;
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
    peer->ack_store[peer->cur].has_data = 1;
    peer->ack_store[peer->cur].addr = &peer->addr;
    timersub(&ack_recv_t, &ack_s->sendtime, &peer->ack_store[0].STT);
    timersub(&received, &ack_s->sendtime, &peer->ack_store[0].RTT);
    peer->ack_store[peer->cur].seq = ack_s->seq;
    peer->ack_store[peer->cur].sent = ack_s->sendtime;
    peer->ack_store[peer->cur].sleeptime = ack_s->sleeptime;
    peer->avgRTT = peer->ack_store[peer->cur].RTT;
    peer->minRTT = peer->ack_store[peer->cur].RTT;
    peer->errRTT = (struct timeval) {0};
    peer->avgSTT = peer->ack_store[peer->cur].STT;
    peer->minSTT = peer->ack_store[peer->cur].STT;
    peer->errSTT = (struct timeval) {0};
    peer->total_acked = 1;
    peer->last_acked = 1;
    peer->total_errors = 0;
    peer->last_error = 0;
    if (highest_seq!=NULL) {
        if (highest_seq->seq < ack_s->seq) {
            highest_seq = &peer->ack_store[0];
        }
    } else {
        highest_seq = &peer->ack_store[0];
    }
    SLIST_INSERT_HEAD(&peeracklist, peer, entries);
  } else {
  struct timeval prevRTT;
  struct timeval prevSTT;
    if (peer->ack_store[peer->cur].has_data == 1) {
        prevRTT = peer->ack_store[peer->cur].RTT;
        prevSTT = peer->ack_store[peer->cur].STT;
    } else {
        timersub(&ack_recv_t, &ack_s->sendtime, &prevSTT);
        timersub(&received, &ack_s->sendtime, &prevRTT);
    }
    peer->cur = (peer->cur + 1) % ACKSTORE_N;
    peer->ack_store[peer->cur].has_data = 1;
    peer->ack_store[peer->cur].addr = &peer->addr;
    timersub(&ack_recv_t, &ack_s->sendtime, &peer->ack_store[0].STT);
    timersub(&received, &ack_s->sendtime, &peer->ack_store[0].RTT);
    peer->ack_store[peer->cur].seq = ack_s->seq;
    peer->ack_store[peer->cur].sent = ack_s->sendtime;
    peer->ack_store[peer->cur].sleeptime = ack_s->sleeptime;
    peer->avgRTT = compute_average(peer->ack_store, 2);
    if (timercmp(&peer->ack_store[peer->cur].RTT, &peer->minRTT, <)) {
        peer->minRTT = peer->ack_store[peer->cur].RTT;
    }
    peer->errRTT = (struct timeval) {0};
    peer->avgSTT = compute_average(peer->ack_store, 1);
    if (timercmp(&peer->ack_store[peer->cur].STT, &peer->minSTT, <)) {
        peer->minSTT = peer->ack_store[peer->cur].STT;
    }
    peer->errSTT = (struct timeval) {0};
    peer->total_acked += 1;
    peer->last_acked += 1;
    int errors = remove_ooo_nacks(ack_s);
    if (errors>0) {
      peer->errSTT = prevSTT;
      peer->errRTT = prevRTT;
      peer->total_errors += errors;
      peer->last_error += errors;
    }
    if (highest_seq!=NULL) {
        if (highest_seq->seq < ack_s->seq) {
            highest_seq = &peer->ack_store[peer->cur];
        }
    } else {
        highest_seq = &peer->ack_store[0];
    }

  }
  pthread_mutex_unlock(&ack_list);
  return 1;
}

struct PeerAckStats get_ack_store(void) {
  pthread_mutex_lock(&ack_list);
  return (struct PeerAckStats) {.peerstats = &peeracklist, .last_seq = highest_seq};
}

void release_ack_store(void) {
  PeerAckStore * cur;
  SLIST_FOREACH(cur, &peeracklist, entries) {
    cur->last_acked = 0;
    cur->last_error = 0;
  }
  pthread_mutex_unlock(&ack_list);
}
