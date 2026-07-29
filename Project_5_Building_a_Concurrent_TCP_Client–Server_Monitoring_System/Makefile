CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=gnu99
LDFLAGS = -lpthread

all: server client

server: server.o shared.o net_utils.o
	$(CC) server.o shared.o net_utils.o -o server $(LDFLAGS)

client: client.o net_utils.o
	$(CC) client.o net_utils.o -o client

server.o: server.c protocol.h net_utils.h shared.h
	$(CC) $(CFLAGS) -c server.c -o server.o

client.o: client.c protocol.h net_utils.h
	$(CC) $(CFLAGS) -c client.c -o client.o

shared.o: shared.c shared.h
	$(CC) $(CFLAGS) -c shared.c -o shared.o

net_utils.o: net_utils.c net_utils.h protocol.h
	$(CC) $(CFLAGS) -c net_utils.c -o net_utils.o

clean:
	rm -f *.o server client

.PHONY: all clean