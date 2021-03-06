#ifndef COOKIE_SENDER_H
#define COOKIE_SENDER_H

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/stat.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdint.h>

#include "packet_sender.h"
#include "ack_received.h"

typedef struct CookieAck {
  struct sockaddr_in addr;
  struct timeval sent;
  struct timeval RTT;
  struct timeval STT;
  uint16_t seq;
  uint32_t sleeptime;
} CookieAck;

CookieAck ckResult[2];

sem_t ckFull;
sem_t ckEmpty;

void init_cksender (void);
SendData * get_cookie_data (void);
int cookie_received (AckStore * ack);
int send_cookie (struct sockaddr_in * addr1, struct sockaddr_in * addr2);
void cookie_cleanup (void);

#endif
