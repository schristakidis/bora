#ifndef BORA_UTIL_H
#define BORA_UTIL_H

#include <sys/time.h>
#include <stdlib.h>
#include <stdint.h>

uint64_t time_to_usec(struct timeval t);

struct timeval time_from_usec(uint64_t usec);

struct timeval time_multiply(struct timeval t, unsigned long mult);

#endif
