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
tcb *terminated_queue_head = NULL;
tcb *blocked_list = NULL;


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

//helper to check if the psjf and mlfq queues are empty
int areTheQueuesEmpty() {
    #ifndef MLFQ
        return (runqueue_head == NULL);
    #else
        // For MLFQ: iterate through each priority level queue
        for (int i = HIGH_PRIO; i >= LOW_PRIO; i--) {
            if (mlfq_queues[i] != NULL) {
                return 0; 
            }
        }
        return 1; 
    #endif
}

//Helper functions that'll be used later on
//WORKING
void enqueue(tcb *thread) {
    if (runqueue_head == NULL) {
        runqueue_head = thread;
        //printf("Thread %d added as head of the run queue.\n", thread->thread_id);
    } else {
        tcb *current = runqueue_head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = thread;
        //printf("Thread %d added to the run queue.\n", thread->thread_id);
    }
    thread->next = NULL; // Ensure the new thread points to NULL
}

//dequeue is basically just taking code from psjf and making it its own function
//WORKING
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
        thread->next = NULL; 
    } else {
        // Go to the end of the queue and add the thread
        tcb *current = *queue_head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = thread;
        
        printf("Thread %d added to priority queue %d.\n", thread->thread_id, priority); // Debug
    }
    thread->next = NULL;
    // Debug: Print the state of the queue after adding the thread
    printf("Priority queue %d after enqueue: ", priority);
    tcb *current = mlfq_queues[priority];
    while (current != NULL) {
        printf("Thread ID: %d -> ", current->thread_id);
        current = current->next;
    }
    printf("NULL\n");
}

//for terminated stuff
void enqueue_terminated(tcb *thread) {
    if (terminated_queue_head == NULL) {
        terminated_queue_head = thread;
    } else {
        tcb *current = terminated_queue_head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = thread;
    }
    thread->next = NULL; // Ensure the terminated thread points to NULL
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

    printf("Dequeued thread %d from priority queue %d.\n", head->thread_id, priority); // Debug

    // Debug: Print the state of the queue after dequeuing
    printf("Priority queue %d after dequeue: ", priority);
    tcb *current = mlfq_queues[priority];
    if (current == NULL) {
        printf("empty\n");
    } else {
        while (current != NULL) {
            printf("Thread ID: %d -> ", current->thread_id);
            current = current->next;
        }
        printf("NULL\n");
    }

    return head;
}

//another
void init_mlfq_queues() {
    for (int i = 0; i < NUMPRIO; i++) {
        mlfq_queues[i] = NULL;
    }
}

//helper to debug worker_join
void print_runqueue() {
    tcb *current = runqueue_head;
    printf("Current run queue: ");
    while (current != NULL) {
        printf("Thread ID: %d, Status: %d -> ", current->thread_id, current->status);
        current = current->next;
    }
    printf("NULL\n");
}

// Function to print the MLFQ queue at a specific priority level
void print_mlfq_queue(int priority) {
    // Ensure the priority is within the valid range
    if (priority < 0 || priority >= NUMPRIO) {
        printf("Invalid priority level: %d\n", priority);
        return;
    }

    // Start with the head of the queue at the given priority level
    tcb *current = mlfq_queues[priority];
    
    // If the queue is empty, print an appropriate message
    if (current == NULL) {
        printf("empty\n");
        return;
    }

    // Iterate through the queue and print each thread's information
    while (current != NULL) {
        printf("Thread ID: %d, Priority: %d, Waiting Time: %d -> ", current->thread_id, current->priority, current->waiting_time);
        current = current->next;
    }
    printf("NULL\n");
}

//helper to be used in worker_setschedprio
//was only working with MLFQ. Altered to work with both
tcb* find_thread_by_id(worker_t thread) {
    
    #ifdef MLFQ
        //Iterate through priority queues
        for (int i = 0; i < NUMPRIO; i++) {
            tcb *current = mlfq_queues[i]; // Get the head of the current priority queue
            while (current != NULL) {
                printf("Checking thread ID %d with status %d...\n", current->thread_id, current->status);
                // Check if the current thread's ID matches the requested thread_id
                if (current->thread_id == thread) {
                    printf("Thread %d found in the MLFQ run queue.\n", thread);
                    return current; 
                }
                current = current->next; 
            }
        }
    #else
        // Iterate through the single run queue
        tcb *current = runqueue_head;
        while (current != NULL) {
            printf("Checking thread ID %d with status %d...\n", current->thread_id, current->status);
            if (current->thread_id == thread) {
                printf("Thread %d found in the PSJF run queue.\n", thread);
                return current;
            }
            current = current->next;
            
        }
    #endif

    
    printf("Thread %d not found in the run queue.\n", thread);
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

        // Remove the thread from its current priority queue
        tcb *prev = NULL;
        tcb *current = mlfq_queues[priority];
        
        while (current != NULL && current != thread) {
            prev = current;
            current = current->next;
        }

        if (current == thread) {
            if (prev == NULL) {
                // If the thread is at the head of the queue
                mlfq_queues[priority] = current->next;
            } else {
                // If the thread is in the middle or end of the queue
                prev->next = current->next;
            }
        }

        // Decrease the priority
        thread->priority--;
        thread->elapsed = 0; // Reset the elapsed time after demotion
        
        // Re-enqueue the thread in its new priority queue
        enqueue_mlfq(thread, thread->priority);

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
        printf("Thread %d created with priority %d\n", new_thread->thread_id, new_thread->priority); //debug

        //add to queue based on its priority 
        enqueue_mlfq(new_thread, new_thread->priority);
    #else
        // PSJF
        printf("Creating thread with ID %d\n", new_thread->thread_id);
        enqueue(new_thread);
        //debug
        //print_runqueue();

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
    // Ensure the current thread is valid
    // Ensure the current thread is valid
    if (current_thread != NULL) {
        
        current_thread->status = READY;

        /*
        // Re-enqueue the current thread into the appropriate queue
        #ifdef MLFQ
            enqueue_mlfq(current_thread, current_thread->priority);
        #else
            enqueue(current_thread);
        #endif
        */
        
        tot_cntx_switches++;

        
        printf("Thread %d yielding...\n", current_thread->thread_id);
        if (swapcontext(&current_thread->context, &scheduler_context) == -1) {
            perror("Error: Failed to swap context in worker_yield");
            return -1;
        }
    }
    return 0;

    
}

/* terminate a thread */
void worker_exit(void *value_ptr) {	
    // - de-allocate any dynamic memory created by the thread
	// YOUR CODE HERE

    if (current_thread == NULL) return;
   
    //disable_timer();
    current_thread->status = FINISHED;

    // If there's a return value, store it in the thread's control block
    if (value_ptr) {
        current_thread->return_value = value_ptr;
    }
    
    /*
    enqueue_terminated(current_thread);
    */
    //setcontext(&scheduler_context);
}


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
	// YOUR CODE HERE
    // Find the thread based on the thread ID
    tcb* joining_thread = find_thread_by_id(thread);
    if (joining_thread == NULL) {
        fprintf(stderr, "Error: Thread with ID %d not found.\n", thread);
        return -1;
    }

    printf("Waiting for thread ID %d to terminate...\n", thread);

    // Wait until the thread's status changes to TERMINATED
    while (joining_thread->status != FINISHED) {
         // Yield to let other threads run
         worker_yield();
    }

    // Retrieve the return value if provided
    if (value_ptr) {
        *value_ptr = joining_thread->return_value;
    }

    // Free up the thread resources
    if (joining_thread->stack) {
        free(joining_thread->stack);
    }
    free(joining_thread);

    return 0;
}

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex
	// YOUR CODE HERE

    if (mutex == NULL) {
        return -1;
    }
    if (mutex->initialize == 1) {
        return -1;
    }

    mutex->initialize = 1; //initialize 
    mutex->locked = 0;
    mutex->owner = NULL;
    mutex->blocked_list = NULL;

    return 0;

};

/*
void blocking_mech(worker_mutex_t *mutex, worker_t *current_thread){
    current_thread->status = BLOCKED;

    current_thread->next = mutex->blocked_list;
        mutex->blocked_list = current_thread;

   
    swapcontext(&current_thread->context, &scheduler_context);

}
*/

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {
    // - use the built-in test-and-set atomic function to test the mutex
    // - if the mutex is acquired successfully, enter the critical section
    // - if acquiring mutex fails, push current thread into block list and
    // context switch to the scheduler thread

    // YOUR CODE HERE

    if (mutex == NULL) {
        return -1; 
    }

    if (mutex->initialize == 0) {
        return -1;
    }

    

    while (__sync_lock_test_and_set(&(mutex->locked), 1)) {
        //blocking_mech(mutex, current_thread);
        current_thread->status = BLOCKED;

        current_thread->next = mutex->blocked_list;
        mutex->blocked_list = current_thread;

   
        swapcontext(&current_thread->context, &scheduler_context);
        
    }

    mutex->owner = current_thread; 

    return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.
	// YOUR CODE HERE
    if (mutex == NULL) {
        return -1; 
    }

    if (mutex->owner != current_thread) {
        fprintf(stderr, "Error: Thread %d does not own the mutex.\n", current_thread->thread_id);
        return -1; 
    }
    // Debug: Print information before modifying the blocked list
    printf("Thread %d releasing the mutex. Checking blocked list...\n", current_thread->thread_id);

    // Move threads from blocked_list to the run queue
    tcb *blocked_thread = mutex->blocked_list;
    mutex->blocked_list = NULL;

    while (blocked_thread != NULL) {
        tcb *next_thread = blocked_thread->next;
        blocked_thread->status = READY;

        // Enqueue the thread based on the scheduling algorithm
        #ifdef MLFQ
            enqueue_mlfq(blocked_thread, blocked_thread->priority);
        #else
            enqueue(blocked_thread);
        #endif

        blocked_thread = next_thread;
    }

    // Release the lock
    mutex->locked = 0;
    mutex->owner = NULL;

    // Debug: Confirm that the mutex is unlocked
    printf("Mutex released by thread %d.\n", current_thread->thread_id);

    return 0; 
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init
    if (mutex == NULL) {
        return -1; // Return an error if mutex is NULL
    }
    if (mutex->initialize == 0 || mutex->locked == 1) {
        return -1;
    }

    // Reset all fields of the mutex
    mutex->initialize = 0;
    mutex->locked = 0;
    mutex->owner = NULL;

    if (mutex->blocked_list) {
        free(mutex->blocked_list);
        mutex->blocked_list = NULL;
    }
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

    while(!areTheQueuesEmpty()){

// - schedule policy
#ifndef MLFQ
	// Choose PSJF
    sched_psjf();
#else 
	// Choose MLFQ
    sched_mlfq();
#endif

    // If there's no current thread, continue to the next iteration
        if (current_thread == NULL) {
            continue;
        }

        // Set the thread status to running
        current_thread->status = SCHEDULED;

        // Context switch to the current thread
        if (current_thread != NULL) {
            tot_cntx_switches++;
            swapcontext(&scheduler_context, &current_thread->context);
            current_thread->elapsed++;
        }

        // Determine what to do with the thread after it returns to the scheduler
        if (current_thread->status == FINISHED) {
            printf("Thread %d finished execution.\n", current_thread->thread_id);
        } else if (current_thread->status == BLOCKED) {
            // Add the blocked thread to the blocked list
            current_thread->next = blocked_list;
            blocked_list = current_thread;
        } else {
            // Re-enqueue the thread
            current_thread->status = READY;
            #ifndef MLFQ
                enqueue(current_thread);
            #else
                enqueue_mlfq(current_thread, current_thread->priority);
            #endif
        }
    }
    printf("All threads have finished execution. Exiting.\n");
    exit(0);
   
}

/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
    //this code was moved to dequeue and that'll be called here now
   // Get the next thread from the runqueue with the shortest remaining time
    tcb *next_thread = dequeue();

    // If there's no thread to run, just return
    if (next_thread == NULL) {
        printf("No threads available in PSJF scheduler.\n");
        return;
    }

    // Set this thread as the current thread
    current_thread = next_thread;

    // Update the status to indicate it’s scheduled
    current_thread->status = SCHEDULED;

    // Debug statement for the chosen thread
    printf("PSJF: Switching to thread with ID: %d\n", current_thread->thread_id);
}


static void sched_mlfq() {
    // Iterate from highest to lowest priority
    for (int i = HIGH_PRIO; i >= LOW_PRIO; i--) { 
        if (mlfq_queues[i] != NULL) {
            printf("Found thread in priority queue %d\n", i);
            
            current_thread = dequeue_from_mlfq(i);
            if (current_thread == NULL) {
                continue;
            }
            //printf("Dequeued the thread %d!\n", current_thread->thread_id);

            // If the dequeued thread is finished, continue
            while (current_thread != NULL && current_thread->status == FINISHED) {
                printf("Thread %d is finished, looking for the next thread.\n", current_thread->thread_id);
                current_thread = dequeue_from_mlfq(i);
            }

            // If a valid thread is found, set its status and exit the function
            if (current_thread != NULL) {
                current_thread->status = SCHEDULED;
                printf("Switching to thread %u with priority %d\n", current_thread->thread_id, i);
                return;
            }
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
            current_thread->priority = current_thread->priority - 1;
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