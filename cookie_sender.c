#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32 || _WIN64
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/stat.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif


#include "messages.h"
#include "packet_sender.h"
#include "netencoder.h"
#include "blockcache.h"

#include "cookie_sender.h"

static SendData bogusData;
static SendData ckData[2];
static int havecookie[2];
static int gotcookie[2];

int send_cookie (struct sockaddr_in * addr1, struct sockaddr_in * addr2) {
    sem_wait(&ckEmpty);
    memcpy(&ckData[0], &bogusData, sizeof(SendData));
    memcpy(&ckData[1], &bogusData, sizeof(SendData));
    memcpy(&ckData[0].to, addr1, sizeof(struct sockaddr_in));
    memcpy(&ckData[1].to, addr2, sizeof(struct sockaddr_in));
    havecookie[0] = 1;
    havecookie[1] = 1;
    gotcookie[0] = 0;
    gotcookie[1] = 0;
    sem_post(&ckFull);
    sem_post(&ckFull);
    return 1;
}

void check_answers (void)
{
    if (gotcookie[1]==1 && gotcookie[0] == 1) {
        sem_post(&ckEmpty);
    }
}

SendData * get_cookie_data (void) {
    if (havecookie[0] == 1) {
        havecookie[0] = 0;
        return &ckData[0];
    } else if (havecookie[1] == 1) {
        havecookie[1] = 0;
        return &ckData[1];
    }
    puts("NO COOKIES TO SEND!");
    exit(0);
    return NULL;
}

int cookie_received (AckStore * ack) {

    if (gotcookie[0] == 0 && memcmp(ack->addr, &ckData[0].to, sizeof(struct sockaddr_in)) == 0) {
        gotcookie[0] = 1;

        memcpy(&ckResult[0].addr, ack->addr, sizeof(struct sockaddr_in));
        memcpy(&ckResult[0].sent, &ack->sent, sizeof(struct timeval));
        memcpy(&ckResult[0].RTT, &ack->RTT, sizeof(struct timeval));
        memcpy(&ckResult[0].STT, &ack->STT, sizeof(struct timeval));
        ckResult[0].seq = ack->seq;
        ckResult[0].sleeptime = ack->sleeptime;
        check_answers();
        return 1;
    }

    if (gotcookie[1] == 0 && memcmp(ack->addr, &ckData[1].to, sizeof(struct sockaddr_in)) == 0) {
        gotcookie[1] = 1;
        memcpy(&ckResult[1].addr, ack->addr, sizeof(struct sockaddr_in));
        memcpy(&ckResult[1].sent, &ack->sent, sizeof(struct timeval));
        memcpy(&ckResult[1].RTT, &ack->RTT, sizeof(struct timeval));
        memcpy(&ckResult[1].STT, &ack->STT, sizeof(struct timeval));
        ckResult[1].seq = ack->seq;
        ckResult[1].sleeptime = ack->sleeptime;
        check_answers();
        return 1;
    }
    printf("\nACK: %s:%i ", inet_ntoa(((struct sockaddr_in *)ack->addr)->sin_addr), ((struct sockaddr_in *)ack->addr)->sin_port);
    printf("C1: %s:%i ", inet_ntoa((ckData[0].to).sin_addr), (ckData[0].to).sin_port);
    printf("C2: %s:%i ", inet_ntoa(ckData[1].to.sin_addr), ckData[1].to.sin_port);
    puts("BAD COOKIE RECEIVED!");
    return 0;
}

void cookie_cleanup (void) {
    puts("COOKIE TIMEOUT!");
    havecookie[0] = 0;
    havecookie[1] = 0;
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
