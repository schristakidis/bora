#ifndef BITER_BRIDGE_H
#define BITER_BRIDGE_H

#ifdef __WIN32__
#include <sys/stat.h>
#endif
#include <semaphore.h>

#define N_BITER 20

sem_t s_biter_full;
sem_t s_biter_empty;
uint16_t b_biter_s[N_BITER];
uint32_t b_biter_b[N_BITER];

void init_biter(void);

void block_completed (uint16_t streamid, uint32_t blockid);

#endif
