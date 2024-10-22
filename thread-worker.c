// File:	thread-worker.c

// List all group member's name: Divit Shetty & Vignesh Venkat
// username of iLab: dps190
// iLab Server: 3

#include "thread-worker.h"

//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;


// INITAILIZE ALL YOUR OTHER VARIABLES HERE
// YOUR CODE HERE


int thread_count = 0;
//scheduler context
ucontext_t scheduler_context;
ucontext_t benchmark;
//thread creation context
ucontext_t main_context;
//Timer 
struct itimerval timer;
//new Queues, no longer lists
Queue blockQueue; 
Queue finishedQueue;
//The queues for MLFQ. 4 (defined in .h remember)
Queue mlfq_queues[NUMPRIO];
int initialContextCall = 1; //use in worker_create to make sure scheduler initial context is only called once
tcb* current_thread = NULL;
int elapsed = 0;
int TIME_SLICE_PER_LEVEL[NUMPRIO] = {5, 10, 15, 20}; 


//Prototypes for shed_mlfq and psjf called in schedule()
static void schedule(); 
static void sched_psjf();
static void sched_mlfq();


//HELPER METHODS

// initialize scheduler_context to point to schedule() [Solution to segmentation fault issue]
int setup_scheduler_context(){

    if (getcontext(&scheduler_context) == -1) {
        perror("Failed to get context for scheduler");
        exit(EXIT_FAILURE);
    }

    void* stack = malloc(STACK_SIZE);
    if (stack == NULL) {
        perror("Failed to allocate stack for scheduler");
        return -1;
    }

    scheduler_context.uc_link = NULL;
    scheduler_context.uc_stack.ss_sp = stack;
    scheduler_context.uc_stack.ss_size = STACK_SIZE;
    scheduler_context.uc_stack.ss_flags = 0;

    //setup schdeuler context to point to schedule()
    makecontext(&scheduler_context, (void*)&schedule, 0, NULL);
    
    timer_setup();

     if (getcontext(&main_context) == -1) {
        perror("Failed to get context for main");
        free(stack); 
        return -1;
    }

    tcb* primaryTCB = malloc(sizeof(tcb));
    if (primaryTCB == NULL) {
        perror("Failed to allocate memory for primaryTCB");
        free(stack); 
        return -1;
    }

    //can i initialize these outside at the start?
    primaryTCB->thread_id = thread_count++;
    primaryTCB->status = READY;
    primaryTCB->priority = HIGH_PRIO;
    primaryTCB->elapsed = 0;
    primaryTCB->next = NULL;
    primaryTCB->queue_time = clock(); //USE FOR METRICS VIGNESH
    
    //enqueue primaryTCB to the hihgest priority queue (default)
    enqueue(&mlfq_queues[HIGH_PRIO], primaryTCB);
    tot_cntx_switches++;

    if (swapcontext(&primaryTCB->context, &scheduler_context) == -1) {
        perror("Failed to swap context to scheduler");
        free(stack);
        free(primaryTCB);
        return -1;
    }

    return 0;

}

//basic enqueue just iwth queue now
void enqueue(Queue* queue, tcb* thread) {
    //error check
    if (queue == NULL || thread == NULL) {
        fprintf(stderr, "Error: enqueue.\n");
        return;
    }

    if (queue->rear == NULL) {
        queue->head = queue->rear = thread;
    } else {
        queue->rear->next = thread;
        queue->rear = thread;
    }
    thread->next = NULL;
}

//basic dequeue function just with a queue now
tcb* dequeue(Queue* queue) {
    //error check
    if (queue == NULL) {
        fprintf(stderr, "Error: dequeue.\n");
        return NULL;
    }
    tcb* thread = queue->head;
    if (thread == NULL) {
        return NULL;
    }
    queue->head = thread->next;
    if (queue->head == NULL){
        queue->rear = NULL;
    }

    thread->next = NULL;
    return thread;
}

//enqueue_mlfq doesn't need priority anymore as a param, just the thread
//called in schedule() not in the sched_mlfq(). Same idea tho with the loop. Enqueue called after is different tho
void enqueue_mlfq(tcb* thread) {
    if (thread->priority >= NUMPRIO) {
        thread->priority = NUMPRIO - 1;
    }
    enqueue(&mlfq_queues[thread->priority], thread);
}


//Added a dequeue for PSJF specifically. Think it was causing issues before using 1 dequeue
//Called in sched_psjf. convenient to have a seperate one for psjf
tcb* dequeue_psjf(Queue* queue) {
    if (queue->head == NULL || queue == NULL) {
        fprintf(stderr, "Error: dequeue_psjf. \n");
        return NULL;
    }

    // Initialize pointers
    tcb* shortest_thread = queue->head; 
    tcb* current_thread = queue->head;
    tcb* previous_thread = NULL;
    tcb* previous_shortest = NULL;

    // Iterate through the queue to find the thread with the shortest elapsed time
    while (current_thread != NULL) {
        if (current_thread->elapsed < shortest_thread->elapsed) {
            shortest_thread = current_thread;
            previous_shortest = previous_thread;
        }
        previous_thread = current_thread;
        current_thread = current_thread->next;
    }

    // Remove the shortest_thread from the queue
    if (previous_shortest == NULL) {
        // The shortest thread is at the head of the queue
        queue->head = shortest_thread->next;
        if (queue->head == NULL) {
            queue->rear = NULL; 
        }
    } else {
        previous_shortest->next = shortest_thread->next;
        if (shortest_thread == queue->rear) {
            queue->rear = previous_shortest;
        }
    }

    
    shortest_thread->next = NULL;

    return shortest_thread;
}

//dequeue for mlfq. Adjusted for queue. Much simpler
//Dequeues the highest priority thread for MLFQ
//Call in sched_mlfq()
void dequeue_mlfq() {
    for (int i = 0; i < NUMPRIO; i++) {
        if ((current_thread = dequeue(&mlfq_queues[i])) != NULL) {
            return;
        }
    }
    
}

//Dequeue from the blockQueue
//Old list implementation had issues; Overly complicated
void dequeue_blocked() {
    //error check
    /*
    if (&blockQueue == NULL) {
        fprintf(stderr, "Error: blockQueue is NULL in dequeue_blocked.\n");
        return NULL;
    }
    */
    tcb* temp = dequeue(&blockQueue);

    if (temp != NULL) {
        temp->status = READY;
        // Reset priority when unblocked 
        temp->priority = HIGH_PRIO;
        enqueue(&mlfq_queues[temp->priority], temp);
    }
}

//function to wipe MLFQ
void reset_mlfq() {
    // Reset threads to default. Default priority is the highest HIGH_PRIO 
    //Resets if not NULL
    if (current_thread != NULL) {
        current_thread->priority = HIGH_PRIO;
    }

    tcb* temp;

    // same resetting but in the blockQueue
    // resetting priority
    temp = blockQueue.head;
    while (temp != NULL) {
        temp->priority = HIGH_PRIO;
        temp = temp->next;
    }

    // Iterate over all priority levels in the MLFQ,
    for (int i = 1; i < NUMPRIO; i++) {
        while ((temp = dequeue(&mlfq_queues[i])) != NULL) {
            temp->priority = HIGH_PRIO;
            enqueue(&mlfq_queues[HIGH_PRIO], temp);
        }
    }
}

//Search a specific queue for a specific thread 
//Altered to fit with the Queue structure, now also takes a queue not just id
//Avoid the issue of not finidng threads when there're multiple runtimequeues
static tcb* find_thread_by_id(worker_t thread, Queue* queue) {
     if (queue == NULL) {
        fprintf(stderr, "Error: Invalid (null) queue passed to find_thread_by_id.\n");
        return NULL;
    }

    tcb* temp = queue->head;
    //find the thread with the matching ID
    while (temp != NULL) {
        if (temp->thread_id == thread) {
            return temp; //found it
        }
        temp = temp->next;
    }
    return NULL;
}

//Addition to find_thread_by_id
////Search all queues for specicifc thread
//Use in worker_join (the godforskaen function) 
static tcb* find_thread_by_id_all_queues(worker_t thread) {
    tcb* found_thread = NULL;

    // Search the MLFQ queues 
    for (int i = 0; i < NUMPRIO; i++) {
        if ((found_thread = find_thread_by_id(thread, &mlfq_queues[i])) != NULL) {
            return found_thread;
        }
    }
    // Search the blocked and finished queues
    // Use find_thread_by_id to search specific queues
    if ((found_thread = find_thread_by_id(thread, &blockQueue)) != NULL) {
        return found_thread;
    }
    if ((found_thread = find_thread_by_id(thread, &finishedQueue)) != NULL) {
        return found_thread;
    }

    //thread not found
    fprintf(stderr, "Thread with ID %d not found in any queue.\n", thread);
    return NULL;
}

//Use in Scheduler to loop so long as there are queues to loop
int theQueueisEmpty() {
#ifndef MLFQ //psjf is being used

    //Check if the highest priority queue is empty
    if (mlfq_queues[HIGH_PRIO].head == NULL) {
        return 1; // Queue is empty
    }
    return 0; // Queue is not empty
#else //mlfq

    // Iterate through all MLFQ levels to check if any queue has threads
    for (int i = 0; i < NUMPRIO; i++) {
        if (mlfq_queues[i].head != NULL) {
            return 0; // At least one queue is not empty
        }
    }

    // If all queues are empty
    return 1;
#endif
}

//MAIN CODE BELOW

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

       // - create Thread Control Block (TCB)
       // - create and initialize the context of this worker thread
       // - allocate space of stack for this thread to run
       // after everything is set, push this thread into run queue and 
       // - make it ready for the execution.

       // YOUR CODE HERE

         //create the intial context. Moved it here 
        //put it in the if to make sure it's only called once. TEst case called it twice by accident
        if (initialContextCall) {
            setup_scheduler_context();
            initialContextCall = 0;
        }
        
        //Carry over 
        tcb* new_thread = malloc(sizeof(tcb));
        if (new_thread == NULL) {
            perror("Error: Failed to allocate memory for TCB in worker_create");
            exit(1);
        }

        if (getcontext(&new_thread->context) < 0) {
            perror("Error: getcontext failed in worker_create"); //more specific debugs
            exit(1);
        }  

        //Stack is now a void 
        void* stack = malloc(STACK_SIZE);
        if (stack == NULL) {
            perror("Failed to allocate memory for the stack"); //DEBUG
            exit(1);
        }

        //Setup context
        // Stack and context
        new_thread->context.uc_link = &scheduler_context;
        new_thread->context.uc_stack.ss_sp = stack;
        new_thread->context.uc_stack.ss_size = STACK_SIZE;
        new_thread->context.uc_stack.ss_flags = 0;
        new_thread->stack = stack;

        //get the other stuff ready
        new_thread->thread_id = thread_count;
        *thread = new_thread->thread_id;
        thread_count++;
        //Initialize status
        new_thread->status = READY;
        //start off at the highest priority
        new_thread->priority = HIGH_PRIO; 
        new_thread->elapsed = 0;
        new_thread->next = NULL;
        new_thread->queue_time = clock();
        makecontext(&new_thread->context, (void*)function, 1, arg);


        //Add the new thread
        enqueue(&mlfq_queues[HIGH_PRIO], new_thread);

       

    return 0;
};


#ifdef MLFQ
/* This function gets called only for MLFQ scheduling set the worker priority. */
int worker_setschedprio(worker_t thread, int prio) {


   // Set the priority value to your thread's TCB
   // YOUR CODE HERE
   //use find_thread_by_id_all_queues helper to find the thread
   tcb* tcb_thread = find_thread_by_id_all_queues(thread);

   //DEBUGS
   if (tcb_thread == NULL) {
        perror("Thread not found");
        return -1; 
    }

    
    if (prio < 0 || prio >= NUMPRIO) {
        perror("Invalid priority");
        return -1; 
    }

   // Check if the priority is already set to avoid unnecessary updates
    if (tcb_thread->priority == prio) {
        return 0; 
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

    //ensure the current thread is valid
    /*
    if(current_thread != NULL){
        if(current_thread->status == SCHEDULED){
            current_thread->status == READY;

            //incriment the context switch count
            tot_cntx_switches++;
            swapcontext(&current_thread->context, &scheduler_context);
        }
    }
    */
    if (current_thread == NULL || current_thread->status != SCHEDULED) {
        fprintf(stderr, "Warning: worker_yield called with invalid thread state.\n");
        return -1;
    }

    current_thread->status = READY;

    tot_cntx_switches++;

    if (swapcontext(&current_thread->context, &scheduler_context) < 0) {
        perror("Error: swapcontext failed in worker_yield");
        exit(EXIT_FAILURE);
    }

	
	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread

	// YOUR CODE HERE
    stop_timer();// stop tracking time here
    current_thread->status = FINISHED;

    if (value_ptr) {
        current_thread->return_value = value_ptr;
    }

    //Implement turnaround time operations here too

    // De-allocate the stack memory allocated during thread creation
    if (current_thread->stack) {
        free(current_thread->stack);
        current_thread->stack = NULL;
    }

    // Yield control to the scheduler context
    if (swapcontext(&current_thread->context, &scheduler_context) < 0) {
        perror("Error: swapcontext failed in worker_exit");
        exit(1);
    }
};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
  
	// YOUR CODE HERE
    tcb* found_thread = find_thread_by_id_all_queues(thread);
    if (found_thread == NULL){
        fprintf(stderr, "Error: Thread with ID %d not found.\n", thread);
        return -1;
    }

    while(found_thread->status != FINISHED){
        worker_yield();
    }

    //save return value
    if (value_ptr) {
        *value_ptr = found_thread->return_value; 
    }

    //free stack mem
    if (found_thread->stack) {
        free(found_thread->stack);  
    }

    //free the tcb
    free(found_thread);
	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex

	// YOUR CODE HERE
    if (mutex == NULL){
        fprintf(stderr, "Error: Attempt to initialize a NULL mutex.\n");
        return -1;
    }
    if (mutex->initialize == 1){
        fprintf(stderr, "Error: Attempt to initialize an already initialized mutex.\n");
        return -1;
    }

    mutex->initialize = 1; 
    mutex->locked = 0;
    mutex->owner = NULL;
    mutex->blocked_list = NULL;

	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        // YOUR CODE HERE
        if (mutex == NULL){
            fprintf(stderr, "Error: Attempt to lock a NULL mutex.\n");
            return -1;
        }

        if (mutex->initialize == 0){
            fprintf(stderr, "Error: Attempt to lock an uninitialized mutex.\n");
            return -1;
        }

        while (__sync_lock_test_and_set(&(mutex->locked), 1)){
            // blocking_mech(mutex, current_thread);
            current_thread->status = BLOCKED;

            current_thread->next = mutex->blocked_list;
            mutex->blocked_list = current_thread;

            tot_cntx_switches++;

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
        fprintf(stderr, "Error: mutex is NULL.\n");
        return -1;
    }
    if (mutex->initialize == 0 || mutex->locked == 0) {
        fprintf(stderr, "Error: mutex is either not initialized or already unlocked.\n");
        return -1;
    }
    if (mutex->owner != current_thread) {
        fprintf(stderr, "Error: current thread is not the owner of the mutex.\n");
        return -1;
    }

    dequeue_blocked();

    // Release the lock
    mutex->locked = 0;
    mutex->owner = NULL;
	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init
    //check if the mutex is valid
    if (mutex == NULL) {
        fprintf(stderr, "Error: Attempt to destroy a NULL mutex.\n");
        return -1;
    }
    if (mutex->initialize == 0) {
        fprintf(stderr, "Error: Attempt to destroy an uninitialized mutex.\n");
        return -1;
    }

    if (mutex->locked == 1) {
        fprintf(stderr, "Error: Attempt to destroy a locked mutex.\n");
        return -1;
    }

    mutex->initialize = 0;
    mutex->locked = 0;
    mutex->owner = NULL;
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

    while (!theQueueisEmpty()) {
        stop_timer();

// - schedule policy
#ifndef MLFQ
	// Choose PSJF
    sched_psjf();
#else 
	// Choose MLFQ
    sched_mlfq();
#endif

    if (current_thread != NULL) {
            // Set the start time if this is the first time the thread is scheduled
            if (current_thread->start_time == 0) {
                current_thread->start_time = clock();
            }

           
            current_thread->status = SCHEDULED;
            tot_cntx_switches++; 

            // Start the timer for the thread's execution
            start_timer();
            swapcontext(&scheduler_context, &current_thread->context);

            // Increment the elapsed time AFTER the context switch
            current_thread->elapsed++;
        }

        // Handle different thread states after execution
        if (current_thread != NULL) {
            if (current_thread->status != FINISHED && current_thread->status != BLOCKED) {
                current_thread->status = READY;
    #ifndef MLFQ
                enqueue(&mlfq_queues[HIGH_PRIO], current_thread); 
    #else
                enqueue_mlfq(current_thread);
    #endif
            } else if (current_thread->status == BLOCKED) {
                enqueue(&blockQueue, current_thread);
            } else if (current_thread->status == FINISHED) {
                enqueue(&finishedQueue, current_thread);
            }
        }
    }
}

/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE


    //i kept psjf in the highest priority
    //easier than having 2 
    current_thread = dequeue_psjf(&mlfq_queues[HIGH_PRIO]);
    

}


/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE

    //just call dequeue. It'll dequeue the next thread
    // Far less omplicated than previous implementation
    dequeue_mlfq();

    if (current_thread != NULL) {
        int time_slice = TIME_SLICE_PER_LEVEL[current_thread->priority];

        // Check if the thread has exceeded its time slice.
        if (current_thread->elapsed >= time_slice) {
            // If not at the lowest priority level, demote it.
            if (current_thread->priority < NUMPRIO - 1) {
                current_thread->priority++;
                current_thread->elapsed = 0; // Reset the elapsed time after demoting a thread
            }
        }

        // Promote threads that have been waiting too long (aging mechanism).
        for (int i = 1; i < NUMPRIO; i++) {
            tcb* aging_thread = mlfq_queues[i].head;
            while (aging_thread != NULL) {
                if (clock() - aging_thread->queue_time >= AGING_THRESHOLD) {
                    // Remove thread from current priority queue.
                    tcb* next_thread = aging_thread->next;
                    dequeue(&mlfq_queues[aging_thread->priority]);
                    
                    // Promote thread to the next higher priority level.
                    aging_thread->priority--;
                    aging_thread->queue_time = clock(); // Reset queue time
                    
                    // Re-enqueue thread.
                    enqueue(&mlfq_queues[aging_thread->priority], aging_thread);
                    aging_thread = next_thread;
                } else {
                    aging_thread = aging_thread->next;
                }
            }
        }
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

// YOUR CODE HERE
//HELPER FUNCTIONS 
//RING AND METRIC FUNCTIONS

//Ring function
static void ring(int signum) {
    if (current_thread != NULL) {
        //MLFQ case to prevent starvation
#ifdef MLFQ
        if (current_thread->priority < NUMPRIO - 1) current_thread->priority++;
        if (++elapsed >= AGING_THRESHOLD) {
            elapsed = 0;
            reset_mlfq();
        }
#endif
        tot_cntx_switches++;
        // Debug message for monitoring context switches
        //fprintf(stderr, "Context switch triggered. Total switches: %ld\n", tot_cntx_switches);
        if (swapcontext(&current_thread->context, &scheduler_context) < 0) {
            perror("Error: swapcontext failed in ring");
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "Warning: ring was called but current_thread is NULL.\n");
    }
}

//Metric stuff
//Helper timer functions
static void start_timer() {
    timer.it_interval.tv_usec = IT_US; 
    timer.it_interval.tv_sec = IT_S;

    timer.it_value.tv_usec = IT_US;
    timer.it_value.tv_sec = IT_S;
    setitimer(ITIMER_PROF, &timer, NULL);
}

static void stop_timer() {
    timer.it_interval.tv_usec = 0;
    timer.it_interval.tv_sec = 0;

    timer.it_value.tv_usec = 0;
    timer.it_value.tv_sec = 0;
    setitimer(ITIMER_PROF, &timer, NULL);
}

//Timer setup function
//Moved from worker_create() to save space
//now called when initialzing context
void timer_setup() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &ring;
    sigaction(SIGPROF, &sa, NULL);

    // Set up the timer
    timer.it_interval.tv_usec = IT_US;
    timer.it_interval.tv_sec = IT_S;

    timer.it_value.tv_usec = IT_US;
    timer.it_value.tv_sec = IT_S;

    // Start 
    setitimer(ITIMER_PROF, &timer, NULL);
}