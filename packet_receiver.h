#ifndef PACKET_RECEIVER_H
#define PACKET_RECEIVER_H

#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/stat.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>

#define BUF_N 256

//uint64_t incomingPkgCounter;
//uint64_t incomingDataCounter;
//uint64_t incomingAckDataCounter;
//uint64_t incomingAckCounter;
//uint64_t incomingGarbage;
//uint64_t incomingDupeCounter;
//uint64_t incomingDupeDataCounter;


pthread_mutex_t stat_lock_r; // = PTHREAD_MUTEX_INITIALIZER;

typedef struct IncomingData {
  struct sockaddr_in from; //shall we cope with ipv6 and use sockaddr_storage??
  socklen_t fromlen;
  unsigned char buf[1500];
  size_t buflen;
  struct timeval tv;
} IncomingData;

void init_receiver(int s);
void receiver_end_threads(void);

#endif
