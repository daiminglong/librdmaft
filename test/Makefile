.PHONY: clean

CFLAGS  := -Wall -Werror -g
LD      := gcc
LDLIBS  := ${LDLIBS} -lrdmaft

APPS    := client server

all: ${APPS}


client: client.o
	${LD} -o $@ $^ ${LDLIBS}

server: server.o
	${LD} -o $@ $^ ${LDLIBS}

clean:
	rm -f *.o ${APPS}
