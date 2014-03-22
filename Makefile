CFLAGS=-ggdb -pg -Wall -Wextra -Wstrict-prototypes

OBJ=ack.o ack_received.o blockcache.o main.o netencoder.o packet_receiver.o packet_sender.o biter_bridge.o bpuller_bridge.o stats_bridge.o recv_stats.o
LIBS=-lpthread

burundi: ${OBJ}
	gcc  ${CFLAGS} -o burundi ${OBJ} ${LIBS}
clean:
	rm -f burundi *.o
