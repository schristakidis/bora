#ifndef ACK_RECEIVED_H
#define ACK_RECEIVED_H

#include <sys/time.h>
#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

#include "ack.h"

typedef struct AckReceived {
  unsigned char flags;
  uint16_t seq;
} AckReceived;


int ack_received(Ack * ack_s, AckReceived * ack_r, struct timeval received, struct sockaddr_in from);

#endif
