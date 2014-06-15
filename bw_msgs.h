#ifndef BW_MSGS_H
#define BW_MSGS_H
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/stat.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "queue.h"
#include "blockcache.h"
#include "packet_sender.h"



typedef struct BWMsg {
  uint32_t bw;
  struct timeval recv_time;
  struct sockaddr_in addr;
  MYSLIST_ENTRY(BWMsg) entries;
} BWMsg;

MYSLIST_HEAD(BandwidthMessages, BWMsg);

void bwmsg_received(BWMsg * bw_message);

struct BandwidthMessages get_bwmsg_list(void);

void send_bwmsg (struct sockaddr_in host, uint32_t bw);

#endif
