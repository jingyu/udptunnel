CC = gcc
LD = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -fvisibility=hidden -O2
LDFLAGS =

SRCS = hashtable.c log.c acl.c socket.c message.c channel.c tunnel.c udptunnel.c

TEST_SRCS = socket.c tcptest.c

OBJS = $(SRCS:.c=.o)

TEST_OBJS = $(TEST_SRCS:.c=.o)

all: udptunnel tcptest

udptunnel: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

tcptest: $(TEST_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(TEST_OBJS)

clean:
	rm -f *.o
	rm -f udptunnel
	rm -f tcptest
