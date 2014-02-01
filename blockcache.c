#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "blockcache.h"
#include "packet_sender.h"
#include "messages.h"
#include "netencoder.h"

//static Block * blockcache = NULL;

static pthread_mutex_t bclock = PTHREAD_MUTEX_INITIALIZER;

void init_bcache(void) {
  blockcache = NULL;
}


// NO MUTEXES ____PRIVATE VERSION____
Block *
_findblock(uint16_t streamid, uint32_t blockid) {
  Block * block = blockcache;
  while(block!=NULL) {
    if (block->streamid == streamid &&
        block->blockid  == blockid) {
        return block;
    }
    block = block->next;
  }
  return NULL;
}

// VERSION WITH MUTEX
Block *
findblock(uint16_t streamid, uint32_t blockid) {
  Block * ret;
  pthread_mutex_lock(&bclock);
  ret = _findblock(streamid, blockid);
  pthread_mutex_unlock(&bclock);
  return ret;
}

// NO MUTEXES ____PRIVATE VERSION____
int __get_blockcache_size(void) {
  int ret = 0;
  Block * block = blockcache;
  while(block!=NULL) {
    ret++;
    block = block->next;
  }
  return ret;
}

BlockFragment * getfragment(Block * block, uint8_t  fragmentid) {
	BlockFragment * cursor;
	cursor = block->content;
	while (cursor) {
		if (cursor->data) {
			if (cursor->data->fragmentid == fragmentid) {
				return cursor;
			}
		}
		cursor = cursor->next;
	}
	return NULL;
}

// NO MUTEXES ____PRIVATE VERSION____
int __iscomplete(Block * block) {
	int i;
	if (block) {
		if (block->content) {
			if (block->content->data) {
			   for (i = 0; i < block->content->data->fragments; i++) {
				   if (!getfragment(block, i)) {
					   return 0;
				   }
			   }
			   return 1;
		    }
	    }
    }
	return 0;
}

int
iscomplete(uint16_t streamid, uint32_t blockid) {
	int i;
	Block * block;
	pthread_mutex_lock(&bclock);
	block = _findblock(streamid, blockid);
	if (block) {
		if (block->content) {
			if (block->content->data) {
			   for (i = 0; i < block->content->data->fragments; i++) {
				   if (!getfragment(block, i)) {
				       pthread_mutex_unlock(&bclock);
					   return 0;
				   }
			   }
			   pthread_mutex_unlock(&bclock);
			   return 1;
		    }
	    }
    }
    pthread_mutex_unlock(&bclock);
	return 0;
}

int
addblock(uint16_t streamid, uint32_t blockid, BlockData * blockdata) {
  if (findblock(streamid, blockid)) {
    return 0;
  }
  unsigned char * data = blockdata->data;
  int length = blockdata->length;
  int i, fnum;
  div_t result;
  result = div(length, MTU);
  fnum = result.quot;
  if (result.rem) { fnum++;}
  Block * ret = (Block*)malloc(sizeof(Block));
  if (ret == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  ret->streamid = streamid;
  ret->blockid = blockid;
  ret->content = NULL;
  for (i=0; i<fnum; i++) {
    BlockFragment * fragment = (BlockFragment*)malloc(sizeof(BlockFragment));
    if (fragment == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
    fragment->next = ret->content;
    bzero(&fragment->host, sizeof(struct sockaddr_in));
    bzero(&fragment->ts, sizeof(struct timeval));
    FragmentData * fdata = (FragmentData*)malloc(sizeof(FragmentData));
    if (fdata == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
    fragment->data = fdata;
    fdata->streamid = streamid;
    fdata->blockid = blockid;
    fdata->fragmentid = i;
    fdata->fragments = fnum;
    fdata->length = MTU;
    if (i+1==fnum && result.rem>0) { fdata->length = result.rem;}
    fdata->data = (unsigned char*)malloc(sizeof(unsigned char)*fdata->length);
    if (fdata->data == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
    memcpy(fdata->data, data+(MTU*i), fdata->length);
    ret->content = fragment;
  }
  pthread_mutex_lock(&bclock);
  ret->next = blockcache;
  blockcache = ret;
  pthread_mutex_unlock(&bclock);
  return 1;
}

int
deleteblock(uint16_t streamid, uint32_t blockid) {
  pthread_mutex_lock(&bclock);
  if (blockcache==NULL) {pthread_mutex_unlock(&bclock); return BD_FAILURE;}
  Block * haz = _findblock(streamid,blockid);
  Block * prev;
  BlockFragment * n;
  if (haz==blockcache && haz->next==NULL) {
    blockcache = NULL;
  }
  if (haz!=NULL) {
      if (haz->content) {
        n = haz->content;
        while (n!=NULL) {
          if (n->data!=NULL) {
            free(n->data->data);
            free(n->data);
          }
          n = n->next;
        }
        free(haz->content);
      }
      if (haz == blockcache && haz->next) {
        blockcache = haz->next;
        pthread_mutex_unlock(&bclock);
        free(haz);
        return BD_SUCCESS;
      }
      prev = blockcache;
      while (prev!=NULL) {
        if (prev->next==haz) {break;}
        prev = prev->next;
      }
      if (prev!=NULL) {prev->next = haz->next;}
      free(haz);
      pthread_mutex_unlock(&bclock);
      return BD_SUCCESS;
  }
  pthread_mutex_unlock(&bclock);
  return BD_FAILURE;
}

int addfragment(FragmentData * fragment, struct sockaddr_in host, struct timeval ts) {
	BlockFragment * newfrag;
    pthread_mutex_lock(&bclock);
	Block * block = _findblock(fragment->streamid, fragment->blockid);
	//IF BLOCK EXISTS
	if (block) {
		if (getfragment(block, fragment->fragmentid)) {
			free(fragment->data);
			free(fragment);
			//puts ("*DUPPPP");
            pthread_mutex_unlock(&bclock);
			return F_DUPLICATE;
		} else {
			if (block->content->data->fragments != fragment->fragments) {
			    free(fragment->data);
			    free(fragment);
                pthread_mutex_unlock(&bclock);
                //puts("*MMATCH");
				return F_FRAGMENTS_MISMATCH;
			} else if (block->content->data->fragmentid >= fragment->fragments) {
			    free(fragment->data);
			    free(fragment);
                pthread_mutex_unlock(&bclock);
                //puts("*OUTBOUND");
			    return F_FRAGMENTID_OUTOFBOUNDS;
			}
			newfrag = (BlockFragment*) malloc(sizeof(BlockFragment));
            if (newfrag == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
			newfrag->data = fragment;
			newfrag->next = block->content;
			newfrag->host = host;
			newfrag->ts = ts;
			block->content = newfrag;
		}
	//IF BLOCK NO EXIST (first fragment received)
	} else {
		block = (Block*) malloc(sizeof(Block));
        if (block == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
		block->streamid = fragment->streamid;
		block->blockid = fragment->blockid;
		block->content = (BlockFragment*) malloc(sizeof(BlockFragment));
        if (block->content == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
			block->content->data = fragment;
			block->content->next = NULL;
			block->content->host = host;
			block->content->ts = ts;
		block->next = blockcache;
		blockcache = block;
	}
    pthread_mutex_unlock(&bclock);
	return F_ADDED;
}

BlockData *get_block_data(uint16_t streamid, uint32_t blockid) {
  pthread_mutex_lock(&bclock);
  Block * block = _findblock(streamid, blockid);
  BlockFragment * bf;
  if (block == NULL) {pthread_mutex_unlock(&bclock); return NULL;}
  BlockData * ret = malloc(sizeof(BlockData));
  if (ret == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  ret->length = 0;
  ret->data = (unsigned char*) malloc(sizeof(unsigned char)*MTU*block->content->data->fragments);
  if (ret->data == NULL) { perror("Unable to allocate memory"); exit(EXIT_FAILURE); }
  int i;
  for (i=0; i<block->content->data->fragments; i++) {
    bf = getfragment(block, i);
    if (bf == NULL) {
      fputs("INCOMPLETE BLOCK\r\n", stderr);
      free(ret->data);
      free(ret);
      pthread_mutex_unlock(&bclock);
      return NULL;
    }
    memcpy(ret->data+(MTU*i), bf->data->data, bf->data->length);
    ret->length += bf->data->length;
  }
  pthread_mutex_unlock(&bclock);
  return ret;
}

int sendblock(uint16_t streamid, uint32_t blockid, struct sockaddr_in to) {
  pthread_mutex_lock(&bclock);
  Block * block = _findblock(streamid, blockid);
  BlockFragment * bf;
  SendData d;
  if (block == NULL) {pthread_mutex_unlock(&bclock); return 0;}
  int i;
  for (i=0; i<block->content->data->fragments; i++) {
    bf = getfragment(block, i);
    if (bf == NULL) {
      fputs("INCOMPLETE BLOCK\r\n", stderr);
      pthread_mutex_unlock(&bclock);
      return 0;
    }
    d = encode_fragment(bf->data);
    d.to = to;
    d.data[0] = BLK_BLOCK_ACK;
    send_data(d);
  }
  pthread_mutex_unlock(&bclock);
  pthread_cond_signal(&blockProduced);
  return i;
}

int get_blockcache_size(void) {
  int ret = 0;
  pthread_mutex_lock(&bclock);
  ret = __get_blockcache_size();
  pthread_mutex_unlock(&bclock);
  return ret;
}

BlockIDList get_incomplete_block_list(void) {
  Block * cur;
  BlockIDList ret;
  ret.length = get_blockcache_size();
  if (!ret.blist) {
    ret.blist = NULL;
    return ret;
  }
  ret.blist = malloc(sizeof(BlockID)*ret.length);
  ret.length = 0;
  pthread_mutex_lock(&bclock);
  cur = blockcache;
  while (cur) {
    if (!__iscomplete(cur)) {
      ret.blist[ret.length].blockid = cur->blockid;
      ret.blist[ret.length].streamid = cur->streamid;
      ret.length++;
    }
    cur = cur->next;
  }
  pthread_mutex_unlock(&bclock);
  return ret;

}

BlockIDList get_complete_block_list(void) {
  Block * cur;
  BlockIDList ret;
  ret.length = get_blockcache_size();
  if (!ret.blist) {
    ret.blist = NULL;
    return ret;
  }
  ret.blist = malloc(sizeof(BlockID)*ret.length);
  ret.length = 0;
  pthread_mutex_lock(&bclock);
  cur = blockcache;
  while (cur) {
    if (__iscomplete(cur)) {
      ret.blist[ret.length].blockid = cur->blockid;
      ret.blist[ret.length].streamid = cur->streamid;
      ret.length++;
    }
    cur = cur->next;
  }
  pthread_mutex_unlock(&bclock);

  return ret;

}

#ifdef TEST
int
main() {
  char data[15] = "PERDItEMPO";
  int length = strlen(data);
  int streamid = 132;
  int blockid = 223;
  addblock(streamid, blockid, data, length);
  printf("%d %d\n", length, deleteblock(streamid, blockid));
  puts("ANIMALIII");
  return EXIT_SUCCESS;
}
#endif
