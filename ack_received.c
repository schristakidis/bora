#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>

#include "messages.h"
#include "cookie_sender.h"
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
struct timeval compute_average (struct Ack_values * ack_store, int trip) {
  struct timeval ret = (struct timeval) {0};
  int i = 0;
  AckStore * cur;
  int64_t sec = 0;
  int64_t usec = 0;
  SLIST_FOREACH(cur, ack_store, entries) {
    i++;
    if (trip==1) {
      sec += cur->STT.tv_sec;
      usec += cur->STT.tv_usec;
    } else if (trip ==2) {
      sec += cur->RTT.tv_sec;
      usec += cur->RTT.tv_usec;
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
  PeerAckStore * peer;
  AckStore * ack_store;
  struct timeval ack_recv_t = (struct timeval) {.tv_sec = (time_t)ack_r->sec, .tv_usec = (time_t)ack_r->usec};
  pthread_mutex_lock(&ack_list);
  peer = find_peer_by_host(&from);
  if (peer==NULL) {
    remove_ooo_nacks(ack_s);
    peer = (PeerAckStore*)malloc(sizeof(PeerAckStore));
    if (peer == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
    peer->addr = from;
    peer->ack_store = ((struct Ack_values){.slh_first = NULL});

    peer->cur = 0;
    ack_store = (AckStore*)malloc(sizeof(AckStore));
    if (ack_store == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
    ack_store->addr = &peer->addr;
    timersub(&ack_recv_t, &ack_s->sendtime, &ack_store->STT);
    timersub(&received, &ack_s->sendtime, &ack_store->RTT);
    ack_store->seq = ack_s->seq;
    ack_store->sent = ack_s->sendtime;
    ack_store->sleeptime = ack_s->sleeptime;
    peer->avgRTT = ack_store->RTT;
    peer->minRTT = ack_store->RTT;
    peer->errRTT = (struct timeval) {0};
    peer->avgSTT = ack_store->STT;
    peer->minSTT = ack_store->STT;
    peer->errSTT = (struct timeval) {0};
    peer->total_acked = 1;
    peer->last_acked = 1;
    peer->total_errors = 0;
    peer->last_error = 0;
    if (highest_seq!=NULL) {
        if (highest_seq->seq < ack_s->seq) {
            highest_seq = ack_store;
        }
    } else {
        highest_seq = ack_store;
    }
    SLIST_INSERT_HEAD(&peer->ack_store, ack_store, entries);
    SLIST_INSERT_HEAD(&peeracklist, peer, entries);
  } else {
  struct timeval prevRTT;
  struct timeval prevSTT;
  AckStore * prevackstore = SLIST_FIRST(&peer->ack_store);
    if (prevackstore) {
        prevRTT = prevackstore->RTT;
        prevSTT = prevackstore->STT;
    } else {
        timersub(&ack_recv_t, &ack_s->sendtime, &prevSTT);
        timersub(&received, &ack_s->sendtime, &prevRTT);
    }
    ack_store = (AckStore*)malloc(sizeof(AckStore));
    if (ack_store == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }

    ack_store->addr = &peer->addr;
    timersub(&ack_recv_t, &ack_s->sendtime, &ack_store->STT);
    timersub(&received, &ack_s->sendtime, &ack_store->RTT);
    ack_store->seq = ack_s->seq;
    ack_store->sent = ack_s->sendtime;
    ack_store->sleeptime = ack_s->sleeptime;
    if (timercmp(&ack_store->RTT, &peer->minRTT, <)) {
        peer->minRTT = ack_store->RTT;
    }

    if (timercmp(&ack_store->STT, &peer->minSTT, <)) {
        peer->minSTT = ack_store->STT;
    }

    SLIST_INSERT_HEAD(&peer->ack_store, ack_store, entries);

    peer->avgRTT = ack_store->RTT;//compute_average(&peer->ack_store, 2);
    peer->avgSTT = ack_store->STT;//compute_average(&peer->ack_store, 1);

    peer->total_acked += 1;
    peer->last_acked += 1;

#ifdef BORA_RETRANSMISSION
    int errors = resend_ooo_nacks(ack_s);
#else
    int errors = remove_ooo_nacks(ack_s);
#endif // BORA_RETRANSMISSION

    if (errors>0) {
      peer->errSTT = prevSTT;
      peer->errRTT = prevRTT;
      peer->total_errors += errors;
      peer->last_error += errors;
    }
    if (highest_seq!=NULL) {
        if (highest_seq->seq < ack_s->seq) {
            highest_seq = ack_store;
        }
    } else {
        highest_seq = ack_store;
    }

  }
  if (ack_s->d.data[0] == (NEED_ACK | COOKIE_MSG) ) {
    cookie_received(ack_store);
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
  highest_seq = NULL;
  pthread_mutex_unlock(&ack_list);
}
