
#ifndef STATS_BRIDGE_H
#define STATS_BRIDGE_H

#include <stdint.h>


#define NUM_STAT_R 7
#define I_PKG_COUNTER 0
#define I_DATA_COUNTER 1
#define I_ACK_DATA_COUNTER 2
#define I_ACK_COUNTER 3
#define I_GARBAGE 4
#define I_DUPE_COUNTER 5
#define I_DUPE_DATA_COUNTER 6

#define NUM_STAT_S 6
#define O_DATA_COUNTER 0
#define O_PKG_COUNTER 1
#define O_ACK_COUNTER 2
#define O_ACK_DATA_COUNTER 3
#define O_RETR_COUNTER 4
#define O_RETR_DATA_COUNTER 5

uint64_t stats_r[NUM_STAT_R];
uint64_t stats_s[NUM_STAT_S];

int reset_in_counters(void);
int reset_out_counters(void);


#endif
