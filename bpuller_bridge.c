
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#include <sys/stat.h>
#endif
#include <semaphore.h>

#include "bpuller_bridge.h"


void init_bpuller(void) {
  sem_init(&s_bpuller_full, 0, 0);
  sem_init(&s_bpuller_empty, 0, N_BPULLER);
}

void block_pull (void) {
  sem_wait(&s_bpuller_empty);
  sem_post(&s_bpuller_full);
}

