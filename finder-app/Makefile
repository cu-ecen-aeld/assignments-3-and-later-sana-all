CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -g

TARGET = writer

SRC = writer.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

# Link the executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(TARGET) $(OBJ)
