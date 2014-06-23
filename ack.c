#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "ack.h"
#include "messages.h"
#include "netencoder.h"


typedef struct __attribute__((__packed__)) {
  //uint32_t sec;
  //uint32_t usec;
  uint16_t seq;
} _AckCookie;

static struct NackList nacklist = ((struct NackList){.slh_first = NULL});

static pthread_mutex_t nack_lock = PTHREAD_MUTEX_INITIALIZER;

static uint16_t seq_num = 0;

Nack_peer * nack_find_by_host(struct sockaddr_in * from) {
  Nack_peer * ret;
  MYSLIST_FOREACH(ret, &nacklist, entries) {
    if (memcmp(&ret->addr, from, sizeof(struct sockaddr_in)) == 0) {
       break;
    }
  }
  return ret;
}

int append_ack(SendData *d, struct timeval sendtime, uint32_t sleeptime) {
  Nack_peer * peer_acks;
  Ack * nack;
  int ret;
  pthread_mutex_lock(&nack_lock);
  seq_num++;
  peer_acks = nack_find_by_host(&d->to);
  if (peer_acks == NULL) {
    peer_acks = (Nack_peer*)malloc(sizeof(Nack_peer));
    if (peer_acks == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
    peer_acks->addr = d->to;
    MYSLIST_INIT(&peer_acks->nacks);
    MYSLIST_INSERT_HEAD(&nacklist, peer_acks, entries);
  }
  nack = (Ack*) malloc(sizeof(Ack));
  if (nack == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  nack->sendtime = sendtime;
  nack->sleeptime = sleeptime;
  nack->d = *d;
  nack->seq = seq_num;
  MYSLIST_INSERT_HEAD(&peer_acks->nacks, nack, entries);
  pthread_mutex_unlock(&nack_lock);
  _AckCookie cookie = (_AckCookie) {.seq = seq_num};//, .sec = htonl((uint32_t)sendtime.tv_sec), .usec = htonl((uint32_t)sendtime.tv_usec)};
  memcpy(&d->data[d->length], &cookie, sizeof(_AckCookie));
  ret = d->length + sizeof(_AckCookie);
  return ret;
}

Ack * pop_ack(uint16_t seq, struct sockaddr_in * from) {
  Ack * ret;
  Nack_peer * peer;
  pthread_mutex_lock(&nack_lock);
  peer = nack_find_by_host(from);
  if (peer == NULL) {
    pthread_mutex_unlock(&nack_lock);
    return NULL;
  }

  MYSLIST_FOREACH(ret, &peer->nacks, entries) {
    //printf("%i %i\n", ret->seq, seq);
    if (ret->seq == seq) {
        break;
    }
  }
  if (ret!=NULL) {
    MYSLIST_REMOVE(&peer->nacks, ret, Ack, entries);
  }
  pthread_mutex_unlock(&nack_lock);
  return ret;
}

AckCookie strip_ack(unsigned char * fragment, size_t fsize) {
  AckCookie ret;
  _AckCookie * acky = (_AckCookie*) (&fragment[ fsize - sizeof(_AckCookie)]);
  //uint16_t a,b,c;
  //a= fragment[ fsize - sizeof(_AckCookie)];
  //b= fragment[ fsize - sizeof(_AckCookie)-1];
  //c= fragment[ fsize - sizeof(_AckCookie)+1];
  //printf("VALUES: %d -- a %d, b %d, c %d", fsize - sizeof(_AckCookie), a, b, c);
  ret.seq = acky->seq;
  //ret.sendtime.tv_sec = ntohl(acky->sec);
  //ret.sendtime.tv_usec = ntohl(acky->usec);
  return ret;
}

int remove_ooo_nacks(Ack*ack) {
  int ret = 0;
  Nack_peer * peer;
  Ack * cur;
  Ack *tmp_cur;
  pthread_mutex_lock(&nack_lock);
  peer = nack_find_by_host(&ack->d.to);
  if (peer != NULL) {
    MYSLIST_FOREACH_SAFE(cur, &peer->nacks, entries, tmp_cur) {
      if (timercmp(&cur->sendtime, &ack->sendtime, <)) {
        ret++;
        MYSLIST_REMOVE(&peer->nacks, cur, Ack, entries);
        free(cur);
      }
    }
  }
  pthread_mutex_unlock(&nack_lock);
  return ret;
}

int resend_ooo_nacks(Ack*ack) {
  int ret = 0;
  Nack_peer * peer;
  Ack * cur;
  Ack *tmp_cur;
  pthread_mutex_lock(&nack_lock);
  peer = nack_find_by_host(&ack->d.to);
  if (peer != NULL) {
    MYSLIST_FOREACH_SAFE(cur, &peer->nacks, entries, tmp_cur) {
      if (timercmp(&cur->sendtime, &ack->sendtime, <)) {
        ret++;
        cur->d.data[0] &= BLOCK_MASK_CONSECUTIVE;
        if (cur->d.data[0]&(BLK_BLOCK_ACK)) {
            cur->d.data[0] |= BLOCK_RETRANSMISSION;
            send_data(cur->d);
        }
        MYSLIST_REMOVE(&peer->nacks, cur, Ack, entries);
        free(cur);
      }
    }
  }
  pthread_mutex_unlock(&nack_lock);
  return ret;
}

int get_n_nack(void) {
  int ret = 0;
  Nack_peer * n;
  Ack * a;
  pthread_mutex_lock(&nack_lock);
  MYSLIST_FOREACH(n, &nacklist, entries) {
    MYSLIST_FOREACH(a, &n->nacks, entries) {
      ret++;
    }
  }
  pthread_mutex_unlock(&nack_lock);
  return ret;
}

int get_seq(void) {
  return seq_num;
}
