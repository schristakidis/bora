#ifndef ACK_H
#define ACK_H
#include <unistd.h>
#include <pthread.h>

#include "blockcache.h"
#include "packet_sender.h"

#define ACKSIZE sizeof(uint16_t)


typedef struct Ack {
  uint16_t seq;
  struct timeval tv;
  struct Ack * next;
} Ack;

int append_ack(SendData * d, struct timeval sendtime);

uint16_t strip_ack(unsigned char * fragment, size_t fsize);

int get_n_nack(void);

Ack * pop_ack(uint16_t seq);

#endif
