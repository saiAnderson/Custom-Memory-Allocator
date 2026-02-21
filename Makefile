CC = gcc
CFLAGS = -Wall -Wextra -g 

TARGET = alloc
OBJS = alloc.o main.o 

all: $(TARGET)

$(TARGET):  $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

alloc.o: alloc.c alloc.h
	$(CC) $(CFLAGS) -c alloc.c

main.o : main.c alloc.h 
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f *.o $(TARGET)