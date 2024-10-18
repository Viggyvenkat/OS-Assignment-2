// File:	worker_t.h

// List all group member's name:
// username of iLab:
// iLab Server:

#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE

#define MAX_SIZE 10000000
 

/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
#define USE_WORKERS 1

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ucontext.h>

typedef uint worker_t;

typedef enum {
    THREAD_NEW,
    THREAD_RUNNABLE,
    THREAD_BLOCKED,
    THREAD_WAITING,
    THREAD_TERMINATED
} thread_status_t;


//thread stack needs to be above TCB
typedef struct stack {
	worker_t arr[MAX_SIZE];
	worker_t top;
} thread_stack;

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
	thread_status_t thread_status; 
	ucontext_t context; //took * out. Now can pass directly
	thread_stack *thread_stack;
	int priority;
	//Function ptr for task
	void (*function)(void *);
	//Arg for thread's function
	void *arg;
	//worker_t parent_id;
} tcb; 

typedef struct Node {
    tcb *data;
    struct Node *next;
}Node;

typedef struct Queue {
    struct Node *front, *rear;
}Queue;


/* mutex struct definition */
typedef struct worker_mutex_t {
	/* add something here */
	// YOUR CODE HERE

    int locked; // 1 is locked, 0 is not locked
    tcb* owner; 
    Queue* blocked_list; //all threads that are blocked get added here
} worker_mutex_t;

/* Priority definitions */
#define NUMPRIO 4

#define HIGH_PRIO 3
#define MEDIUM_PRIO 2
#define DEFAULT_PRIO 1
#define LOW_PRIO 0
//moving this up cause it's not getting recognized

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)






// YOUR CODE HERE
/* Function Declarations: */

extern int push(thread_stack *stack, worker_t value);
extern int pop(thread_stack *stack, worker_t *value);
extern int peek(thread_stack *stack, worker_t *value);
extern void enqueue(struct Queue* queue, tcb* new_thread);
extern tcb* dequeue(struct Queue* queue);
extern void ring(int signum);
extern int is_full(thread_stack *stack);
extern int is_empty(thread_stack *stack);

extern long tot_cntx_switches;
extern double avg_turn_time;
extern double avg_resp_time;
extern ucontext_t scheduling_context, main_context, current_context;
extern Queue* runqueue;
extern struct itimerval timer;
extern struct sigaction sa;
extern tcb* main_thread;
extern tcb* cur_thread;
extern int scheduling_init;



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
