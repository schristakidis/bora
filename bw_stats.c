
#include <stdint.h>
#ifdef __WIN32__
#include <sys/stat.h>
#endif
#include <sys/time.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "stats_bridge.h"
#include "packet_sender.h"
#include "ack_received.h"
#include "bw_stats.h"

static pthread_t bws_pusher;
static int bws_timer = 0;

void * bws_thread (void * args);

void init_bws(int interval) {
  int t1;
  sem_init(&s_bws_hasdata, 0, 0);
  sem_init(&s_bws_processed, 0, 0);
  bws_timer = interval;

  t1 = pthread_create(&bws_pusher, NULL, bws_thread, NULL);
  if (t1) {
    printf("ERROR; return code from pthread_create() is %d\n", t1);
    exit(EXIT_FAILURE);
  }
}

void * bws_thread (void * args) {
  assert(args==NULL);
  for (;;) {
    //puts("BWS THREAD PRE SLEEP\n");
    usleep(bws_timer);
    //puts("BWS THREAD POST SLEEP\n");
    pthread_mutex_lock(&bwLock);
    sem_post(&s_bws_hasdata);
    sem_wait(&s_bws_processed);
    pthread_mutex_unlock(&bwLock);
  }
  printf("BWS THREAD DOWN\n");
  return NULL;
}

void bws_return_value (int bw) {
    //puts("bws_return_value START");
    set_bandwidth(bw);
    reset_out_counters();
    release_ack_store();
    sem_post(&s_bws_processed);
    //puts("bws_return_value END");
}

void set_bws_interval(int interval) {
    bws_timer = interval;
}
