#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdint.h>

#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "netencoder.h"
#include "queue.h"
#include "bw_msgs.h"


static struct BandwidthMessages bwmlist = ((struct BandwidthMessages){.slh_first = NULL});

static pthread_mutex_t bwm_lock = PTHREAD_MUTEX_INITIALIZER;


void bwmsg_received(BWMsg * bw_message) {
    pthread_mutex_lock(&bwm_lock);
    SLIST_INSERT_HEAD(&bwmlist, bw_message, entries);
    pthread_mutex_unlock(&bwm_lock);
}

struct BandwidthMessages get_bwmsg_list(void) {
    struct BandwidthMessages ret;
    pthread_mutex_lock(&bwm_lock);
    ret = bwmlist;
    SLIST_INIT(&bwmlist);
    pthread_mutex_unlock(&bwm_lock);
    return ret;
}

void send_bwmsg (struct sockaddr_in host, uint32_t bw) {
    SendData mess = encode_bw(bw);
    mess.to = host;
    send_data(mess);
}
