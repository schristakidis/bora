#ifndef BWS_BRIDGE_H
#define BWS_BRIDGE_H

#if defined(_WIN32) || defined(_WIN64)
#include <sys/stat.h>
#endif
#include <semaphore.h>
#include <sys/time.h>


sem_t s_bws_hasdata;
sem_t s_bws_processed;

void init_bws(int interval);

void bws_return_value (int bw);
void set_bws_interval(int interval);

void bws_end_threads(void);

#endif
