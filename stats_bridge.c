#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include "stats_bridge.h"
#include "packet_receiver.h"
#include "packet_sender.h"



int reset_in_counters(void) {
  memset(stats_r, 0, sizeof(stats_r));
  return 1;
}

int reset_out_counters(void) {
  memset(stats_s, 0, sizeof(stats_s));
  return 1;
}
