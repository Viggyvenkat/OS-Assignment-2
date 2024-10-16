// File:	thread-worker.c

// List all group member's name:
// username of iLab:
// iLab Server:

#include "thread-worker.h"
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ucontext.h>
#include <sys/time.h>
#include <signal.h>

//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;


// INITAILIZE ALL YOUR OTHER VARIABLES HERE
// YOUR CODE HERE
ucontext_t scheduling_context, main_context, current_context;
Queue* runqueue;
struct itimerval timer;
struct sigaction sa;
tcb* main_thread;
tcb* cur_thread;
int scheduling_init= 0;

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

       // - create Thread Control Block (TCB)
       // - create and initialize the context of this worker thread
       // - allocate space of stack for this thread to run
       // after everything is set, push this thread into run queue and 
       // - make it ready for the execution.

       // YOUR CODE HERE
	   	if (scheduling_init = 0){
			scheduling_context.uc_stack.ss_sp = malloc(MAX_SIZE); 
			scheduling_context.uc_stack.ss_size = MAX_SIZE;
			scheduling_context.uc_stack.ss_flags = 0;
			scheduling_context.uc_link = NULL;

			memset (&sa, 0, sizeof(sa));
			sa.sa_handler = &ring;
			sigaction (SIGPROF, &sa, NULL);

			timer.it_value.tv_usec = 0;
			timer.it_value.tv_sec = 1;
			setitimer(ITIMER_PROF, &timer, NULL);

			main_thread = (tcb **) malloc(sizeof(main_thread));
			getcontext(main_thread->context);
			main_thread->context = &main_context;
			main_thread->context->uc_link = &scheduling_context;
			main_thread->thread_id = 0;
			cur_thread = main_thread;
			swapcontext(main_thread->context, &scheduling_context);
			
			scheduling_init = 1;
		}
	    runqueue = (Queue *) malloc(sizeof(Queue));
		tcb* worker_thread = (tcb *) malloc(sizeof(tcb));
		worker_thread->thread_id = thread;
		worker_thread->priority = DEFAULT_PRIO;
		worker_thread->thread_stack = (thread_stack *) malloc(sizeof(thread_stack));

		getcontext(worker_thread->context);
		worker_thread->context->uc_stack.ss_sp = malloc(MAX_SIZE); 
		worker_thread->context->uc_stack.ss_size = MAX_SIZE;
		worker_thread->context->uc_stack.ss_flags = 0;
		worker_thread->context->uc_link = &scheduling_context;

        makecontext(worker_thread->context, function, 1, arg);	

		worker_thread->thread_status = THREAD_NEW;
		enqueue(runqueue, worker_thread->thread_id);

    return 0;
};


#ifdef MLFQ
/* This function gets called only for MLFQ scheduling set the worker priority. */
int worker_setschedprio(worker_t thread, int prio) {


   // Set the priority value to your thread's TCB
   // YOUR CODE HERE

   return 0;	

}
#endif



/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context
	

	// YOUR CODE HERE
	
	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread
	// YOUR CODE HERE
	free(value_ptr);
};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
  
	// YOUR CODE HERE
	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex

	// YOUR CODE HERE
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        // YOUR CODE HERE
        return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init

	return 0;
};

/* scheduler */
static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function

	// - invoke scheduling algorithms according to the policy (PSJF or MLFQ)

	// if (sched == PSJF)
	//		sched_psjf();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE

// - schedule policy
#ifndef MLFQ
	// Choose PSJF
#else 
	// Choose MLFQ
#endif

}

/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}


/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

//DO NOT MODIFY THIS FUNCTION
/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void) {

       fprintf(stderr, "Total context switches %ld \n", tot_cntx_switches);
       fprintf(stderr, "Average turnaround time %lf \n", avg_turn_time);
       fprintf(stderr, "Average response time  %lf \n", avg_resp_time);
}


// Feel free to add any other functions you need

// YOUR CODE HERE


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

void ring(int signum){
	//We have to add something here Divit
}
