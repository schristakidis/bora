#ifndef BPULLER_BRIDGE_H
#define BPULLER_BRIDGE_H

#if defined(_WIN32) || defined(_WIN64)
#include <sys/stat.h>
#endif
#include <semaphore.h>

#define N_BPULLER 20

sem_t s_bpuller_full;
sem_t s_bpuller_empty;


void init_bpuller(void);

void block_pull (void);

#endif

