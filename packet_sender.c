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

#include "bora_threads.h"
#include "packet_sender.h"
#include "messages.h"
#include "netencoder.h"
#include "stats_bridge.h"
#include "bpuller_bridge.h"
#include "cookie_sender.h"

#define S_TRESHOLD 3
#define N_SEND 2000
#define N_RETR 2000
#define N_PRIO 2000


//blockProduced = PTHREAD_COND_INITIALIZER;
//bpLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t produceBlock = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t pbLock = PTHREAD_MUTEX_INITIALIZER;


static struct timeval idleTime = (struct timeval){0};


static pthread_t sender_t;
static pthread_t puller_t;

static SendData send_buf[N_SEND];
static pthread_mutex_t send_lock = PTHREAD_MUTEX_INITIALIZER;
static int c_send = 0;
static int f_send = 0;

static SendData retr_buf[N_RETR];
static pthread_mutex_t retr_lock = PTHREAD_MUTEX_INITIALIZER;
static int c_retr = 0;
static int f_retr = 0;

static SendData prio_buf[N_PRIO];
static pthread_mutex_t prio_lock = PTHREAD_MUTEX_INITIALIZER;
static int c_prio = 0;
static int f_prio = 0;

static uint64_t bandwidth = 1000000;
static uint64_t sleeptime = 1500;

sem_t sFull;
sem_t qEmpty;

static struct sockaddr_in* lasthost = NULL;
static struct sockaddr_in* lhalloc;

static uint16_t natPort = 0;


struct timeval packet_send(int s) {
  struct timeval t_start;
  struct timeval t_end;
  struct timeval ret;
  struct timeval t_idle;
  int c = 0;
  int l;
  int z = 0;
  SendData d;
  gettimeofday(&t_start, NULL);

  pthread_mutex_lock(&send_lock);
  if (f_send<S_TRESHOLD) {
    pthread_cond_signal(&produceBlock);
  }
  pthread_mutex_unlock(&send_lock);

  sem_wait(&sFull);

  if (lasthost) {
    struct sockaddr_in* nexthost;
    pthread_mutex_lock(&send_lock);
    if (f_send>0) {
      nexthost = &send_buf[(N_SEND+c_send-f_send)%N_SEND].to;
      if ((nexthost->sin_port == lasthost->sin_port) && (memcmp(&nexthost->sin_addr, &lasthost->sin_addr, 4)==0)) {
        z = 1;
      }
    }
    pthread_mutex_unlock(&send_lock);
    if (z) {
        //puts("\nCONSECUTIVE!\n");
        goto send_data_packet;
    } else {
        lasthost = NULL;
    }
  }

  if (sem_trywait(&ckFull)==0) {
    d = *(get_cookie_data());
    goto send_d;
  }

  pthread_mutex_lock(&prio_lock);
  if (f_prio) {
    d = prio_buf[(N_PRIO+c_prio-f_prio)%N_PRIO];
    f_prio--;
    c = 1;
    //puts("SEND PRIO\n");
  }
  pthread_mutex_unlock(&prio_lock);

  if (!c) {
    pthread_mutex_lock(&retr_lock);
    if (f_retr) {
      d = retr_buf[(N_RETR+c_retr-f_retr)%N_RETR];
      f_retr--;
      c = 2;
      //puts("SEND RETR\n");
    }
    pthread_mutex_unlock(&retr_lock);
  }

  send_data_packet:
  if (!c) {
    pthread_mutex_lock(&send_lock);
    d = send_buf[(N_SEND+c_send-f_send)%N_SEND];
    f_send--;
    pthread_mutex_unlock(&send_lock);
    //puts("SEND DATA\n");
  }

  send_d:
  pthread_mutex_lock(&bwLock);
  sleeptime = (uint64_t)(1000000L * d.length / bandwidth);
  pthread_mutex_unlock(&bwLock);
  gettimeofday(&t_end, NULL);
  if (d.data[0] & NEED_ACK) {
    d.length = append_ack(&d, t_end, sleeptime);
  }
  if (z) {
    d.data[0]=d.data[0]|BLOCK_CONSECUTIVE;
  }

  /*append port*/
  if (d.data[0]) {
    memcpy(&d.data[d.length], &natPort, sizeof(uint16_t));
    d.length = d.length + sizeof(uint16_t);
  }

  if (sendto(s, d.data, d.length, 0, (struct sockaddr*) &d.to, sizeof(d.to)) == -1) {
    perror("SEND FAILED");
  } else {
    if (!c) {
      if (z) {
        lasthost = NULL;
      } else if (d.data[0] & BLK_BLOCK) {
        lasthost = lhalloc;
        memcpy(lasthost, &d.to, sizeof(struct sockaddr_in));
      }
    }
  }
  l=d.length;
  sem_post(&qEmpty);
  timersub(&t_end, &t_start, &ret);
  pthread_mutex_lock(&stat_lock_s);
  t_idle = idleTime;
  timeradd(&ret, &t_idle, &idleTime);
  if (c==1) {
    stats_s[O_ACK_COUNTER]++;
    stats_s[O_ACK_DATA_COUNTER] += l;
  } else if (c==2) {
    stats_s[O_RETR_COUNTER]++;
    stats_s[O_RETR_DATA_COUNTER] += l;
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
  for (;;) {
    if (kill_bora_threads) {
        break;
    }
    //puts("SEND_PACKET pre sleep\n");
    usleep(sleeptime);
    //puts("SEND_PACKET post sleep\n");
    packet_send(s);
    //puts("SEND_PACKET post send\n");

    pthread_mutex_lock(&send_lock);
    if (f_send<S_TRESHOLD) {
      pthread_cond_signal(&produceBlock);
      //puts("COND PRODUCEBLOCK");
    }
    pthread_mutex_unlock(&send_lock);
  }
  printf("SEND thread going out\n");
  return 0;
}

//PULLER THREAD
void * send_pull(void* args) {
  (void) (args);
  assert(args==NULL);
  for (;;) {
    if (kill_bora_threads) {
        break;
    }
    pthread_mutex_lock(&pbLock);
    pthread_cond_wait(&produceBlock, &pbLock);
    pthread_mutex_unlock(&pbLock);

    block_pull();

    pthread_mutex_lock(&bpLock);
    pthread_cond_wait(&blockProduced, &bpLock);
    pthread_mutex_unlock(&bpLock);
  }
  printf("SEND puller going out\n");
  return 0;
}

//PUBLIC FUNCTION
void send_data(SendData d) {
  if (!d.data[0]) {
    puts("IS THIS A MESSAGE?");
  }
  int w = sem_trywait(&qEmpty);
  if (w==-1) {
    puts ("ALL QUEUES FULL\n");
    return;
  }
  if (d.data[0] & BLK_ACK || d.data[0] & BW_MSG) {
    pthread_mutex_lock(&prio_lock);
    if (f_prio<N_PRIO) {
        prio_buf[c_prio] = d;
        c_prio = (c_prio+1)%N_PRIO;
        f_prio++;
        pthread_mutex_unlock(&prio_lock);
    } else {
        sem_post(&qEmpty);
        //puts("PRIO QUEUE FULL\n");
        pthread_mutex_unlock(&prio_lock);
        return;
    }
  } else if (d.data[0] & BLOCK_RETRANSMISSION) {
    pthread_mutex_lock(&retr_lock);
    if (f_retr<N_RETR) {
        retr_buf[c_retr] = d;
        c_retr = (c_retr+1)%N_RETR;
        f_retr++;
        pthread_mutex_unlock(&retr_lock);
    } else {
        sem_post(&qEmpty);
        //puts("RETR QUEUE FULL\n");
        pthread_mutex_unlock(&retr_lock);
        return;
    }
  } else {
    pthread_mutex_lock(&send_lock);
    if (f_send<N_SEND) {
        send_buf[c_send] = d;
        c_send = (c_send+1)%N_SEND;
        f_send++;
        pthread_mutex_unlock(&send_lock);
    } else {
        sem_post(&qEmpty);
        //puts("SEND QUEUE FULL\n");
        pthread_mutex_unlock(&send_lock);
        return;
    }
  }
  sem_post(&sFull);
}

void set_bandwidth(int bw) {
  bandwidth = (uint64_t) bw;
  //printf("bw %d", bandwidth);
  return;
}

void init_sender(int s) {
  void * sock = (void*) malloc(sizeof(int));
  if (sock == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  memcpy(sock, &s, sizeof(int));
  lhalloc = malloc(sizeof(struct sockaddr_in));
  if (lhalloc == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  int t1, t2;
  memset(&stats_s, 0, sizeof(stats_s));
  pthread_mutex_init(&stat_lock_s, NULL);
  pthread_mutex_init(&bpLock, NULL);
  pthread_mutex_init(&bwLock, NULL);
  pthread_cond_init(&blockProduced, NULL);
  sem_init(&sFull, 0, 0);
  sem_init(&qEmpty, 0, N_SEND*3);
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

uint64_t get_idle(void) {
  uint64_t ret = idleTime.tv_sec * 1000000L + idleTime.tv_usec;
  idleTime = (struct timeval) {0};
  return ret;
}

uint16_t get_nat_port(void) {
    return ntohs(natPort);
}

uint16_t set_nat_port(uint16_t port_n) {
    natPort = htons(port_n);
    return natPort;
}

void sender_end_threads(void) {
    sem_post(&sFull);
    sem_post(&qEmpty);
    sem_destroy(&sFull);
    sem_destroy(&qEmpty);
    //pthread_join(sender_t, NULL);
    pthread_cond_signal(&blockProduced);
    pthread_cond_signal(&produceBlock);
    //pthread_join(puller_t, NULL);
    pthread_mutex_destroy(&stat_lock_s);
    pthread_mutex_destroy(&bpLock);
    pthread_mutex_destroy(&bwLock);
    pthread_cond_destroy(&blockProduced);
    free(lhalloc);
}
