CFLAGS=-ggdb -pg -Wall -Wextra -Wstrict-prototypes -DBORA_RETRANSMISSION -DBORA_TIMEOUT

OBJ=ack.o ack_received.o blockcache.o main.o netencoder.o packet_receiver.o packet_sender.o biter_bridge.o bpuller_bridge.o stats_bridge.o recv_stats.o bw_stats.o bw_msgs.o cookie_sender.o bora_util.o
LIBS=-lpthread

burundi: ${OBJ}
	gcc  ${CFLAGS} -o burundi ${OBJ} ${LIBS}
clean:
	rm -f burundi *.o
