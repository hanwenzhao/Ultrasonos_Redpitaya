CC = gcc
CFLAGS = -g -std=gnu99 -Wall -O3
CFLAGS += -I/opt/redpitaya/include
LDFLAGS = -L/opt/redpitaya/lib
LDLIBS = -lm -lpthread -lrp

OBJFILES = main.o
TARGET = main

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) $(OBJFILES) $(LDFLAGS) $(LDLIBS) -o $(TARGET)

clean:
	rm -f $(OBJFILES) $(TARGET)