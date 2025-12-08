#Cross-compiling check
# Makefile
# To build and clean the aesdsocket utility
# Author: Mcrey Fonacier <polpokpol2@gmail.com>


CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -g
# LDFLAGS ?= 
LDFLAGS ?= -Wl,--hash-style=gnu
LIBS ?= -lpthread -lrt

OUTPUT = aesdsocket
SRC = aesdsocket.c

all: $(OUTPUT)

default: all

$(OUTPUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OUTPUT)