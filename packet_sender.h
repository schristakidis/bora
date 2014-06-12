
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
  uint16_t port_n;
  struct sockaddr_in to;
} SendData;


void send_data(SendData d);
void init_sender(int s);
void set_bandwidth(int bw);
uint64_t get_idle(void);
uint16_t get_nat_port(void);
uint16_t set_nat_port(uint16_t port_n);

#endif
