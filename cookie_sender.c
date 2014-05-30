#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "messages.h"
#include "packet_sender.h"
#include "netencoder.h"
#include "blockcache.h"

#include "cookie_sender.h"

static SendData bogusData;
static SendData ckData[2];
static CookieAck ckAck[2];
static int havecookie[2];
//static pthread_mutex_t ckLock = PTHREAD_MUTEX_INITIALIZER;

int send_cookie (struct sockaddr_in * addr1, struct sockaddr_in * addr2) {
    sem_wait(&ckEmpty);
    //pthread_mutex_lock(&ckLock);
    memcpy(&ckData[0], &bogusData, sizeof(SendData));
    memcpy(&ckData[1], &bogusData, sizeof(SendData));
    memcpy(&ckData[0].to, addr1, sizeof(struct sockaddr_in));
    memcpy(&ckData[1].to, addr2, sizeof(struct sockaddr_in));
    havecookie[0] = 1;
    havecookie[1] = 1;
    //puts("setting fields");
    //printf("field1: %i field2: %i\n", havecookie[0], havecookie[1]);
    sem_post(&ckFull);
    sem_post(&ckFull);
    //pthread_mutex_unlock(&ckLock);
    return 1;
}

void check_answers (void)
{
    if (havecookie[1]==0 && havecookie[0] == 0) {
        memcpy(&ckAck, &ckResult, sizeof(CookieAck[2]));
        sem_post(&ckEmpty);
    }
}

SendData * get_cookie_data (void) {
    //pthread_mutex_lock(&ckLock);
    //puts("GET COOKIE DATA\n");
    //printf("field1: %i field2: %i\n", havecookie[0], havecookie[1]);
    if (havecookie[0] == 1) {
        havecookie[0] = 0;
        //pthread_mutex_unlock(&ckLock);
        return &ckData[0];
    } else if (havecookie[1] == 1) {
        havecookie[1] = 0;
        //pthread_mutex_unlock(&ckLock);
        return &ckData[1];
    }
    //puts("AAAAAAAAAAAAAAAAAAAAA");
    puts("NO COOKIES TO SEND!");
    //pthread_mutex_unlock(&ckLock);
    exit(0);
    return NULL;
}

int cookie_received (AckStore * ack) {
    //pthread_mutex_lock(&ckLock);

    if (havecookie[0] == 1 && memcmp(ack->addr, &ckData[0].to, sizeof(struct sockaddr_in)) == 0) {
        havecookie[0] = 0;
        memcpy(&ckAck[0].addr, ack->addr, sizeof(struct sockaddr_in));
        memcpy(&ckAck[0].sent, &ack->sent, sizeof(struct timeval));
        memcpy(&ckAck[0].RTT, &ack->RTT, sizeof(struct timeval));
        memcpy(&ckAck[0].STT, &ack->STT, sizeof(struct timeval));
        ckAck[0].seq = ack->seq;
        ckAck[0].sleeptime = ack->sleeptime;
        check_answers();
        //pthread_mutex_unlock(&ckLock);
        return 1;
    }

    if (havecookie[1] == 1 && memcmp(ack->addr, &ckData[1].to, sizeof(struct sockaddr_in)) == 0) {
        havecookie[1] = 0;
        memcpy(&ckAck[1].addr, ack->addr, sizeof(struct sockaddr_in));
        memcpy(&ckAck[1].sent, &ack->sent, sizeof(struct timeval));
        memcpy(&ckAck[1].RTT, &ack->RTT, sizeof(struct timeval));
        memcpy(&ckAck[1].STT, &ack->STT, sizeof(struct timeval));
        ckAck[1].seq = ack->seq;
        ckAck[1].sleeptime = ack->sleeptime;
        check_answers();
        //pthread_mutex_unlock(&ckLock);
        return 1;
    }
    //pthread_mutex_unlock(&ckLock);
    //printf("\nACK: %s:%i ", inet_ntoa(((struct sockaddr_in *)ack->addr)->sin_addr), ((struct sockaddr_in *)ack->addr)->sin_port);
    //printf("C1: %s:%i ", inet_ntoa((ckData[0].to).sin_addr), (ckData[0].to).sin_port);
    //printf("C2: %s:%i ", inet_ntoa(ckData[1].to.sin_addr), ckData[1].to.sin_port);
    puts("BAD COOKIE RECEIVED!");
    return 0;
}

void cookie_cleanup (void) {
    puts("COOKIE TIMEOUT!");
    //pthread_mutex_lock(&ckLock);
    havecookie[0] = 0;
    havecookie[1] = 0;
    //pthread_mutex_unlock(&ckLock);
    sem_post(&ckEmpty);
}


void init_cksender (void) {
    havecookie[0] = 0;
    havecookie[1] = 0;
    memset(&bogusData.data, 1, MTU+get_header_size());
    bogusData.data[0] = NEED_ACK | COOKIE_MSG;
    bogusData.length = MTU+get_header_size();
    sem_init(&ckFull, 0, 0);
    sem_init(&ckEmpty, 0, 1);
}
