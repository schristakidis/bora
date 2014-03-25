
#ifndef PACKET_SENDER_H
#define PACKET_SENDER_H

#include <stdint.h>

#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

pthread_cond_t blockProduced; // = PTHREAD_COND_INITIALIZER;
pthread_mutex_t bpLock; // = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t bwLock; // = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stat_lock_s; // = PTHREAD_MUTEX_INITIALIZER;

typedef struct SendData {
  unsigned char data[1500];
  uint32_t length;
  struct sockaddr_in to;
  uint32_t sleeptime;
} SendData;


void send_data(SendData d);
void init_sender(int s);
void set_bandwidth(int bw);

#endif
