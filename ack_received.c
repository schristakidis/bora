#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "ack_received.h"

typedef struct AckStore {
  struct timeval received;
  struct timeval RTT;
  struct sockaddr_in from;
  // ADD ACK PAYLOAD
  struct AckStore * next;
} AckStore;

AckStore * ack_storage = NULL;

static pthread_mutex_t ack_list = PTHREAD_MUTEX_INITIALIZER;


int ack_received(Ack * ack_s, AckReceived * ack_r, struct timeval received, struct sockaddr_in from) {
  assert(ack_r != NULL);
  AckStore * ack_stor = malloc(sizeof(AckStore));
  if (ack_stor == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  ack_stor->from = from;
  ack_stor->received = received;
  timersub(&received, &ack_s->tv, &ack_stor->RTT);
  printf("RTT %ld %ld\n", ack_stor->RTT.tv_sec, ack_stor->RTT.tv_usec);
  pthread_mutex_lock(&ack_list);
  ack_stor->next = ack_storage;
  ack_storage = ack_stor;
  pthread_mutex_unlock(&ack_list);
  return 1;
}


