#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ack.h"
#include "blockcache.h"
#include "netencoder.h"
#include "packet_receiver.h"
#include "packet_sender.h"
#include "biter_bridge.h"

int main (void) {

init_bcache();
printf("BCACHE %p\n", blockcache);

    unsigned char * a = malloc(20000);
    memset(a, 1, 20000);
    BlockData * bdata = malloc(sizeof(BlockData));
    bdata->data = (unsigned char *)a;
    bdata->length = 20;


  int s, i;
  struct sockaddr_in servaddr = {0};
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
  //inet_pton(AF_INET, "127.0.0.1", &(servaddr.sin_addr));
  servaddr.sin_port=htons(3200);
  s = socket(AF_INET, SOCK_DGRAM, 0);
  printf("%i\n", s);
  if (bind(s, (struct sockaddr*)&servaddr, sizeof(servaddr))==-1) {
    perror("CANT BIND:");
  }
  init_sender(s);
  init_receiver(s);
  init_biter();
//sleep(100);
  addblock(1, 1, bdata);
  for (i=0;i<5000;i++) {
  printf("\nadd %i %p %i\n", addblock(1,i,bdata), blockcache, i);
  }
  Block * found = findblock(1,9);
  printf("9:%p ->next:%p", found, found->next);
  puts("OVER");
  BlockIDList listblock = get_complete_block_list();
  printf("LIST: %i\n", listblock.length);
  sleep(2);
  for (i=0; i<listblock.length; i++) {
      //PyTuple_SetItem(ret, i, Py_BuildValue("{sisi}", "sid", blist.blist[i].streamid, "bid", blist.blist[i].blockid));
      printf("%i %i\n", listblock.blist[i].blockid,listblock.blist[i].streamid );
    }
  for (i=0;i<2;i++) {
  printf("\ndel %i %p %i\n", deleteblock(1,i), blockcache, i);
  }
  listblock = get_complete_block_list();
  printf("LIST: %i\n", listblock.length);
  for (i=0; i<listblock.length; i++) {
      //PyTuple_SetItem(ret, i, Py_BuildValue("{sisi}", "sid", blist.blist[i].streamid, "bid", blist.blist[i].blockid));
      printf("%i %i\n", listblock.blist[i].blockid,listblock.blist[i].streamid );
  }
  printf("\ndel %i %p %i\n", deleteblock(1,9), blockcache, 9);
  printf("\ndel %i %p %i\n", deleteblock(1,6), blockcache, 6);
  listblock = get_complete_block_list();
  printf("LIST: %i\n", listblock.length);
  for (i=0; i<listblock.length; i++) {
      //PyTuple_SetItem(ret, i, Py_BuildValue("{sisi}", "sid", blist.blist[i].streamid, "bid", blist.blist[i].blockid));
      printf("%i %i\n", listblock.blist[i].blockid,listblock.blist[i].streamid );
  }
  for (i=0;i<5000;i++) {
  printf("\ndel %i %p %i\n", deleteblock(1,i), blockcache, i);
  }
  puts("OVER");
  listblock = get_complete_block_list();
  printf("LIST: %i\n", listblock.length);
  for (i=0; i<listblock.length; i++) {
      //PyTuple_SetItem(ret, i, Py_BuildValue("{sisi}", "sid", blist.blist[i].streamid, "bid", blist.blist[i].blockid));
      printf("%i %i\n", listblock.blist[i].blockid,listblock.blist[i].streamid );
    }
  //return 0;

  addblock(1, 1, bdata);
  for (i=0; i<100; i++) {
  sendblock(1, 1, servaddr);
  usleep(17500);
  }
  sleep(3);
  for (i=0; i<3; i++) {
  sendblock(1, 1, servaddr);
  }
  sleep(1);
  printf("NACK %i\n\n\n", get_n_nack());
  exit(0);
  return 0;
}
