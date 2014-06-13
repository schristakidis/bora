#ifndef BITER_BRIDGE_H
#define BITER_BRIDGE_H

#ifdef __WIN32__
#include <sys/stat.h>
#endif
#include <semaphore.h>

#include "packet_receiver.h"

#define N_BITER 20

sem_t s_biter_full;
sem_t s_biter_empty;
int b_biter_s[N_BITER];
int b_biter_b[N_BITER];
IncomingData b_biter_d[N_BITER];


void init_biter(void);

void block_completed (int streamid, int blockid, IncomingData * data);

#endif
