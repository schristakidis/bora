#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "netencoder.h"
#include "messages.h"
#include "ack.h"
#include "ack_received.h"

typedef struct __attribute__((__packed__)) {
  unsigned char flags;
  uint16_t streamid;
  uint32_t blockid;
  uint16_t  fragmentid;
  uint16_t  fragments;
  uint16_t length;
} FragmentHeader;

typedef struct __attribute__((__packed__)) {
  unsigned char flags;
  uint16_t seq;
  uint32_t sec;
  uint32_t usec;
} AckPacket;

int get_fragment_size(FragmentData * fragment) {
	int ret;
	ret = sizeof(FragmentHeader) + fragment->length;
	return ret;
}

int validate_ack(unsigned char * blob, size_t l) {
  if (l == sizeof(AckPacket)) {
    if ((((AckPacket*)blob)->flags ^ BLK_ACK) == 0) {
      return 1;
    }
  }

  return 0;
}

int validate_block(unsigned char * blob, size_t l) {
  uint16_t i;
  unsigned char f;
  if (l>=sizeof(FragmentHeader)) {
    i = htons(((FragmentHeader*)blob)->length);
    f = ((FragmentHeader*)blob)->flags;
    if (l==sizeof(FragmentHeader)+i && ((f&MASK_BLOCK_ACK)^BLK_EMPTY) == 0) {
      return 1;
    }
  }
  return 0;
}

SendData encode_fragment(FragmentData * fragment) {
    SendData s;
	FragmentHeader * header;
	//s.data = (unsigned char*) malloc(1500);
	//if (s.data == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
	header = (FragmentHeader *) s.data;
	header->flags = BLK_BLOCK;
	header->streamid = htons(fragment->streamid);
	header->blockid = htonl(fragment->blockid);
	header->fragmentid = htons(fragment->fragmentid);
	header->fragments = htons(fragment->fragments);
	header->length = htons(fragment->length);
	memcpy(s.data+sizeof(FragmentHeader), fragment->data, fragment->length);
	s.length = sizeof(FragmentHeader) + fragment->length;
    return s;
}

FragmentData * decode_fragment(unsigned char * fragmentstring, ssize_t length) {
	if (length < (unsigned)sizeof(FragmentHeader)) {
		fputs("MALFORMED FRAGMENT: LEN LESS THAN HEADER\r\n", stderr);
		return NULL;
	}
	if (!((((FragmentHeader*)fragmentstring)->flags)&BLK_BLOCK)) {
		fputs("FRAGMENT IS NOT PART OF A BLOCK\r\n", stderr);
		return NULL;
	}
	FragmentData * ret = (FragmentData *) malloc(sizeof(FragmentData));
	if (ret == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
	ret->streamid = ntohs(((FragmentHeader*)fragmentstring)->streamid);
	ret->blockid = ntohl(((FragmentHeader*)fragmentstring)->blockid);
	ret->fragmentid = ntohs(((FragmentHeader*)fragmentstring)->fragmentid);
	ret->fragments = ntohs(((FragmentHeader*)fragmentstring)->fragments);
	ret->length = ntohs(((FragmentHeader*)fragmentstring)->length);
	if (length != ret->length + (unsigned)sizeof(FragmentHeader)) {
		fputs("MALFORMED FRAGMENT: LEN NOT MATCHING\r\n", stderr);
		free(ret);
		return NULL;
	}
	ret->data = (unsigned char*) malloc(ret->length);
	if (ret->data == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
	memcpy(ret->data, fragmentstring+sizeof(FragmentHeader), ret->length);
	return ret;
}

SendData encode_ack(uint16_t seq) {
    SendData s;
    //s.data = (unsigned char*) malloc(sizeof(AckPacket));
	//if (s.data == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
	((AckPacket*)s.data)->flags = BLK_ACK;
	((AckPacket*)s.data)->seq = seq;
	s.length = sizeof(AckPacket);
	return s;
}

void append_ack_ts(SendData *s, struct timeval *ts) {
    ((AckPacket*)s->data)->sec = htonl((uint32_t)ts->tv_sec);
    ((AckPacket*)s->data)->usec = htonl((uint32_t)ts->tv_usec);
}

AckReceived * decode_ack(unsigned char* ack_r, ssize_t length) {
    if (length<(unsigned)sizeof(AckPacket)) {
      return NULL;
    }
    AckReceived * ret = (AckReceived*) malloc(sizeof(AckReceived));
	if (ret == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
    ret->flags = ((AckPacket*) ack_r)->flags;
    ret->seq = ((AckPacket*) ack_r)->seq;
    ret->sec = ntohl(((AckPacket*) ack_r)->sec);
    ret->usec = ntohl(((AckPacket*) ack_r)->usec);
    return ret;
}

unsigned char get_flags(unsigned char * fragment) {
    unsigned char ret;
    memcpy(&ret, fragment, 1);
    return ret;
}

unsigned char set_flags(unsigned char * fragment, unsigned char flags) {
    memcpy(fragment, &flags, 1);
    return flags;
}
