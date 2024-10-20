// File:	thread-worker.c

// List all group member's name:
// username of iLab:
// iLab Server:

#include "thread-worker.h"

//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;

// INITAILIZE ALL YOUR OTHER VARIABLES HERE
// YOUR CODE HERE
static worker_t next_thread_id = 1; // Counter for assigning unique thread IDs
tcb *runqueue_head = NULL; // Global variable for the runqueue head
tcb *current_thread = NULL;    // Define the current running thread
ucontext_t scheduler_context;  // Define the scheduler context
bool scheduler_initialized = false; //to help setup scheduler context
tcb *mlfq_queues[NUMPRIO]; // Array of runqueues for MLFQ
int time_quantum[NUMPRIO] = {2, 4, 8, 8}; // Time quanta for MLFQ levels (lowest to highest ie; 8 =HIGHPRIO)

//function prototpyes that're needed cause schedule() is really far towards the bottom 
static void schedule();
static void sched_psjf();
static void sched_mlfq();

//initialize scheduler_context to point to schedule() [Solution to segmentation fault issue]
ucontext_t scheduler_context;
char scheduler_stack[STACK_SIZE];
void setup_scheduler_context() {
    // Initialize the scheduler context
    if (getcontext(&scheduler_context) == -1) {
        perror("Failed to get context for scheduler");
        exit(EXIT_FAILURE);
    }

    // Set up the stack for the scheduler context
    scheduler_context.uc_stack.ss_sp = scheduler_stack;
    scheduler_context.uc_stack.ss_size = STACK_SIZE;
    scheduler_context.uc_stack.ss_flags = 0;
    scheduler_context.uc_link = NULL; 

    // Set the context to execute the schedule function
    makecontext(&scheduler_context, schedule, 0);
}

//Helper functions that'll be used later on
void enqueue(tcb *thread) {
    if (runqueue_head == NULL) {
        runqueue_head = thread;
    } else {
        tcb *current = runqueue_head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = thread;
    }
    thread->next = NULL; // Ensure the new thread points to NULL
}

//dequeue is basically just taking code from psjf and making it its own function
tcb* dequeue() {
    
    if (runqueue_head == NULL) {
        printf("Runqueue is empty. No threads to schedule.\n");
        return NULL;
    }

    // Find the thread with the shortest remaining time
    tcb *next_thread = runqueue_head;
    tcb *prev = NULL;
    tcb *current = runqueue_head;
    tcb *prev_shortest = NULL;

    // Iterate through the runqueue to find the thread with the shortest remaining time
    while (current != NULL) {
        if (current->elapsed < next_thread->elapsed) {
            next_thread = current;
            prev_shortest = prev;
        }
        prev = current;
        current = current->next;
    }

    // Remove the selected thread from the runqueue
    // it's either at the head, or in the middle/end
    if (prev_shortest == NULL) {
        runqueue_head = next_thread->next;
    } else {
        prev_shortest->next = next_thread->next;
    }

    //dequeued thread shall not point to any others
    next_thread->next = NULL;

    return next_thread;
}

//MLFQ version of enqueue because I can't figure out how to use one function for both schedulers
void enqueue_mlfq(tcb *thread, int priority) {
    if (priority < 0 || priority >= NUMPRIO) {
        perror("Invalid priority level");
        exit(EXIT_FAILURE);
    }

    // Get the head of the queue for the given priority
    tcb **queue_head = &mlfq_queues[priority];

    if (*queue_head == NULL) {
        *queue_head = thread;
    } else {
        // Go to the end of the queue and add the thread
        tcb *current = *queue_head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = thread;
    }
    thread->next = NULL; 
}

//for mlfq again
tcb* dequeue_from_mlfq(int priority) {

    if (priority < 0 || priority >= NUMPRIO) {
        perror("Invalid priority level");
        exit(EXIT_FAILURE);
    }

    // get the head of the queue for the given priority
    tcb *head = mlfq_queues[priority];

    
    if (head == NULL) {
        return NULL;
    }

    // Update the head of the queue to the next thread
    mlfq_queues[priority] = head->next;

    // making sure the dequeued thread doesn't point to anything else
    head->next = NULL;

    
    return head;
}

//another
void init_mlfq_queues() {
    for (int i = 0; i < NUMPRIO; i++) {
        mlfq_queues[i] = NULL;
    }
}

//helper to be used in worker_setschedprio
tcb* find_thread_by_id(worker_t thread) {
    
    for (int i = 0; i < NUMPRIO; i++) {
        tcb *current = mlfq_queues[i]; // Get the head of the current priority queue
        while (current != NULL) {
            // Check if the current thread's ID matches the requested thread_id
            if (current->thread_id == thread) {
                return current; 
            }
            current = current->next; 
        }
    }
    // thread isn't found in any of the queues
    return NULL;
}

//Promotion function for MLFQ scheduler
void promote_threads() {
    // Iterate through lower-priority queues (except the highest one)
    for (int i = 0; i < NUMPRIO - 1; i++) {
        tcb *current = mlfq_queues[i];
        tcb *prev = NULL;

        while (current != NULL) {
            // Check if the thread crossed the threshold for promotion
            if (current->waiting_time >= AGING_THRESHOLD) {
                tcb *to_promote = current;

                // Remove the thread from the current queue
                if (prev == NULL) {
                    mlfq_queues[i] = current->next; //move the head 
                } else {
                    prev->next = current->next; //remove it from the middle/edn
                }

                current = current->next;

                // Promote the thread to the next higher queue
                to_promote->priority++;
                enqueue_mlfq(to_promote, to_promote->priority);
                to_promote->waiting_time = 0; // Reset waiting time

            } else {
                // If it's not promoted, increase the waiting time then move on to next thread
                current->waiting_time++;
                prev = current;
                current = current->next;
            }
        }
    }
}
#ifdef MLFQ
// Function to handle demotion of threads based on their time quantum
void demote_thread(tcb *thread) {
    int priority = thread->priority;
    
    // Debug: Show the current state of the thread before checking for demotion
    printf("Checking demotion for thread %d; priority: %d, elapsed: %d, time quantum: %d\n",
           thread->thread_id, priority, thread->elapsed, time_quantum[priority]);

    // Check if the thread's elapsed time has reached the time quantum for its priority level
    if (thread->elapsed >= time_quantum[priority] && priority > LOW_PRIO) {
        printf("Demoting thread %d from priority %d to %d\n",
               thread->thread_id, priority, priority - 1);
        thread->priority--; // Decrease the priority
        thread->elapsed = 0; // Reset the elapsed time after demotion
        printf("Thread %d demoted; priority is now %d and elapsed time reset to %d\n", 
            thread->thread_id, thread->priority, thread->elapsed);

    }
}
#endif

//came with the code
/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

       // - create Thread Control Block (TCB)
       // - create and initialize the context of this worker thread
       // - allocate space of stack for this thread to run
       // after everything is set, push this thread into run queue and 
       // - make it ready for the execution.

       // YOUR CODE HERE
	   // Allocate memory for the new thread control block

    //Check if the scheduler context is initialized
    if (!scheduler_initialized) {
        setup_scheduler_context();
        scheduler_initialized = true;
    }

    tcb *new_thread = malloc(sizeof(tcb));
    if (new_thread == NULL) {
        perror("Failed to allocate memory for thread"); //DEBUG
        return -1;
    }

    // Allocate stack for the new thread
    new_thread->stack = malloc(STACK_SIZE);
    if (new_thread->stack == NULL) {
        perror("Failed to allocate stack"); //DEBUG
        free(new_thread);
        return -1;
    }

    // Initialize the context
    if (getcontext(&new_thread->context) == -1) {
        perror("Failed to get context"); //DEBUG
        free(new_thread->stack);
        free(new_thread);
        return -1;
    }

    // Set up the context's stack
    new_thread->context.uc_stack.ss_sp = new_thread->stack;
    new_thread->context.uc_stack.ss_size = STACK_SIZE;
    new_thread->context.uc_stack.ss_flags = 0;
    new_thread->context.uc_link = NULL; // If this thread exits, control returns to the scheduler

    // Set the context to execute the function with the given argument
    makecontext(&new_thread->context, (void (*)(void))function, 1, arg);

    // Add the new thread to the runqueue
    new_thread->thread_id = next_thread_id++;
    new_thread->status = READY;
    new_thread->elapsed = 0; 
    new_thread->waiting_time = 0; 
    
    #ifdef MLFQ
        // Set the initial priority to highest
        new_thread->priority = HIGH_PRIO;
        printf("Thread %d created with priority %d\n", new_thread->thread_id, new_thread->priority);

        //add to queue based on its priority 
        enqueue_mlfq(new_thread, new_thread->priority);
    #else
        // PSJF
        enqueue(new_thread);
    #endif

    *thread = new_thread->thread_id;
    return 0;

};


#ifdef MLFQ
/* This function gets called only for MLFQ scheduling set the worker priority. */
int worker_setschedprio(worker_t thread, int prio) {


   // Set the priority value to your thread's TCB
   // YOUR CODE HERE

    // Find the thread based on thread_id
    // uses a helper that I made (messy to do it in here)
    tcb *tcb_thread = find_thread_by_id(thread); 

    if (tcb_thread == NULL) {
        perror("Thread not found");
        return -1; 
    }

    
    if (prio < 0 || prio >= NUMPRIO) {
        perror("Invalid priority");
        return -1; 
    }

    // Set the priority of the thread's TCB
    tcb_thread->priority = prio;

    return 0; 

}
#endif

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	// YOUR CODE HERE
	// Save the current thread's context
    if (getcontext(&current_thread->context) == -1) {
        perror("Failed to get context in worker_yield"); //DEBUG: REMOVE BEFORE SUBMITTING 
        return -1;
    }
    
    if (current_thread->status == FINISHED) {
        printf("Thread %d is finished and will not be requeued.\n", current_thread->thread_id);
        return 0;
    }

    // Update the thread's state to READY
    current_thread->status = READY;

    //Solved the issue of enqueue not working well with MLFQ
    //Use conditional compilation (like everything els in this file involving MLFQ) 
    #ifdef MLFQ
        current_thread->elapsed++;
        //check for demotion using the helper
        demote_thread(current_thread);
        
        enqueue_mlfq(current_thread, current_thread->priority);
    #else
        enqueue(current_thread);
    #endif


    // Swap context to the scheduler to pick the next thread
    if (swapcontext(&current_thread->context, &scheduler_context) == -1) {
        perror("Failed to swap context to scheduler"); //DEBUG: REMOVE BEFORE SUBMITTING 
        return -1;
    }

    return 0; 
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
    
    tcb *temp_ptr = runqueue_head;
    while (temp_ptr->next != current_thread) {
        temp_ptr = temp_ptr->next;
    }

    
    if (temp_ptr->next == current_thread) {
        if (value_ptr != NULL) {
            *(void **)value_ptr = current_thread->return_value;
        }

        
        temp_ptr->next = current_thread->next;

        
        if (current_thread->stack != NULL) {
            free(current_thread->stack);
        }

       
        free(current_thread);

        setcontext(&scheduler_context);
    }
}

/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
	// YOUR CODE HERE
	tcb* temp_ptr = runqueue_head;
    while (temp_ptr->next->thread_id != thread) {
        temp_ptr = temp_ptr->next;
    }

	tcb* thread_ptr = temp_ptr->next;
	while(thread_ptr->status != FINISHED);
	
	if (value_ptr != NULL) {
        *value_ptr = thread_ptr->return_value;
    }

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
    //debug
    printf("Calling schedule...\n");

// - schedule policy
#ifndef MLFQ
	// Choose PSJF
    sched_psjf();
#else 
	// Choose MLFQ
    sched_mlfq();
#endif

}

/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
    //this code was moved to dequeue and that'll be called here now
    tcb *next_thread = dequeue();

    // Check if the runqueue is empty
    if (next_thread == NULL) {
        printf("Runqueue is empty. No threads to schedule.\n");
        return; // Exit the program if no threads are left
    }
    

    // Set the selected thread as the current thread
    current_thread = next_thread;
    current_thread->status = SCHEDULED;

    // Switch to the next thread's context
    printf("Switching to thread %u\n", current_thread->thread_id);
    if (setcontext(&current_thread->context) == -1) {
        perror("Failed to switch context to the next thread");
        exit(EXIT_FAILURE);
    }
}

static void sched_mlfq() {
    // Iterate from highest priority to lowest
    for (int i = HIGH_PRIO; i >= LOW_PRIO; i--) { 
        if (mlfq_queues[i] != NULL) {
            printf("Found thread in priority queue %d\n", i);
            
            // Dequeue the thread with highest priority
            current_thread = dequeue_from_mlfq(i);

            // If the dequeued thread is finished, skip it
            while (current_thread != NULL && current_thread->status == FINISHED) {
                printf("Thread %d is finished, looking for the next thread.\n", current_thread->thread_id);
                current_thread = dequeue_from_mlfq(i); // Dequeue the next thread
            }

            // If no more threads are available in this queue, continue to the next priority level
            if (current_thread == NULL) {
                continue;
            }

            current_thread->status = SCHEDULED;

            printf("Switching to thread %u with priority %d\n", current_thread->thread_id, i);
            
            // Switch to the selected thread
            setcontext(&current_thread->context); 
            break;
        }
    }
}

//The timer functions that call schedule()
// Signal handler for the timer
void ring(int signum) {
    
    //debug for testing 
    printf("timer triggered\n");
    //check 
    if (current_thread == NULL) {
        perror("No current thread initialized.");
        exit(EXIT_FAILURE);
    }
    // Save the context of the current thread
    if (getcontext(&current_thread->context) == -1) {
        perror("Failed to get context in timer_handler");
        return;
    }
    
    #ifdef MLFQ
    current_thread->elapsed++;
    // Check for demotion based on time quantum 
    if (current_thread->elapsed >= time_quantum[current_thread->priority]) {
        printf("Demoting thread %d from priority %d to %d\n",
               current_thread->thread_id, current_thread->priority, current_thread->priority - 1);
        worker_yield(); // demote if needed
    }else{
        printf("No demotion for thread %d; elapsed: %d, time quantum: %d\n",
               current_thread->thread_id, current_thread->elapsed, time_quantum[current_thread->priority]);
    }

    // Call the promotion function to prevent starvation
    promote_threads();
    #endif

    // Add the current thread back to the runqueue if it's still running
    if (current_thread->status == SCHEDULED) {
        current_thread->status = READY;
        
        //solution for ring's MLFQ problem
        #ifdef MLFQ
        enqueue_mlfq(current_thread, current_thread->priority);
        #else
        enqueue(current_thread);
        #endif
    }

    // Call the scheduler to select the next thread
    schedule();
}

//Took from worker_create
// Worker_create was too messy so I took parts and made it its own function
void setup_timer() {
    struct sigaction sa;
    struct itimerval timer;

    // Configure the signal handler for SIGALRM
    sa.sa_handler = &ring;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("Failed to set up signal handler");
        exit(EXIT_FAILURE);
    }

    // Set the timer interval (100ms intervals)
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 100000; // First trigger after 100ms
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 100000; // Subsequent triggers every 100ms

    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("Failed to set up timer");
        exit(EXIT_FAILURE);
    }
}

//DO NOT MODIFY THIS FUNCTION
/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void) {

       fprintf(stderr, "Total context switches %ld \n", tot_cntx_switches);
       fprintf(stderr, "Average turnaround time %lf \n", avg_turn_time);
       fprintf(stderr, "Average response time  %lf \n", avg_resp_time);
}


// Feel free to add any other functions you need