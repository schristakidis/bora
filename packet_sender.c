#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/stat.h>
#else
#include <sys/socket.h>
#endif

#include <semaphore.h>

#include "packet_sender.h"
#include "messages.h"
#include "netencoder.h"
#include "stats_bridge.h"
#include "bpuller_bridge.h"

#define N_SEND 200
#define S_TRESHOLD 3
#define N_PRIO 200


//blockProduced = PTHREAD_COND_INITIALIZER;
//bpLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t produceBlock = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t pbLock = PTHREAD_MUTEX_INITIALIZER;


static struct timeval idleTime = (struct timeval){0};


static pthread_t sender_t;
static pthread_t puller_t;

SendData send_buf[N_SEND];
static pthread_mutex_t send_lock = PTHREAD_MUTEX_INITIALIZER;
int c_send = 0;
int f_send = 0;

SendData prio_buf[N_PRIO];
static pthread_mutex_t prio_lock = PTHREAD_MUTEX_INITIALIZER;
int c_prio = 0;
int f_prio = 0;

sem_t sFull;
sem_t qEmpty;


struct timeval packet_send(int s) {
  struct timeval t_start;
  struct timeval t_end;
  struct timeval ret;
  struct timeval t_idle;
  int c = 0;
  int l;
  SendData d;
  gettimeofday(&t_start, NULL);
  sem_wait(&sFull);
  pthread_mutex_lock(&prio_lock);
  if (f_prio) {
    d = prio_buf[(N_PRIO+c_prio-f_prio)%N_PRIO];
    f_prio--;
    c = 1;

    pthread_mutex_lock(&send_lock);
    if (f_send<S_TRESHOLD) {
      pthread_cond_signal(&produceBlock);
    }
    pthread_mutex_unlock(&send_lock);
  }
  pthread_mutex_unlock(&prio_lock);
  if (!c) {
    pthread_mutex_lock(&send_lock);
    d = send_buf[(N_SEND+c_send-f_send)%N_SEND];
    f_send--;
    if (f_send<S_TRESHOLD) {
      pthread_cond_signal(&produceBlock);
    }
    pthread_mutex_unlock(&send_lock);
  }
  gettimeofday(&t_end, NULL);
  if (d.data[0]&BLK_NEED_ACK) {
    d.length = append_ack(&d, t_end);
  }
  if (sendto(s, d.data, d.length, 0, (struct sockaddr*) &d.to, sizeof(d.to)) == -1) {
    perror("SEND FAILED");
  }
  free(d.data);
  l=d.length;
  sem_post(&qEmpty);
  timersub(&t_end, &t_start, &ret);
  pthread_mutex_lock(&stat_lock_s);
  t_idle = idleTime;
  timeradd(&ret, &t_idle, &idleTime);
  if (c) {
    stats_s[O_ACK_COUNTER]++;
    stats_s[O_ACK_DATA_COUNTER] += l;
  }
  stats_s[O_PKG_COUNTER]++;
  stats_s[O_DATA_COUNTER] += l;
  pthread_mutex_unlock(&stat_lock_s);
  return ret;
}

//SENDING THREAD
void * send_packet(void * sock) {
  int s = *(int*) sock;
  free(sock);
  for(;;) {
    usleep(500);
    packet_send(s);
  }
  return 0;
}

//PULLER THREAD
void * send_pull(void* args) {
  assert(args==NULL);
  for (;;) {
    pthread_mutex_lock(&pbLock);
    pthread_cond_wait(&produceBlock, &pbLock);
    pthread_mutex_unlock(&pbLock);

    block_pull();

    pthread_mutex_lock(&bpLock);
    pthread_cond_wait(&blockProduced, &bpLock);
    pthread_mutex_unlock(&bpLock);
  }
}

//PUBLIC FUNCTION
void send_data(SendData d) {
  sem_wait(&qEmpty);
  if (d.data[0] & BLK_ACK) {
    pthread_mutex_lock(&prio_lock);
    prio_buf[c_prio] = d;
    c_prio = (c_prio+1)%N_PRIO;
    f_prio++;
    pthread_mutex_unlock(&prio_lock);
  } else {
    pthread_mutex_lock(&send_lock);
    send_buf[c_send] = d;
    c_send = (c_send+1)%N_SEND;
    f_send++;
    pthread_mutex_unlock(&send_lock);
  }
  sem_post(&sFull);
}

void init_sender(int s) {
  void * sock = (void*) malloc(sizeof(int));
  if (sock == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  memcpy(sock, &s, sizeof(int));
  int t1, t2;
  memset(&stats_s, 0, sizeof(stats_s));
  pthread_mutex_init(&stat_lock_s, NULL);
  pthread_mutex_init(&bpLock, NULL);
  pthread_cond_init(&blockProduced, NULL);
  sem_init(&sFull, 0, 0);
  sem_init(&qEmpty, 0, N_SEND);
  t1 = pthread_create(&puller_t, NULL, send_pull, NULL);
  if (t1) {
    printf("ERROR; return code from pthread_create() is %d\n", t1);
    exit(EXIT_FAILURE);
  }
  t2 = pthread_create(&sender_t, NULL, send_packet, sock);
  if (t2) {
    printf("ERROR; return code from pthread_create() is %d\n", t2);
    exit(EXIT_FAILURE);
  }
}
