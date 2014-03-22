#ifndef ACK_RECEIVED_H
#define ACK_RECEIVED_H

#include <sys/time.h>
#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#define ACKSTORE_N 20

#include "ack.h"
#include "queue.h"

typedef struct AckReceived {
  unsigned char flags;
  uint16_t seq;
  int32_t sec;
  int32_t usec;
} AckReceived;

typedef struct AckStore {
  int has_data;
  struct sockaddr_in * addr;
  struct timeval sent;
  struct timeval RTT;
  struct timeval STT;
  uint16_t seq;
  uint32_t sleeptime;
} AckStore;

typedef struct PeerAckStore {
  struct sockaddr_in addr;
  struct timeval minSTT;
  struct timeval minRTT;
  struct timeval avgRTT;
  struct timeval avgSTT;
  struct timeval errRTT;
  struct timeval errSTT;
  struct AckStore ack_store[ACKSTORE_N];
  int cur;
  uint64_t total_acked;
  uint64_t total_errors;
  int last_acked;
  int last_error;
  SLIST_ENTRY(PeerAckStore) entries;
} PeerAckStore;

SLIST_HEAD(Ack_store, PeerAckStore);


int ack_received(Ack * ack_s, AckReceived * ack_r, struct timeval received, struct sockaddr_in from);

#endif
