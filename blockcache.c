#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "blockcache.h"
#include "packet_sender.h"
#include "messages.h"
#include "netencoder.h"

static pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
//static pthread_mutex_t sending_block = PTHREAD_MUTEX_INITIALIZER;

static Block * blockcache = NULL;

void init_bcache(void) {
  return;
  //blockcache = NULL;
}

/*returns LOCKED block - no rwlock -*/
Block *
__findblock(uint16_t streamid, uint32_t blockid) {
  Block * ret = blockcache;
  while (ret != NULL) {
    //pthread_mutex_lock(&ret->lock);
    if (ret->id.streamid == streamid && ret->id.blockid == blockid) {
      pthread_mutex_lock(&ret->lock);
      return ret;
    }
    //pthread_mutex_unlock(&ret->lock);
    ret = ret->next;
  }
  return NULL;
}

/*returns LOCKED block*/
Block *
findblock(uint16_t streamid, uint32_t blockid) {
  Block * ret;
  pthread_rwlock_rdlock(&rwlock);
  ret = __findblock(streamid, blockid);
  pthread_rwlock_unlock(&rwlock);
  return ret;
}

/* block->lock must be locked*/
int
__iscomplete(Block * block) {
  uint16_t i;
  if(block != NULL) {
    i = block->fs.fn;// - 1;
    while (i>0) {
      i--;
      if ((block->f)[i].have == 0) {
        //printf("INCOMPLETEEEEEEE %d of %d\n", i, block->fs.fn);
        pthread_mutex_unlock(&block->lock);
        return 0;
      }
    }
    pthread_mutex_unlock(&block->lock);
  }
  return 1;
}

int
iscomplete(uint16_t streamid, uint32_t blockid) {
  Block * block = findblock(streamid, blockid);
  return __iscomplete(block);
}

Fragments
calcfragments(uint32_t length) {
  if (length<1) {
    Fragments ret = (Fragments){ .fn = 0, .lastlen = 0};
    return ret;
  }
  Fragments ret = (Fragments){ .fn = 0, .lastlen = MTU};
  div_t result;
  result = div(length, MTU);
  ret.fn = result.quot;
  if (result.rem) {
    ret.fn++;
    ret.lastlen = result.rem;
  }
  //printf("LEN %d last= %d fn = %d\n", length, ret.lastlen, ret.fn);
  return ret;
}

int
addblock(uint16_t streamid, uint32_t blockid, BlockData * blockdata) {
  int i;
  Block * block = findblock(streamid, blockid);
  if (block) {
    pthread_mutex_unlock(&block->lock);
    return 0;
  }
  block = (Block*)malloc(sizeof(Block));
  if (block == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  block->id.streamid = streamid;
  block->id.blockid = blockid;
  block->content.data = (unsigned char*)malloc(sizeof(unsigned char)*blockdata->length);
  if (block->content.data == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  memcpy(block->content.data, blockdata->data, blockdata->length);
  block->content.length = blockdata->length;
  block->fs = (Fragments)calcfragments(blockdata->length);
  block->f = (BlockFragment*)malloc(block->fs.fn * sizeof(BlockFragment));
  if (block->f == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  for (i=0; i<block->fs.fn; i++) {
    block->f[i].have = 1;
    //printf("HAVE: %d, i: %d, fn: %d\n", block->f[i].have, i, block->fs.fn);
    block->f[i].host = (struct sockaddr_in){0};
    block->f[i].ts = (struct timeval){0};
  }
  pthread_mutex_init ( &block->lock, NULL);
  pthread_rwlock_wrlock(&rwlock);
  block->next = blockcache;
  blockcache = block;
  pthread_rwlock_unlock(&rwlock);
  return 1;
}

int
deleteblock(uint16_t streamid, uint32_t blockid) {
    Block *prev, *current;

    pthread_rwlock_wrlock(&rwlock);
    prev = blockcache;
    if (prev==NULL) {
      pthread_rwlock_unlock(&rwlock);
      return BD_FAILURE;
    }
    while ((current = prev->next) != NULL) {
        if (current->id.streamid == streamid && current->id.blockid == blockid) {
            prev->next = current->next;
            current->next = NULL;
            pthread_rwlock_unlock(&rwlock);
            pthread_mutex_destroy(&current->lock);
            free(current->content.data);
            free(current->f);
            free(current);
            return BD_SUCCESS;
        }
        prev = current;
    }
    pthread_rwlock_unlock(&rwlock);
    return BD_FAILURE;
}

int addfragment(FragmentData * fragment, struct sockaddr_in host, struct timeval ts) {
    uint16_t i;
	Block * block = findblock(fragment->streamid, fragment->blockid);
	//printf("FRAGMENT RECEIVED: sid:%d, bid:%d, fid:%d, fs:%d, len:%d\n", fragment->streamid, fragment->blockid, fragment->fragmentid, fragment->fragments, fragment->length);
	if (block==NULL) {
	  block = (Block*)malloc(sizeof(Block));
      if (block == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
      block->fs.fn = fragment->fragments;
      block->f = (BlockFragment*)malloc(block->fs.fn * sizeof(BlockFragment));
      if (block->f == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
      for (i=0;i<block->fs.fn;i++) {
        (block->f)[i].have = 0;
      }
      block->content.length = MTU*block->fs.fn;
      block->content.data = (unsigned char *)malloc(sizeof(unsigned char)*MTU*block->fs.fn);
      if (block->content.data == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
      memset(block->content.data, 0, sizeof(unsigned char)*MTU*block->fs.fn);
      block->id.streamid = fragment->streamid;
      block->id.blockid = fragment->blockid;
      pthread_mutex_init ( &block->lock, NULL);
      block->f[fragment->fragmentid] = (BlockFragment){ .have = 1, .ts = ts, .host = host };
      if (fragment->fragmentid == (fragment->fragments)-1) {
        block->fs.lastlen = fragment->length;
      } else {
        if (fragment->length != MTU) {
          pthread_mutex_unlock(&block->lock);
          //puts("BAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADDDDLEEEEEEEEEEEEEEEEEEEEEEEEEEENNN");
          return F_BAD_LEN;
        }
      }
      memcpy(block->content.data + (MTU * fragment->fragmentid), fragment->data, fragment->length);
      pthread_rwlock_wrlock(&rwlock);
      block->next = blockcache;
      blockcache = block;
      pthread_rwlock_unlock(&rwlock);
      return F_ADDED;
	} else {
      if ((block->f)[fragment->fragmentid].have == 1) {
        pthread_mutex_unlock(&block->lock);
        return F_DUPLICATE;
      }
      if (block->fs.fn != fragment->fragments) {
        pthread_mutex_unlock(&block->lock);
        return F_FRAGMENTS_MISMATCH;
      }
      if (fragment->fragmentid >= block->fs.fn) {
        pthread_mutex_unlock(&block->lock);
        return F_FRAGMENTID_OUTOFBOUNDS;
      }
      if (fragment->fragmentid < block->fs.fn -1 && fragment->length!=MTU) {
        pthread_mutex_unlock(&block->lock);
        return F_BAD_LEN;
      }
      if (fragment->fragmentid == fragment->fragments-1) {
        //puts("LAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAASSSTTTTT\n");
        block->fs.lastlen = fragment->length;
        block->content.length = ((block->fs.fn-1)*MTU) + block->fs.lastlen;
      }
      (block->f)[fragment->fragmentid] = (BlockFragment){ .have = 1, .ts = ts, .host = host };
      memcpy(block->content.data + (MTU * fragment->fragmentid), fragment->data, fragment->length);
      //printf("\nFRAGMENT RECEIVED\n len:%d fid:%d fn:%d\n", fragment->length, fragment->fragmentid, fragment->fragments);
      pthread_mutex_unlock(&block->lock);
      return F_ADDED;
	}
}

BlockData *get_block_data(uint16_t streamid, uint32_t blockid) {
  Block * block = findblock(streamid, blockid);
  if (block == NULL) {return NULL;}
  BlockData * ret = malloc(sizeof(BlockData));
  if (ret == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  ret->length = block->content.length;
  ret->data = (unsigned char*) malloc(ret->length);
  if (ret->data == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  //printf("LENNNN: %d FN:%d LAST:%d\n", ret->length, block->fs.fn, block->fs.lastlen);
  memcpy(ret->data, block->content.data, ret->length);
  pthread_mutex_unlock(&block->lock);
  return ret;
}

int sendblock(uint16_t streamid, uint32_t blockid, struct sockaddr_in to) {
  //pthread_mutex_lock(&sending_block);
  Block * block = findblock(streamid, blockid);
  if (block == NULL) {pthread_cond_signal(&blockProduced); /*pthread_mutex_unlock(&sending_block);*/ return 0;}
  FragmentData fragment; // = malloc(sizeof(FragmentData));
  SendData d;
  uint16_t i;
  for (i=0;i<block->fs.fn;i++) {
    //fragment = malloc(sizeof(FragmentData));
    if (i+1 == block->fs.fn) {
      //printf("LAST FRAGMENT\n");
      fragment = (FragmentData){.streamid = streamid, .blockid = blockid, .fragmentid = i, .fragments = block->fs.fn, .length = block->fs.lastlen, .data = block->content.data + MTU*i};

    } else {
      //printf("FRAGMENT %d\n", i);
      fragment = (FragmentData){.streamid = streamid, .blockid = blockid, .fragmentid = i, .fragments = block->fs.fn, .length = MTU, .data = block->content.data + MTU*i};
    }
    d = encode_fragment(&fragment);
    d.to = to;
    //d.data[0] = BLK_BLOCK_ACK;
    d.data[0] = BLK_BLOCK_ACK;
    send_data(d);
    //free(fragment);
  }
  pthread_mutex_unlock(&block->lock);
  pthread_cond_signal(&blockProduced);
  //pthread_mutex_unlock(&sending_block);
  return i;
}

int __get_blockcache_size(void) {
  int ret = 0;
  Block * block = blockcache;
  while(block!=NULL) {
    ret++;
    block = block->next;
  }
  return ret;
}

BlockIDList get_incomplete_block_list(void) {
  Block * cur;
  BlockIDList ret;
  int len;
  ret.length = 0;
  pthread_rwlock_rdlock(&rwlock);
  len = __get_blockcache_size();
  if (!len) {
    ret.blist = NULL;
    pthread_rwlock_unlock(&rwlock);
    return ret;
  }
  cur = blockcache;
  ret.blist = malloc(sizeof(BlockID)*len);
  while (cur != NULL) {
    pthread_mutex_lock(&cur->lock);
    if (!__iscomplete(cur)) {
      ret.blist[ret.length].blockid = cur->id.blockid;
      ret.blist[ret.length].streamid = cur->id.streamid;
      ret.length++;
    }
    cur = cur->next;

  }
  pthread_rwlock_unlock(&rwlock);
  return ret;
}

BlockIDList get_complete_block_list(void) {
  Block * cur;
  BlockIDList ret;
  int len;
  ret.length = 0;
  pthread_rwlock_rdlock(&rwlock);
  len = __get_blockcache_size();
  if (!len) {
    ret.blist = NULL;
    pthread_rwlock_unlock(&rwlock);
    return ret;
  }
  cur = blockcache;
  ret.blist = malloc(sizeof(BlockID)*len);
  while (cur != NULL) {
    pthread_mutex_lock(&cur->lock);
    if (__iscomplete(cur)) {
      ret.blist[ret.length].blockid = cur->id.blockid;
      ret.blist[ret.length].streamid = cur->id.streamid;
      ret.length++;
    }
    cur = cur->next;
  }
  pthread_rwlock_unlock(&rwlock);
  return ret;
}

uint16_t get_consecutives(FragmentID * fragment) {
    uint16_t ret = 0;
    int i;
	Block * block = findblock(fragment->streamid, fragment->blockid);
	//printf("FRAGMENT RECEIVED: sid:%d, bid:%d, fid:%d, fs:%d, len:%d\n", fragment->streamid, fragment->blockid, fragment->fragmentid, fragment->fragments, fragment->length);
	if (block==NULL) {
        return ret;
    }
    for (i=fragment->fragmentid; i>0; i++) {
        i--;
        if (block->f[i].have == 1) {
            ret++;
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&block->lock);
    return ret;
}
