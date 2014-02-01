
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "bw_est.h"

#define N_ESTS 200

typedef struct BW_data {
  uint32_t blockid;
  uint8_t f;
  struct timeval tv;
  size_t fsize;
} BW_data;

typedef struct BW_host {
  int active;
  struct sockaddr_in host;
  struct BW_data last_BW_data;
  uint64_t best;
  uint64_t ests[N_ESTS];
  int c_ests;
  struct BW_host * next;
} BW_host;

static struct timeval bw_interval;

static BW_host * BW_EST = NULL;
static pthread_mutex_t lock_bw_est = PTHREAD_MUTEX_INITIALIZER;

void bw_est_init(void) {
  bw_interval.tv_sec = 0;
  bw_interval.tv_usec = BW_EST_INTERVAL;

  sem_init(&s_bw_est_full, 0, 0);
  sem_init(&s_bw_est_empty, 0, N_BW_EST);
}



void calc_bw(struct sockaddr_in host, uint32_t blockid, uint8_t f, struct timeval tv, size_t fsize) {
  BW_data this_bw;
  BW_host * my_host = NULL;
  struct timeval time_delta;
  this_bw.blockid = blockid;
  this_bw.f = f;
  this_bw.tv = tv;
  this_bw.fsize = fsize;

  pthread_mutex_lock(&lock_bw_est);
  my_host = BW_EST;
  while (my_host) {
    if (memcmp(&(my_host->host), &host, sizeof(struct sockaddr_in))==0) {
      break;
    }
    my_host = my_host->next;
  }
  if (my_host) {
    if (my_host->active) {
      if (my_host->last_BW_data.blockid == blockid && my_host->last_BW_data.f+1 == f) {
        timersub(&tv, &(my_host->last_BW_data.tv), &time_delta);
        my_host->ests[my_host->c_ests] = (uint64_t) (fsize*1000000)/(1000000*time_delta.tv_sec+time_delta.tv_usec);

        my_host->last_BW_data = this_bw;
        my_host->c_ests=(1+my_host->c_ests)%N_ESTS;
      } else {
        my_host->last_BW_data = this_bw;
      }
    } else {
      my_host->active = 1;
      my_host->last_BW_data = this_bw;
    }
  } else {
    my_host = (BW_host*) malloc(sizeof(BW_host));
    my_host->active = 1;
    my_host->best = 0;
    memset(&(my_host->ests), 0, sizeof(my_host->ests));
    my_host->c_ests = 0;
    my_host->host = host;
    my_host->last_BW_data = this_bw;
    my_host->next = BW_EST;
    BW_EST = my_host;
  }
  pthread_mutex_unlock(&lock_bw_est);
}
