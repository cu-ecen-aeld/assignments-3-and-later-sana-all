#Cross-compiling check
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -O0 -pthread
LDFLAGS ?= -pthread
TARGET ?= aesdsocket

all: aesdsocket
default: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) aesdsocket.c -o $(TARGET) $(LDFLAGS)

#clean target
clean:
	rm -f aesdsocket *.o

.PHONY: all default clean
