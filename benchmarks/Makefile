# Default to empty if SCHED is not set
SCHED ?=

# Use SCHED_FLAG only if SCHED is set
SCHED_FLAG := $(if $(SCHED),-D$(SCHED),)

CC = gcc
CFLAGS = -g -w $(SCHED_FLAG)


all:: clean parallel_cal vector_multiply external_cal

parallel_cal:
	$(CC) $(CFLAGS) -pthread -o parallel_cal parallel_cal.c -L../ -lthread-worker

vector_multiply:
	$(CC) $(CFLAGS) -pthread -o vector_multiply vector_multiply.c -L../ -lthread-worker

external_cal:
	$(CC) $(CFLAGS) -pthread -o external_cal external_cal.c -L../ -lthread-worker

clean:
	rm -rf testcase parallel_cal vector_multiply external_cal *.o ./record/ *.dSYM