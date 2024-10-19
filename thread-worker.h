// File:	worker_t.h

// List all group member's name:
// username of iLab:
// iLab Server:

#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
#define USE_WORKERS 1
#define STACK_SIZE 8192


/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h> //for scheduler stuff


//Enum for thread states
typedef enum {
	READY,
	SCHEDULED,
	BLOCKED,
	FINISHED
}thread_state_t;

typedef uint worker_t; //worker_t is thread ID

typedef struct TCB {
	/* add important states in a thread control block */
	// thread Id
	// thread status
	// thread context
	// thread stack
	// thread priority
	// And more ...

	// YOUR CODE HERE
	int thread_id;// changed to int
    int priority; 
    thread_state_t status;
    ucontext_t context;
    void *stack; 
    void *return_value;// Pointer to hold the return value (if any)
    int elapsed;// Elapsed time used for scheduling algorithms
	int waiting_time; //Used for MLFQ to see the time a thread has been wiaitng in current queue
    struct TCB *next;
} tcb; 

/* mutex struct definition */
typedef struct worker_mutex_t {
	/* add something here */

	// YOUR CODE HERE
} worker_mutex_t;

/* Priority definitions */
#define NUMPRIO 4

#define HIGH_PRIO 3
#define MEDIUM_PRIO 2
#define DEFAULT_PRIO 1
#define LOW_PRIO 0

//definition for MLFQ to prevent starvation
//Promote thread after 50 timer ticks
#define AGING_THRESHOLD 50 



//Externs
extern tcb *runqueue_head; // Head of the runqueue 
extern tcb *current_thread;  // Global variable for the currently running thread
extern ucontext_t scheduler_context; // Global variable for the scheduler context

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE


/* Function Declarations: */

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level worker threads voluntarily */
int worker_yield();

/* terminate a thread */
void worker_exit(void *value_ptr);

/* wait for thread termination */
int worker_join(worker_t thread, void **value_ptr);

/* initial the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex);

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex);

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex);

//My function prototypes for helpers in thread-worker.c
void enqueue(tcb *thread);
void setup_timer();



/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void);

//for test
#ifdef MLFQ
int worker_setschedprio(worker_t thread, int prio);
#endif

#ifdef USE_WORKERS
#define pthread_t worker_t
#define pthread_mutex_t worker_mutex_t
#define pthread_create worker_create
#define pthread_exit worker_exit
#define pthread_join worker_join
#define pthread_mutex_init worker_mutex_init
#define pthread_mutex_lock worker_mutex_lock
#define pthread_mutex_unlock worker_mutex_unlock
#define pthread_mutex_destroy worker_mutex_destroy
#define pthread_setschedprio worker_setschedprio
#endif

#endif
