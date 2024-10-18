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
#include <string.h>
#include <signal.h>

//was getting an error so just gonna define psjf and mlfq here first
static void sched_psjf(void);
static void sched_mlfq(void);


//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;


// INITAILIZE ALL YOUR OTHER VARIABLES HERE
// YOUR CODE HERE
ucontext_t scheduling_context, main_context, current_context;
Queue* runqueue = NULL;
struct itimerval timer;
struct sigaction sa;
tcb* main_thread;
tcb* cur_thread;
int scheduling_init= 0;
//needed for sched_mlfq();
Queue* mlfq_queues[NUMPRIO];


//Wrapper function to match signature for makecontetx (got an error when trying to Make)
void thread_start_wrapper() {
   
    if (cur_thread && cur_thread->function) {
        cur_thread->function(cur_thread->arg);
    }
   
    worker_exit(NULL);
}

//

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

       // - create Thread Control Block (TCB)
       // - create and initialize the context of this worker thread
       // - allocate space of stack for this thread to run
       // after everything is set, push this thread into run queue and 
       // - make it ready for the execution.

       // YOUR CODE HERE
	   printf("creating a new thread \n"); //debug

	   if (scheduling_init == 0) {
		printf("Initializing scheduling context\n");

        scheduling_context.uc_stack.ss_sp = malloc(MAX_SIZE); 
        if (scheduling_context.uc_stack.ss_sp == NULL) {
            perror("Failed to allocate scheduling stack");
            exit(EXIT_FAILURE);
        }
        scheduling_context.uc_stack.ss_size = MAX_SIZE;
        scheduling_context.uc_stack.ss_flags = 0;
        scheduling_context.uc_link = NULL;

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = &ring;
        if (sigaction(SIGPROF, &sa, NULL) != 0) {
            perror("sigaction setup failed");
            exit(EXIT_FAILURE);
        }

        // Changing these to 1 millisecond. Timer fires every 1 millisecond
        timer.it_value.tv_usec = 0;
        timer.it_value.tv_sec = 1000; // 1 millisecond
        // Adding interval time quantum
        timer.it_interval.tv_sec = 0;
        timer.it_interval.tv_usec = 1000; // 1 millisecond
        if (setitimer(ITIMER_PROF, &timer, NULL) != 0) {
            perror("setitimer setup failed");
            exit(EXIT_FAILURE);
        }

        main_thread = (tcb *) malloc(sizeof(tcb));
        if (main_thread == NULL) {
            perror("Failed to allocate main thread");
            exit(EXIT_FAILURE);
        }

        if (getcontext(&(main_thread->context)) != 0) {
            perror("Failed to get context for main thread");
            exit(EXIT_FAILURE);
        }
        main_thread->context.uc_stack.ss_sp = malloc(MAX_SIZE);
        if (main_thread->context.uc_stack.ss_sp == NULL) {
            perror("Failed to allocate stack for main thread");
            exit(EXIT_FAILURE);
        }
        main_thread->context.uc_stack.ss_size = MAX_SIZE;
        main_thread->context.uc_link = &scheduling_context;
        main_thread->thread_id = 0;
        cur_thread = main_thread;

        // Initialize runqueue
        runqueue = (Queue *) malloc(sizeof(Queue));
        if (runqueue == NULL) {
            perror("Failed to allocate run queue");
            exit(EXIT_FAILURE);
        }
        runqueue->front = NULL;
        runqueue->rear = NULL;

        // Adding this to initialize MLFQ queues
        for (int i = 0; i < NUMPRIO; i++) {
            mlfq_queues[i] = (Queue*) malloc(sizeof(Queue));
            if (mlfq_queues[i] == NULL) {
                perror("Failed to allocate MLFQ queue");
                exit(EXIT_FAILURE);
            }
            mlfq_queues[i]->front = NULL;
            mlfq_queues[i]->rear = NULL;
        }

        scheduling_init = 1;
		printf("Scheduling initialized\n");
    }

    tcb* new_thread = (tcb*) malloc(sizeof(tcb));
    if (new_thread == NULL) {
        perror("Failed to allocate new thread");
        exit(EXIT_FAILURE);
    }

    // Adding to make my life easier with scheduling. DEFAULT Runtime is in .h file
    new_thread->remaining_time = DEFAULT_RUNTIME;
    new_thread->thread_id = *thread;
    new_thread->priority = DEFAULT_PRIO;
    new_thread->thread_stack = (thread_stack*) malloc(sizeof(thread_stack));
    if (new_thread->thread_stack == NULL) {
        perror("Failed to allocate thread stack");
        exit(EXIT_FAILURE);
    }
    new_thread->thread_stack->top = -1;

    // Set the function and argument passed to this thread
    new_thread->function = (void (*)(void *))function;
    new_thread->arg = arg; // Used by thread_start_wrapper

	//debug 
	printf("Setting up context for thread ID: %d\n", *thread);
    if (getcontext(&(new_thread->context)) != 0) {
        perror("Failed to get context for new thread");
        exit(EXIT_FAILURE);
    }
    new_thread->context.uc_stack.ss_sp = malloc(MAX_SIZE);
    if (new_thread->context.uc_stack.ss_sp == NULL) {
        perror("Failed to allocate stack for new thread");
        exit(EXIT_FAILURE);
    }
    new_thread->context.uc_stack.ss_size = MAX_SIZE;
    new_thread->context.uc_stack.ss_flags = 0;
    new_thread->context.uc_link = &scheduling_context;

	//debug before makecontext 
	printf("Making context for thread ID: %d\n", *thread);
    // Set up the context for the new thread
    makecontext(&(new_thread->context), (void (*)(void)) thread_start_wrapper, 0);

    new_thread->thread_status = THREAD_NEW;
    enqueue(runqueue, new_thread);

	printf("Thread created and enqueued\n");

    return 0;
};

//Helper for setshecprio
tcb* tcb_by_id(worker_t thread_id) {
    Node* current = runqueue->front;
    
    // Traverse the runqueue to find the thread with the given thread_id
    while (current != NULL) {
        if (current->data->thread_id == thread_id) {
            return current->data;
        }
        current = current->next;
    }
    
    return NULL;
}


/* This function gets called only for MLFQ scheduling set the worker priority. */
int worker_setschedprio(worker_t thread, int prio) {


   // Set the priority value to your thread's TCB
   // YOUR CODE HERE
   // Locate the TCB of the specified thread
    tcb* target_thread = tcb_by_id(thread);

    // Check if the priority value is within valid bounds
    if (prio < LOW_PRIO || prio > HIGH_PRIO) {
        printf("Invalid priority value.\n");
        return -1; 
    }

    // Set the priority value to the thread's TCB
    if (target_thread != NULL) {
        target_thread->priority = prio;
        return 0;
    } else {
        printf("Thread not found.\n");
        return -1; 
    }

   return 0;	

}




/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context


	// YOUR CODE HERE
	cur_thread->thread_status = READY;
	//enqueue the current thread back to the runqueue
	enqueue(runqueue, cur_thread);
	swapcontext(&(cur_thread->context), &scheduling_context);
	
	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread
	// YOUR CODE HERE
	cur_thread->thread_status = TERMINATED;
	setcontext(&scheduling_context);
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
	//initialize the mutex state
	mutex->locked = 0; // 0 = unlocked (not held by a thread)
    mutex->owner = NULL; //stores TCB of thread holding the mutex
    mutex->blocked_list = (Queue*)malloc(sizeof(Queue)); //Queue to manage threads waiting for mutex
    mutex->blocked_list->front = NULL;
	mutex->blocked_list->rear = NULL;
    return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        // YOUR CODE HERE
		// Atomic check if the mutex is free using atomic op
		if (__sync_lock_test_and_set(&(mutex->locked), 1) == 0) {
			// Mutex is free; current thread acquires it
			mutex->owner = cur_thread;
			return 0;
		}

		// Mutex is already locked; block the current thread
		cur_thread->thread_status = BLOCKED;
		enqueue(mutex->blocked_list, cur_thread);

		// Switch context to the scheduler
		swapcontext(&cur_thread->context, &scheduling_context);

        return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	if (mutex->owner != cur_thread) {
        return -1; 
    }

    // Release the mutex
    mutex->locked = 0;
    mutex->owner = NULL;

    // If there are any threads waiting on the mutex, wake one up
    tcb* next_thread = dequeue(mutex->blocked_list);
    if (next_thread != NULL) {
        
        next_thread->thread_status = READY;
        enqueue(runqueue, next_thread);
    }
	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init
	free(mutex->blocked_list);

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
	// Check if the run queue is empty
	printf("Scheduler called\n");

    if (runqueue == NULL || runqueue->front == NULL) {
        printf("No threads to schedule.\n");
        return;
    }

    // Dequeue the next thread from the run queue
    tcb* next_thread = dequeue(runqueue);

    if (next_thread != NULL) {
		printf("Scheduling thread ID: %d\n", next_thread->thread_id);
        // Set the current thread to the next thread
        cur_thread = next_thread;
        cur_thread->thread_status = READY;

       // Swap context from the scheduler to the selected thread
	   //Debug statement needs ot be commented out later
        if (swapcontext(&scheduling_context, &cur_thread->context) == -1) {
            perror("swapcontext failed");
            exit(EXIT_FAILURE);
        }
	}


// - schedule policy
#ifndef MLFQ
	// Choose PSJF
	sched_psjf();
#else 
	// Choose MLFQ
	sched_mlfq();
#endif

}

//helper for psjf to remove a thread from a queue
// can't use dequeue cause psjf needs to remove from anywhere, not just the fron
void remove_from_queue(Queue* queue, tcb* thread) {
    if (queue->front == NULL) return;

    Node* temp = queue->front;
    Node* prev = NULL;

    // Find the node containing the thread
    while (temp != NULL) {
        if (temp->data == thread) {
            if (prev == NULL) {
                queue->front = temp->next;
            } else {
                prev->next = temp->next;
            }
            if (temp == queue->rear) {
                queue->rear = prev;
            }
            free(temp);
            return;
        }
        prev = temp;
        temp = temp->next;
    }
}


/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf(void) {
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
	// Check if runqueue is empty
    if (runqueue == NULL || is_empty(runqueue)) {
        printf("No threads to schedule.\n"); //debug statement
        return;
    }

    tcb* shortest_thread = NULL;
    Node* current = runqueue->front;

    // Iterate through runqueue to find the thread with the shortest remaining time
    while (current != NULL) {
        tcb* thread = current->data;
        if (shortest_thread == NULL || thread->remaining_time < shortest_thread->remaining_time) {
            shortest_thread = thread;
        }
        current = current->next;
    }

    // If tge thread with the shortest remaining time is found, schedule it
    if (shortest_thread != NULL) {
       
        cur_thread = shortest_thread;
        cur_thread->thread_status = SCHEDULED;

        
        remove_from_queue(runqueue, cur_thread);

       
        swapcontext(&scheduling_context, &cur_thread->context);
    } else {
        printf("PSJF: No threads available.\n"); //debug, remove before submitting
    }

	
}


/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq(void) {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
	//call in schedule()
	//Iterate iver queues fom high to low priority
    for (int i = HIGH_PRIO; i >= LOW_PRIO; i--) {
        // Check if there's a thread in the current priority queue
        if (!is_empty(mlfq_queues[i])) {
            // Dequeue the next thread from the highest non-empty queue
            tcb* next_thread = dequeue(mlfq_queues[i]);

            // Check if the thread has used up its time quantum
            if (next_thread->remaining_time <= 0) {
                // Demote the thread to a lower priority if it's not at the lowest level
                if (next_thread->priority > LOW_PRIO) {
                    worker_setschedprio(next_thread->thread_id, next_thread->priority - 1);
                }

                // Enqueue the thread back into its new priority queue
                enqueue(mlfq_queues[next_thread->priority], next_thread);
                continue; 
            }

            // Set the current thread to the next thread
            cur_thread = next_thread;
            cur_thread->thread_status = SCHEDULED;

            // Swap context from the scheduler to the selected thread
            swapcontext(&scheduling_context, &cur_thread->context);
            return;
        }
    }

    // Debug statement to check if no threads are found
    printf("MLFQ: No threads available in any queue.\n");

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

int is_full(thread_stack *stack) {
    return stack->top == (MAX_SIZE - 1);
}

int is_empty(Queue* queue) {
    return (queue->front == NULL);
}
//isempty for stacks
int is_empty_stack(thread_stack *stack) {
    return stack->top == -1;
}



int push(thread_stack *stack, worker_t value) {
    if (is_full(stack)) {
        return 0;
    }
    stack->arr[++(stack->top)] = value;  
    return 1;
}

int pop(thread_stack *stack, worker_t *value) {
    if (is_empty_stack(stack)) {
        return 0;
    }
    *value = stack->arr[(stack->top)--];  
    return 1;
}

int peek(thread_stack *stack, worker_t *value) {
    if (is_empty_stack(stack)) {
        return 0;
    }
    *value = stack->arr[stack->top];  
    return 1;
}

void enqueue(struct Queue* queue, tcb* new_thread) {
    struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
    newNode->data = new_thread;
    newNode->next = NULL;
    if (queue->rear == NULL) {
        queue->front = queue->rear = newNode;
        return;
    }
    queue->rear->next = newNode;
    queue->rear = newNode;
}

tcb* dequeue(struct Queue* queue) {
    if (queue->front == NULL)
        return NULL;

    struct Node* temp = queue->front;
    tcb* thread = temp->data;
    queue->front = queue->front->next;

    if (queue->front == NULL){
        queue->rear = NULL;
	}

    free(temp);
    return thread;
}

void ring(int signum){
	//We have to add something here Divit
	//Goal is to save the state of cur_thread and switch to the scheduler
	//Incrimenting total context switches

	//debug statement
	printf("Timer interrupt. Switching context \n");
	tot_cntx_switches++;

	//Switch from current thread context to scheduler context 
	if (cur_thread && swapcontext(&(cur_thread->context), &scheduling_context) == -1) {
		//if swapcontext fails
		perror("swapcontext (failed in ring)");
		exit(EXIT_FAILURE);
	}
}
