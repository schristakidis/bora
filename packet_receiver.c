#include <pthread.h>
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "bora_threads.h"
#include "packet_receiver.h"
#include "packet_sender.h"
#include "messages.h"
#include "ack.h"
#include "ack_received.h"
#include "netencoder.h"
#include "blockcache.h"
#include "biter_bridge.h"
#include "stats_bridge.h"
#include "recv_stats.h"
#include "bw_msgs.h"

//struct timeval prev_recv = {0};
//int prev_recv_len = 0;



static pthread_t receiver_t;
static pthread_t processor_t;

static IncomingData buffer[BUF_N];

static sem_t bEmpty;
static sem_t bFull;

void * packet_receiver(void * socket) {
  int s = *(int*) socket;
  uint8_t c = 0;
  int l;
  for (;;) {
    if (kill_bora_threads) {
        break;
    }
    sem_wait(&bEmpty);
    buffer[c].fromlen = sizeof(struct sockaddr_in);
    l = recvfrom(s, &buffer[c].buf, 1500, 0, (struct sockaddr*)&buffer[c].from, &buffer[c].fromlen);
    if (l==-1) {
      perror("RECV ERROR");
      break;
    }
    gettimeofday(&buffer[c].tv, NULL);
    buffer[c].buflen = l;
    //printf("RECEIVED: 0x%x %i\n", buffer[c].buf[0], (int)buffer[c].buflen);
    c = (BUF_N+1+c)%BUF_N;
    sem_post(&bFull);
  }
  printf("Packet receiver processor going out\n");
  pthread_exit(0);
  return (void*) 0;
}

void * packet_processor(void*args) {
  (void)(args);
  assert(args==NULL);
  uint8_t c = 0;
  //uint16_t port_n;
  AckCookie cookie;
  for (;;) {
    if (kill_bora_threads) {
        break;
    }
    sem_wait(&bFull);
    pthread_mutex_lock(&stat_lock_r);
    if (buffer[c].buflen>1) {
      if (!buffer[c].buf[0]) {
        if (buffer[c].buflen>3) {
          block_completed(-1, -1, &buffer[c]);
        }
      } else {

        buffer[c].buflen = buffer[c].buflen - sizeof(uint16_t);

        if ( (uint16_t) *(&buffer[c].buf[buffer[c].buflen]) != 0 ) {
          buffer[c].from.sin_port = (uint16_t) *(&buffer[c].buf[buffer[c].buflen]);
          buffer[c].buflen = buffer[c].buflen - sizeof(uint16_t);
        }

      }
      // IF ACK IS REQUIRED SEND ACK IMMEDIATELY
      if (buffer[c].buf[0] & NEED_ACK) {
        if (validate_block(&buffer[c].buf[0], buffer[c].buflen-ACKSIZE)) {
          // GET seq for ACK request
          cookie = strip_ack(buffer[c].buf, buffer[c].buflen);
          //printf("COOKIE %i\n", cookie.seq);
          // REDUCE buflen
          buffer[c].buflen -= ACKSIZE;
          stats_r[I_DATA_COUNTER] += ACKSIZE;
          // MAKE ACKnowledge packet
          SendData s;
          s = encode_ack(cookie.seq);
          FragmentID fdata = get_fragment_id(&buffer[c].buf[0], buffer[c].buflen);
          append_ack_cons(&s, get_consecutives(&fdata));
          append_ack_ts(&s, &buffer[c].tv);
          s.to = buffer[c].from;
          send_data(s);

        } else {
          //BUFFER IS NOT FILLED bad packet!?!
          buffer[c].buf[0] = 0x00;
          stats_r[I_GARBAGE] += buffer[c].buflen;
          puts("NO GOOD");
        }
      }
      // IF PACKET IS AN ACK POP FROM ACK, GIVE TO ACK RECEIVER AND FREE
      if (buffer[c].buf[0] & BLK_ACK) {
        if (validate_ack(&buffer[c].buf[0], buffer[c].buflen)) {
          stats_r[I_ACK_COUNTER]++;
          stats_r[I_ACK_DATA_COUNTER] += buffer[c].buflen;
          AckReceived * ack_r = decode_ack(&buffer[c].buf[0], buffer[c].buflen);
          Ack * pop = pop_ack(ack_r->seq, &buffer[c].from);
          if (pop) {
            ack_received(pop, ack_r, buffer[c].tv, buffer[c].from);
            free(pop);
            free(ack_r);
          } else {
            free(ack_r);
            stats_r[I_GARBAGE] += buffer[c].buflen;
            stats_r[I_BAD_ACK_COUNTER]++;
            // puts("BAD ACK\n*************\n");
          }
        } else {
          //BUFFER IS NOT FILLED bad packet!?!
          stats_r[I_GARBAGE] += buffer[c].buflen;
          buffer[c].buf[0] = 0x00;
          puts("NO GOOD #2");
        }

      }

      if (buffer[c].buf[0] & BW_MSG) {
        if (validate_bw(&buffer[c].buf[0], buffer[c].buflen)) {
            BWMsg * bw_message = decode_bwmsg(&buffer[c].buf[0], buffer[c].buflen);
            if (bw_message) {
                bw_message->addr = buffer[c].from;
                bw_message->recv_time = buffer[c].tv;
                bwmsg_received(bw_message);
            } else {
                buffer[c].buf[0] = 0x00;
                stats_r[I_GARBAGE] += buffer[c].buflen;
            }
        } else {
            buffer[c].buf[0] = 0x00;
            stats_r[I_GARBAGE] += buffer[c].buflen;
        }
      }

      // IF PACKET CONTAINS BLOCK FRAGMENTS PROCESS IT AND SEND TO BLOCK CACHE
      if (buffer[c].buf[0] & BLK_BLOCK) {
        if (validate_block(&buffer[c].buf[0], buffer[c].buflen)) {
          FragmentData * fragment = decode_fragment(&buffer[c].buf[0], buffer[c].buflen);
          if (fragment) {
            switch (addfragment(fragment, buffer[c].from, buffer[c].tv)) {
              case F_DUPLICATE:
                stats_r[I_DUPE_COUNTER]++;
                stats_r[I_DUPE_DATA_COUNTER] += buffer[c].buflen;
                //puts("DUPLICATE FRAGMENT\n");
                break;
              case F_FRAGMENTID_OUTOFBOUNDS:
                stats_r[I_GARBAGE] += buffer[c].buflen;
                puts("OUTOFBOUNDSSS\n");
                break;
              case F_FRAGMENTS_MISMATCH:
                stats_r[I_GARBAGE] += buffer[c].buflen;
                puts("MISMATCH\n");
                break;
              case F_BAD_LEN:
                stats_r[I_GARBAGE] += buffer[c].buflen;
                puts("BAD LEN\n");
                break;
              case F_ADDED:
                //puts("FADDED");
                fragment_received((RecvFragment) {.from = buffer[c].from, .fromlen = buffer[c].fromlen,
                                                  .streamid = fragment->streamid, .blockid = fragment->blockid, .fragmentid = fragment->fragmentid,
                                                  .buflen = buffer[c].buflen, .tv = buffer[c].tv, .flags=buffer[c].buf[0]});
                if(iscomplete(fragment->streamid, fragment->blockid)) {
                block_completed(fragment->streamid, fragment->blockid, NULL);}
                break;
            }
            free(fragment->data);
            free(fragment);
          } else {
            buffer[c].buf[0] = 0x00;
            stats_r[I_GARBAGE] += buffer[c].buflen;
          }
        } else {
          //BUFFER IS NOT FILLED bad packet!?!
          buffer[c].buf[0] = 0x00;
          stats_r[I_GARBAGE] += buffer[c].buflen;
          puts("NO GOOD #3");
        }
      }
    }
    //if (prev_recv_len) {
    //  struct timeval delta;
    //  timersub(&buffer[c].tv, &prev_recv, &delta);
    //  int delta_t = (delta.tv_sec * 1000000) + delta.tv_usec;
    //  printf("D %i %i\n", delta_t, (int)buffer[c].buflen);
    //  //printf("%i\n\n\n",  ( ( (int)buffer[c].buflen) /delta_t)*1000000 );
    //
    //}
    //prev_recv_len = buffer[c].buflen;
    //prev_recv = buffer[c].tv;
    stats_r[I_DATA_COUNTER] += buffer[c].buflen;
    stats_r[I_PKG_COUNTER]++;
    pthread_mutex_unlock(&stat_lock_r);
    c = (BUF_N+1+c)%BUF_N;
    sem_post(&bEmpty);
  }
  puts("THREAD \"processor\" going out");
  pthread_exit(0);
  return (void*) 0;

}

void init_receiver (int s) {
  int t1, t2;
  void * sock = (void*) malloc(sizeof(int));
  if (sock == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  memcpy(sock, &s, sizeof(int));
  memset(&stats_r, 0, sizeof(stats_r));
  pthread_mutex_init(&stat_lock_r, NULL);
  sem_init(&bFull, 0, 0);
  sem_init(&bEmpty, 0, BUF_N);
  t1 = pthread_create(&processor_t, NULL, packet_processor, NULL);
      if (t1){
         printf("ERROR; return code from pthread_create() is %d\n", t1);
         exit(EXIT_FAILURE);
      }
  t2 = pthread_create(&receiver_t, NULL, packet_receiver, (void *) sock);
      if (t2){
         printf("ERROR; return code from pthread_create() is %d\n", t2);
         exit(EXIT_FAILURE);
      }
}

void receiver_end_threads(void) {
    sem_post(&bFull);
    sem_post(&bEmpty);
    pthread_join(processor_t, NULL);
    //pthread_join(receiver_t, NULL);
    sem_destroy(&bFull);
    sem_destroy(&bEmpty);
    pthread_mutex_destroy(&stat_lock_r);
}
