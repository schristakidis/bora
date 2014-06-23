#ifndef ACK_H
#define ACK_H
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/stat.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "queue.h"
#include "blockcache.h"
#include "packet_sender.h"

#define ACKSIZE (sizeof(uint16_t) )//+ sizeof(uint32_t) + sizeof(uint32_t))

typedef struct AckCookie {
  uint16_t seq;
  //struct timeval sendtime;
} AckCookie;

typedef struct Ack {
  uint16_t seq;
  struct timeval sendtime;
  uint32_t sleeptime;
  struct SendData d;
  MYSLIST_ENTRY(Ack) entries;
} Ack;

MYSLIST_HEAD(Nacks, Ack);

typedef struct Nack_peer {
    struct sockaddr_in addr;
    struct Nacks nacks;
    MYSLIST_ENTRY(Nack_peer) entries;
} Nack_peer;

MYSLIST_HEAD(NackList, Nack_peer);

int append_ack(SendData * d, struct timeval sendtime, uint32_t sleeptime);

AckCookie strip_ack(unsigned char * fragment, size_t fsize);

int get_n_nack(void);

Ack * pop_ack(uint16_t seq, struct sockaddr_in * from);

int remove_ooo_nacks(Ack*ack);
int resend_ooo_nacks(Ack*ack);

int get_seq(void);
#endif
