
#include <stdint.h>
#ifdef __WIN32__
#include <sys/stat.h>
#endif
#include <semaphore.h>
#include "biter_bridge.h"

static int c = 0;

void init_biter(void) {
  sem_init(&s_biter_full, 0, 0);
  sem_init(&s_biter_empty, 0, N_BITER);
}

void block_completed (uint16_t streamid, uint32_t blockid) {
  sem_wait(&s_biter_empty);
  b_biter_s[c] = streamid;
  b_biter_b[c] = blockid;
  sem_post(&s_biter_full);
  c = (c+1) % N_BITER;
}
