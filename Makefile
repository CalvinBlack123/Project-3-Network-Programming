CC = gcc
CFLAGS = -pthread -Wall -Wextra
TARGET = server

all: $(TARGET)

$(TARGET): server.c server_utils.c
	$(CC) $(CFLAGS) -o $(TARGET) server.c server_utils.c

clean:
	rm -f $(TARGET)
