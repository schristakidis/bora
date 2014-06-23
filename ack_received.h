#ifndef ACK_RECEIVED_H
#define ACK_RECEIVED_H

#include <sys/time.h>
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#define ACKSTORE_N 1

#include "ack.h"
#include "queue.h"

typedef struct AckReceived {
  unsigned char flags;
  uint16_t port_n;
  uint16_t seq;
  int32_t sec;
  int32_t usec;
} AckReceived;

typedef struct AckStore {
  //int has_data;
  struct sockaddr_in * addr;
  uint16_t port_n;
  struct timeval sent;
  struct timeval RTT;
  struct timeval STT;
  uint16_t seq;
  uint32_t sleeptime;
  MYSLIST_ENTRY(AckStore) entries;
} AckStore;

MYSLIST_HEAD(Ack_values, AckStore);

typedef struct PeerAckStore {
  struct sockaddr_in addr;
  struct timeval minSTT;
  struct timeval minRTT;
  struct timeval avgRTT;
  struct timeval avgSTT;
  struct timeval errRTT;
  struct timeval errSTT;
  struct Ack_values ack_store;
  int cur;
  uint64_t total_acked;
  uint64_t total_errors;
  int last_acked;
  int last_error;
  MYSLIST_ENTRY(PeerAckStore) entries;
} PeerAckStore;

MYSLIST_HEAD(Ack_store, PeerAckStore);

typedef struct PeerAckStats {
  struct Ack_store * peerstats;
  struct AckStore * last_seq;
} PeerAckStats;


int ack_received(Ack * ack_s, AckReceived * ack_r, struct timeval received, struct sockaddr_in from);
struct PeerAckStats get_ack_store(void);
void release_ack_store(void);

#endif
