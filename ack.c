#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

#include "ack.h"
#include "netencoder.h"

static Ack * ackcache = NULL;

static pthread_mutex_t nack_list = PTHREAD_MUTEX_INITIALIZER;

uint16_t seq_num = 0;

int append_ack(SendData * d, struct timeval sendtime) {
  pthread_mutex_lock(&nack_list);
  int ret;
  Ack * ack;
  ack = (Ack*) malloc(sizeof(Ack));
  if (ack == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  ret = d->length;
  memcpy(d->data+ret, &seq_num, sizeof(uint16_t));
  ack->seq = seq_num;
  ack->tv = sendtime;
  ack->next = ackcache;
  seq_num++;
  ackcache = ack;
  pthread_mutex_unlock(&nack_list);
  return ret+ACKSIZE;
}

Ack * pop_ack(uint16_t seq) {
  //printf("ACK %i\n %p\nincache: %i\n", seq, ackcache, ackcache->seq);
  pthread_mutex_lock(&nack_list);
  if (!ackcache) {
    return NULL;
    pthread_mutex_unlock(&nack_list);
  }
  Ack * ret = ackcache;
  Ack * c = ackcache;
  Ack * p;
  if (ackcache->seq==seq) {
    ackcache = ackcache->next;
    pthread_mutex_unlock(&nack_list);
    return ret;
  }
  p = ackcache;
  c = ackcache->next;
  while (c) {
    if (c->seq==seq) {
      ret = c;
      p->next = c->next;
      pthread_mutex_unlock(&nack_list);
      return ret;
    }
    c = c->next;
    p = p->next;
  }
  pthread_mutex_unlock(&nack_list);
  return NULL;
}

uint16_t strip_ack(unsigned char * fragment, size_t fsize) {
  uint16_t ret;
  memcpy(&ret, fragment+fsize-ACKSIZE, ACKSIZE);
  return ret;
}

int get_n_nack(void) {
  int ret = 0;
  Ack * cur;
  pthread_mutex_lock(&nack_list);
  cur = ackcache;
  while (cur) {
    ret++;
    cur = cur->next;
  }
  pthread_mutex_unlock(&nack_list);
  return ret;
}
