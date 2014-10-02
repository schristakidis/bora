#ifndef BLOCKCACHE_H
#define BLOCKCACHE_H

#include <stdint.h>
#include <sys/time.h>
#include <pthread.h>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

#define MTU 1400

#ifdef TEST
#define MTU 4
#endif


typedef struct BlockData {
  unsigned char * data;
  uint32_t length;
} BlockData;

typedef struct BlockID {
  uint16_t streamid;
  uint32_t blockid;
} BlockID;

typedef struct Fragments {
  uint16_t fn;
  uint16_t lastlen;
} Fragments;

typedef struct FragmentData {
  uint16_t streamid;
  uint32_t blockid;
  uint16_t fragmentid;
  uint16_t fragments;
  uint16_t length;
  unsigned char * data;
} FragmentData;

typedef struct BlockFragment {
  int have;
  struct sockaddr_in host;
  struct timeval ts;
} BlockFragment;

typedef struct Block {
  struct BlockID id;
  struct BlockData content;
  struct BlockFragment * f;
  struct Fragments fs;
  struct Block * next;
  pthread_mutex_t lock;
} Block;

typedef struct BlockIDList {
  BlockID * blist;
  int length;
} BlockIDList;

typedef struct FragmentID {
  unsigned char flags;
  uint16_t streamid;
  uint32_t blockid;
  uint16_t fragmentid;
  uint16_t fragments;
  uint16_t length;
} FragmentID;

void init_bcache(void);

/* returns 1 if Block has all fragments, 0 if not */
int iscomplete(uint16_t streamid, uint32_t blockid);

/* returns 1 on success or 0 on failure */
int addblock(uint16_t streamid, uint32_t blockid, BlockData * blockdata);

/* addfragment return values */
#define F_ADDED 0
#define F_DUPLICATE 1
#define F_FRAGMENTS_MISMATCH 2
#define F_FRAGMENTID_OUTOFBOUNDS 3
#define F_BAD_LEN 4
int addfragment(FragmentData * fragment, struct sockaddr_in host, struct timeval ts);

/* RETURNS NULL if block doesnt exist OR is incomplete */
/* BlockData.data MUST BE FREED */
BlockData *get_block_data(uint16_t streamid, uint32_t blockid);

/* deleteblock return values */
#define BD_FAILURE 0
#define BD_SUCCESS 1
/* end of deleteblock return values */
int deleteblock(uint16_t streamid, uint32_t blockid);

int sendblock(uint16_t streamid, uint32_t blockid, struct sockaddr_in to);

BlockIDList get_incomplete_block_list(void);

BlockIDList get_complete_block_list(void);

#include "netencoder.h"

uint16_t get_consecutives(FragmentID * fragment);

#endif
