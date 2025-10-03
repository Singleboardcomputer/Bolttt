CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -O3 -march=native -mavx2 -mclflush -std=c11
CXXFLAGS = -Wall -Wextra -O3 -march=native -mavx2 -mclflush -std=c++17
LDFLAGS = -lpthread -lm -lrt

TARGET = lvi-dma-fuzzer
SOURCES = main.c \
          core/timing.c \
          core/cache.c \
          core/affinity.c \
          core/coresidency.c \
          stats/bootstrap.c \
          virtio/descriptor.c \
          virtio/race.c \
          gadgets/scanner.c \
          db/sqlite_db.c

OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
