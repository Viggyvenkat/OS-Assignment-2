# Default to empty if SCHED is not set
SCHED ?=

# Use SCHED_FLAG only if SCHED is set
SCHED_FLAG := $(if $(SCHED),-D$(SCHED),)

CC = gcc
CFLAGS = -g -w $(SCHED_FLAG)


all:: clean parallel_cal vector_multiply external_cal test

parallel_cal:
	$(CC) $(CFLAGS) -pthread -o parallel_cal parallel_cal.c -L../ -lthread-worker

vector_multiply:
	$(CC) $(CFLAGS) -pthread -o vector_multiply vector_multiply.c -L../ -lthread-worker

external_cal:
	$(CC) $(CFLAGS) -pthread -o external_cal external_cal.c -L../ -lthread-worker

test:
	$(CC) $(CFLAGS) -pthread -o test test.c -L../ -lthread-worker

clean:
	rm -rf testcase test parallel_cal vector_multiply external_cal *.o ./record/ *.dSYM
