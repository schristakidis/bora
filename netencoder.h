#ifndef NETENCODER_H
#define NETENCODER_H

#include "ack_received.h"
#include "packet_sender.h"
#include "blockcache.h"



/* MUST BE FREED */
SendData encode_fragment(FragmentData * fragment);

/* MUST BE FREED */
/* ret is NULL if fragment is malformed */
FragmentData * decode_fragment(unsigned char * fragmentstring, ssize_t length);

/* MUST BE FREED */
SendData encode_ack(uint16_t seq);

/* MUST BE FREED */
AckReceived * decode_ack(unsigned char* ack, ssize_t length);

int get_fragment_size(FragmentData * fragment);

int validate_ack(unsigned char * blob, size_t l);

int validate_block(unsigned char * blob, size_t l);

void append_ack_ts(SendData *s, struct timeval *ts);

#endif
