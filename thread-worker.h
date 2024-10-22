// File:	worker_t.h

// List all group member's name: Divit Shetty & Vignesh Venkat
// username of iLab: dps190 & vvv11
// iLab Server: 3

#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
#define USE_WORKERS 1
#define STACK_SIZE 8192

//10 ms
#define TIME 10
#define IT_S TIME/1000
#define IT_US (TIME * 1000) % 1000000

//definition for MLFQ to prevent starvation
//Promote thread after 10 timer ticks
#define AGING_THRESHOLD 10


/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h> //for scheduler stuff
#include <time.h> //for clock


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
	worker_t thread_id; 
    thread_state_t status; 
    ucontext_t context; 
    void* stack; 
    int priority; 
    int elapsed; //use for promotion/demotion
    void* return_value; 
    struct TCB* next; 
    struct TCB* blocked_list; //may not be necessary with the block_queue
    clock_t queue_time; //use for metrics that're needed 
    clock_t start_time;  //use for metrics
    clock_t end_time;  //use for metrics
    long response_time;  //use for metrics
    long turnaround_time; //use for metrics 
} tcb; 

//Reusing the old Queue Struct
typedef struct Queue {
        tcb* head;
        tcb* rear;
} Queue;

#define MAX_BLOCK 10000000
/* mutex struct definition */
typedef struct worker_mutex_t {
	/* add something here */
	int initialize; // 1 = initialize
    int locked; // 1 = locked
    tcb* owner; // Pointer to TCB owner of the current mutex lock
    tcb* blocked_list; // Pointer to a list of blocked threads waiting for the mutex. Holdover from last one not really used 
    int blocked_count; 
    int max_blocked; // Maximum number of blocked threads allowed

	// YOUR CODE HERE
} worker_mutex_t;

/* Priority definitions */
#define NUMPRIO 4

#define HIGH_PRIO 3
#define MEDIUM_PRIO 2
#define DEFAULT_PRIO 1
#define LOW_PRIO 0


/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE
//Our methods

// initialize scheduler_context to point to schedule() [Solution to segmentation fault issue]
int setup_scheduler_context();

//add a thread to the queue
void enqueue(Queue *queue, tcb* thread);

//Remove a thread from a queue
tcb* dequeue(Queue* queue);

//clean reset to the highest priority (default)
void refresh_mlfq();

//Remove thread (MLFQ)
void dequeue_mlfq();

//Remove thread (for PSJF)
tcb* dequeue_psjf(Queue* queue);

// Removes thread from blocked queue 
void dequeue_blocked();

//Search a specific queue for a specific thread 
//Similar to the other find_thread_by_id just that it checks a queue instead of a list
static tcb* find_thread_by_id(worker_t thread, Queue *queue);

//Search all queues for specicifc thread 
//Created a new function after having trouble extending the original
static tcb* find_thread_by_id_all_queues(worker_t thread);

//use in schedule() to keep it running until the Queue is empty
int theQueueisEmpty();

//Metric stuff to track runtime
static void ring(); 
static void timer_setup();
static void start_timer();
static void stop_timer();

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