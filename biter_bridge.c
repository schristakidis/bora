
#include <stdint.h>
#ifdef _WIN32 || _WIN64
#include <sys/stat.h>
#endif
#include <semaphore.h>
#include <string.h>
#include "biter_bridge.h"

static int c = 0;

void init_biter(void) {
  sem_init(&s_biter_full, 0, 0);
  sem_init(&s_biter_empty, 0, N_BITER);
}

void block_completed (int streamid, int blockid, IncomingData * data) {
  sem_wait(&s_biter_empty);
  b_biter_s[c] = streamid;
  b_biter_b[c] = blockid;
  if (data) {
    memcpy(&b_biter_d[c], data, sizeof(IncomingData));
  }
  sem_post(&s_biter_full);
  c = (c+1) % N_BITER;
}
