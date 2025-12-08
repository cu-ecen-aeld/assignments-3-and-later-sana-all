#Cross-compiling check
# Makefile
# To build and clean the aesdsocket utility
# Author: Mcrey Fonacier <polpokpol2@gmail.com>


CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -O0 -pthread
LDFLAGS ?= -pthread
TARGET ?= aesdsocket

all: aesdsocket
default: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) aesdsocket.c -o $(TARGET) $(LDFLAGS)
	chmod 755 $(TARGET)  # Set permissions to 755

#clean target
clean:
	rm -f aesdsocket *.o

.PHONY: all default clean
