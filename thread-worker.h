// File:	worker_t.h

// List all group member's name:
// username of iLab:
// iLab Server:

#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE

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
	ucontext_t *context;
	thread_stack *thread_stack;
	int priority;
	//worker_t parent_id;
} tcb; 

/* mutex struct definition */
typedef struct worker_mutex_t {
	/* add something here */
	// YOUR CODE HERE

    int locked; // 1 is locked, 0 is not locked
    tcb* owner; 
    tcb* blocked_list; //all threads that are blocked get added here
} worker_mutex_t;

/* Priority definitions */
#define NUMPRIO 4

#define HIGH_PRIO 3
#define MEDIUM_PRIO 2
#define DEFAULT_PRIO 1
#define LOW_PRIO 0
#define MAX_SIZE 10000000

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)
typedef struct stack {
	worker_t arr[MAX_SIZE];
	worker_t top;
} thread_stack;


typedef struct Node {
    worker_t data;
    struct Node* next;
}Node;

typedef struct Queue {
    struct Node *front, *rear;
}Queue;


// YOUR CODE HERE
/* Function Declarations: */

int push(thread_stack *stack, worker_t value) {
    if (is_full(stack)) {
        return 0;
    }
    stack->arr[++(stack->top)] = value;  
    return 1;
}

int pop(thread_stack *stack, worker_t *value) {
    if (is_empty(stack)) {
        return 0;
    }
    *value = stack->arr[(stack->top)--];  
    return 1;
}

int peek(thread_stack *stack, worker_t *value) {
    if (is_empty(stack)) {
        return 0;
    }
    *value = stack->arr[stack->top];  
    return 1;
}

void enqueue(struct Queue* queue, worker_t value) {
    struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
    newNode->data = value;
    newNode->next = NULL;
    if (queue->rear == NULL) {
        queue->front = queue->rear = newNode;
        return;
    }
    queue->rear->next = newNode;
    queue->rear = newNode;
}

worker_t dequeue(struct Queue* queue) {
    if (queue->front == NULL)
        return -1;
    struct Node* temp = queue->front;
    worker_t thread = temp->data;
    queue->front = queue->front->next;
    if (queue->front == NULL)
        queue->rear = NULL;
    free(temp);
    return thread;
}

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
