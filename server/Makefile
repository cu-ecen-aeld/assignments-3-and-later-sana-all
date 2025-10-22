CC ?= gcc
CFLAGS := -Wall -Werror -g
LDFLAGS := 

# Default target
all: aesdsocket
default: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c $(LDFLAGS)

clean:
	rm -f aesdsocket *.o

.PHONY: all default clean