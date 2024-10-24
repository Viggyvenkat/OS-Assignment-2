CC = gcc
CFLAGS = -g -c
AR = ar -rc
RANLIB = ranlib

all: clean thread-worker.a

thread-worker.a: thread-worker.o
	$(AR) libthread-worker.a thread-worker.o
	$(RANLIB) libthread-worker.a

thread-worker.o: thread-worker.h
ifeq ($(SCHED), PSJF)
	$(CC) -pthread $(CFLAGS) -DPSJF thread-worker.c
else ifeq ($(SCHED), MLFQ)
	$(CC) -pthread $(CFLAGS) -DMLFQ thread-worker.c
else
	echo "no such scheduling algorithm"
endif

test: thread-worker.a
ifeq ($(SCHED), PSJF)
	$(CC) -pthread -g test.c -o test libthread-worker.a -lrt -DPSJF
else ifeq ($(SCHED), MLFQ)
	$(CC) -pthread -g test.c -o test libthread-worker.a -lrt -DMLFQ
else
	echo "no such scheduling algorithm"
endif

valgrind: test
	valgrind --leak-check=full --show-leak-kinds=all ./test

clean:
	rm -rf testfile *.o *.a test