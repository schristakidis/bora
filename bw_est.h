
#include <stdint.h>
#include <pthread.h>

#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/stat.h>
#else
#include <netinet/in.h>
#endif
#include <semaphore.h>

#define BW_EST_INTERVAL 100000 //microseconds


#define N_BW_EST 20

typedef struct BW_est {
  struct sockaddr_in host;
  struct timeval tv;
  uint16_t streamid;
  uint32_t blockid;
  uint8_t fid;
  uint8_t fragments;
} BW_est;

sem_t s_bw_est_full;
sem_t s_bw_est_empty;

void bw_est_init(void);
