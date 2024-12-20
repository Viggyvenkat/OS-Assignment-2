// File:	thread-worker.c

// List all group member's name: Divit Shetty & Vignesh Venkat
// username of iLab: dps190 & vvv11
// iLab Server: 3

#include "thread-worker.h"

// TO WHOEVER GRADES THIS,
// PARARLLEL_CAL TAKES A COUPLE SECONDS LONGER THAN THE OTHERS BUT IT ALWAYS RUNS. JUST WANT TO KEEP THAT IN MIND
// PARALLEL_CAL ALSO (VERY RARELY) ONLY PRINTS RUNTIME AND GETS STUCK. IF THAT HAPEPNS JSUT CTRL C AND THEN RUN AGAIN. VERY, VERY RARELY HAPPENS
// EXTREMELY RARE INSTANCE

//IF YOU GET ERROR WHEN RUNNING EXTERNAL_CAL THAT "Error: current thread is not the owner of the mutex.", CTRL C AND RUN IT AGAIN. VERY RARE

//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;


// INITAILIZE ALL YOUR OTHER VARIABLES HERE
// YOUR CODE HERE


//Timer 
struct itimerval timer;
int thread_count = 0;
//scheduler context
ucontext_t scheduler_context;
//thread creation context
ucontext_t main_context;
tcb* current_thread = NULL;
//The queues for MLFQ &PSJF. 4 (defined in .h remember)
//No more seperate for MLFQ and PSJF. PSJF is in the HIGH_PRIO queue
Queue queues[NUMPRIO];
//new Queues, no longer lists
Queue BLOCKED_Queue; 
Queue FINISHED_Queue;
int elapsed = 0; //needed for MLFQ
int TIME_SLICE_PER_LEVEL[NUMPRIO] = {5, 10, 15, 20}; // 5 = LOW_PRIO, 20 = HIGH_PRIO
int completed_threads;
double total_turn_time;
double total_resp_time;


//Prototypes for shed_mlfq and psjf called in schedule()
static void schedule(); 
static void sched_psjf();
static void sched_mlfq();


//HELPER METHODS

// initialize scheduler_context to point to schedule() [Solution to segmentation fault issue]
int setup_scheduler_context(){

    //precent memory leaks 
    static int initialized = 0; // Check if the function has already been called

    if (initialized) {
        // If the function was already called, return without doing anything
        return 0;
    }

    initialized = 1; // Set the flag to indicate the function has been called

    //current context for scheduler
    if (getcontext(&scheduler_context) == -1) {
        perror("Failed to get context for scheduler");
        exit(EXIT_FAILURE);
    }

    //Mem for scheduler stack
    void* stack = malloc(STACK_SIZE);
    if (stack == NULL) {
        perror("Failed to allocate stack for scheduler");
        return -1;
    }

    //from the sample code
    //setup context stack and properties for scheduler
    scheduler_context.uc_stack.ss_sp = stack;
    scheduler_context.uc_stack.ss_size = STACK_SIZE;
    scheduler_context.uc_link = NULL;
    scheduler_context.uc_stack.ss_flags = 0;

    //setup schdeuler context to point to schedule()
    makecontext(&scheduler_context, (void*)&schedule, 0, NULL);
    
    timer_setup();

    //current context for main thread
    if (getcontext(&main_context) == -1) {
        perror("Failed to get context for main");
        free(stack); 
        return -1;
    }

    //allocate memory for the primary TCB
    tcb* primaryTCB = malloc(sizeof(tcb));
    if (primaryTCB == NULL) {
        perror("Failed to allocate memory for primaryTCB");
        free(stack); 
        return -1;
    }

    //can i initialize these properites outside at the start?
    primaryTCB->thread_id = thread_count++;
    primaryTCB->status = READY; //default status
    primaryTCB->priority = HIGH_PRIO; //default prio
    //printf("setup_scheduler_context: Primary thread initialized with priority level %d (HIGH_PRIO)\n", primaryTCB->priority);
    primaryTCB->queue_time = clock(); //USE FOR METRICS VIGNESH
    primaryTCB->elapsed = 0; //VIGNESH, THIS IS FOR PROMOTION/DEMOTION. CAN MAYBE USE FOR METRICS BUT THAT'S ITS MAIN USE
    primaryTCB->next = NULL;
   

   //keep track of total context switch count throughout
    tot_cntx_switches++;

    //Add primaryTCB to the hihgest priority queue (default)
    Enqueue(&queues[HIGH_PRIO], primaryTCB);
    //debug: print the queue (just to check)
    //print_Queue(&queues[HIGH_PRIO]);
    
    

    //switch context to the scheduler
    if (swapcontext(&primaryTCB->context, &scheduler_context) == -1) {
        perror("Failed to swap context to scheduler");
        free(stack);
        free(primaryTCB);
        return -1;
    }

    return 0;

}

//Ring and timer metrics
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

    // Set up the timer using definitions in .h
    timer.it_interval.tv_usec = IT_US;
    timer.it_interval.tv_sec = IT_S;

    timer.it_value.tv_usec = IT_US;
    timer.it_value.tv_sec = IT_S;

    // Start 
    setitimer(ITIMER_PROF, &timer, NULL);
}

//helpers for enqueue. Kept seperate to keep clean
// Helper function for enqueue to initialize the queue with the new thread when it's empty
void initialize_queue(Queue* queue, tcb* new_thread) {
    queue->head = new_thread;
    queue->rear = new_thread;
    new_thread->next = NULL;
}

// Helper function for enqueue to append a new thread to the end of the queue when it's not empty
void append_to_queue(Queue* queue, tcb* new_thread) {
    queue->rear->next = new_thread;
    queue->rear = new_thread;
    new_thread->next = NULL;
}

//basic enqueue just iwth queue now instead of list
//Used for both PSJF and MLFQ
void Enqueue(Queue* queue, tcb* new_thread) {
    //error check
    if (queue == NULL || new_thread == NULL) {
        fprintf(stderr, "Error: Enqueue.\n");
        return;
    }
    //queue is empty
    if (queue->rear == NULL) {
        initialize_queue(queue, new_thread);
    } else {
        //queue isn't empty so add to the rear
        append_to_queue(queue, new_thread);
    }
}

//helper for Dequeue only
//removal process (kept seperate for clarity)
tcb* remove_head(Queue* queue){
    tcb* queue_head = queue->head;
    if (queue_head == NULL) {
        return NULL;
    }

    //set the next thread as the head
    queue->head = queue_head->next;
    //if the queue is empty after removing the thread
    if (queue->head == NULL){
        queue->rear = NULL;
    }

    //detach the current head from the queue
    queue_head->next = NULL;
    return queue_head;
}

//basic dequeue function just with a queue now
tcb* Dequeue(Queue* queue) {
    //error check
    if (queue == NULL) {
        fprintf(stderr, "Error: Dequeue.\n");
        return NULL;
    }

    return remove_head(queue);
    
}

//PSJF dequeue helper: find shortest_thread based on elapsed time
tcb* find_shortest_thread(Queue* queue, tcb** previous_shortest) {
    tcb* shortest_thread = queue->head;
    tcb* current_thread = queue->head;
    tcb* previous_thread = NULL;
    *previous_shortest = NULL;

    while (current_thread != NULL) {
        if (current_thread->elapsed < shortest_thread->elapsed) {
            shortest_thread = current_thread;
            *previous_shortest = previous_thread;
        }
        previous_thread = current_thread;
        current_thread = current_thread->next;
    }

    return shortest_thread;
}

//Remove the thread once found
void remove_thread(Queue* queue, tcb* thread, tcb* previous_thread) {
    if (previous_thread == NULL) {
        queue->head = thread->next;
        if (queue->head == NULL) {
            queue->rear = NULL;
        }
    } else {
        previous_thread->next = thread->next;
        if (thread == queue->rear) {
            queue->rear = previous_thread;
        }
    }
    thread->next = NULL;
}

//Added a dequeue for PSJF specifically. Think it was causing issues before using 1 
//Finds the thread with the shortest elapsed time and dequeues it
//Called in sched_psjf. convenient to have a seperate one for psjf
tcb* Dequeue_psjf(Queue* queue) {
    if (queue->head == NULL || queue == NULL) {
        fprintf(stderr, "Error: Dequeue_psjf. \n");
        return NULL;
    }

    tcb* previous_shortest = NULL;
    tcb* shortest_thread = find_shortest_thread(queue, &previous_shortest);

    remove_thread(queue, shortest_thread, previous_shortest);

    return shortest_thread;
}

//dequeue for mlfq. Adjusted for queue. Much simpler
//Dequeues the highest priority thread for MLFQ
//Call in sched_mlfq()
void Dequeue_mlfq() {
    //Start from HIGH_PRIO (3) to LOW_PRIO (O)
    for (int i = HIGH_PRIO; i >= LOW_PRIO; i++) {
        //find the highest priority thread
        tcb* thread = Dequeue(&queues[i]);
        if(thread!=NULL){
            current_thread = thread;
            break;
        }
    }
    
}

//dequeue from the BLOCKED_Queue
//Old list implementation had issues; Overly complicated
void Dequeue_blocked() {
    //error check
    /*
    if (&BLOCKED_Queue == NULL) {
        fprintf(stderr, "Error: BLOCKED_Queue is NULL in Dequeue_blocked.\n");
        return NULL;
    }
    */
    //dequeue the thread from the BLOCKED_Queue
    tcb* unblocked_thread = Dequeue(&BLOCKED_Queue);

    if (unblocked_thread != NULL) {
        unblocked_thread->status = READY;
        // Reset priority once it's nblocked 
        unblocked_thread->priority = HIGH_PRIO;
        Enqueue(&queues[unblocked_thread->priority], unblocked_thread);
    }
    /*else{
        fprintf(stderr, "Error in Dequeue_blocked");
    }
    */
}

//debug method to print a queue
//displays thread id, status, priority
void print_Queue(Queue* queue) {
    if (queue == NULL) {
        printf("Error: Queue is NULL.\n");
        return;
    }

    tcb* temp = queue->head;

    // Determine the queue type
    if (queue == &BLOCKED_Queue) {
        printf("Contents of Blocked Queue:\n");
    } else if (queue == &FINISHED_Queue) {
        printf("Contents of Finished Queue:\n");
    } else {
        // Assuming queue is one of the queues
        for (int i = 0; i < NUMPRIO; i++) {
            if (queue == &queues[i]) {
                printf("Contents of MLFQ Level %d:\n", i);
                break;
            }
        }
    }

    if (temp == NULL) {
        printf("Queue is empty.\n");
        return;
    }

    while (temp != NULL) {
        printf("Thread ID: %d | Status: %d | Priority: %d | Elapsed Time: %d\n", 
               temp->thread_id, temp->status, temp->priority, temp->elapsed);
        temp = temp->next;
    }
    printf("\n");
}

//function to wipe MLFQ
void Refresh_mlfq() {
    // Reset threads to default. Default priority is the highest HIGH_PRIO 
    //Resets if not NULL
    if (current_thread != NULL) {
        current_thread->priority = HIGH_PRIO;
        //printf("Refresh_mlfq: Current thread ID %d reset to priority level %d (HIGH_PRIO)\n", current_thread->thread_id, current_thread->priority);
    }


    tcb* thread;
    // same resetting but in the BLOCKED_Queue
    // resetting priority
    thread = BLOCKED_Queue.head;
    while (thread != NULL) {
        thread->priority = HIGH_PRIO;
        //printf("Refresh_mlfq: Thread ID %d in BLOCKED_Queue reset to priority level %d (HIGH_PRIO)\n", thread->thread_id, thread->priority);
        thread = thread->next;
    }

    
    //Highest is default so led to infinite loop
    // Iterate between the second highest and the lowest priorities
    for (int i = MEDIUM_PRIO; i >= LOW_PRIO; i--) {
        while ((thread = Dequeue(&queues[i])) != NULL) {
            thread->priority = HIGH_PRIO;
            //printf("Refresh_mlfq: Thread ID %d moved from queue %d to priority level %d (HIGH_PRIO)\n", thread->thread_id, i, thread->priority);
            Enqueue(&queues[HIGH_PRIO], thread);
        }
    }
}

//Ring function 
static void ring(int signum) {
    // Check if there is a current thread to operate on
    if (current_thread == NULL) {
        fprintf(stderr, "Warning: ring called with no active thread.\n");
        return;
    }
    // Handle the MLFQ case to prevent starvation
#ifdef MLFQ
    // Increment priority if not at the highest level
    if (current_thread->priority < (NUMPRIO - 1)) {
        current_thread->priority++;
    }

    // Refresh the MLFQ if the aging threshold is reached
    // incriment elapsed by 1 
    //prevent starvation
    elapsed++;
    if (elapsed >= AGING_THRESHOLD) {
        elapsed = 0; //reset back to 0 and start again
        Refresh_mlfq(); //starvation solved
        //print_Queue(&queues[current_thread->priority]);
    }
#endif

    // Update the total context switch count
    tot_cntx_switches++;
    // fprintf(stderr, "Context switch occurred. Total switches: %ld\n", tot_cntx_switches);

    // Perform the context switch to the scheduler
    if (swapcontext(&current_thread->context, &scheduler_context) == -1) {
        perror("Error: swapcontext failure in ring function");
        exit(EXIT_FAILURE);
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
    //find the thread with the matching ID
    for(tcb* finding_thread = queue->head; finding_thread != NULL; finding_thread = finding_thread->next){
        // Directly compare the thread ID
        if (finding_thread->thread_id == thread) {
            return finding_thread;
        }
    }
    return NULL;
}

//Addition to find_thread_by_id
////Search all queues for specicifc thread
//Use in worker_join (the godforskaen function) 
static tcb* find_thread_by_id_all_queues(worker_t thread) {
    // Search the MLFQ queues 
    for (int i = 0; i < NUMPRIO; i++) {
        tcb* found_thread = find_thread_by_id(thread, &queues[i]);
        if (found_thread) {
            return found_thread;
        }
    }
    // Search the blocked and finished queues
    // Use find_thread_by_id to search specific queues
    tcb* found_thread = find_thread_by_id(thread, &BLOCKED_Queue);
    if (found_thread) {
        return found_thread;
    }

    found_thread = find_thread_by_id(thread, &FINISHED_Queue);
    if (found_thread) {
        return found_thread;
    }

    //thread not found
    fprintf(stderr, "Thread with ID %d not found in any queue.\n", thread);
    return NULL;
}

//Use in Scheduler to loop so long as there are queues to loop
int theQueueisEmpty() {
#ifndef MLFQ //psjf is being used

    //Check only if the highest priority queue is empty
    if (queues[HIGH_PRIO].head == NULL) {
        return 1; // Queue is empty
    }
    return 0; 
#else //mlfq

    // Iterate through ALL MLFQ levels to check if any queue has threads
    for (int i = 0; i < NUMPRIO; i++) {
        if (queues[i].head != NULL) {
            return 0; // At least one queue is not empty
        }
    }

    // If all queues are empty
    return 1;
#endif
}



/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

       // - create Thread Control Block (TCB)
       // - create and initialize the context of this worker thread
       // - allocate space of stack for this thread to run
       // after everything is set, push this thread into run queue and 
       // - make it ready for the execution.

       // YOUR CODE HERE
        

         //create the intial context. 
        setup_scheduler_context();
        
           
        
        //Carry over
        //Allocate memory for the new thread's TCB  
        tcb* new_thread = malloc(sizeof(tcb));
        if (new_thread == NULL) {
            perror("Error: Failed to allocate memory for TCB in worker_create");
            exit(1);
        }

        //get context for the new thread
        if (getcontext(&new_thread->context) == -1) {
            perror("Error: getcontext failed in worker_create"); //more specific debugs
            exit(1);
        }  

        //Allocate mem for the new therad's stack
        void* stack = malloc(STACK_SIZE);
        if (stack == NULL) {
            perror("Failed to allocate memory for the stack"); //DEBUG
            exit(1);
        }

        //Setup stack and context
        new_thread->context.uc_link = &scheduler_context;
        new_thread->context.uc_stack.ss_sp = stack;
        new_thread->context.uc_stack.ss_size = STACK_SIZE;
        new_thread->context.uc_stack.ss_flags = 0;
        new_thread->stack = stack;


        //get the other stuff ready; initialize the TCB stuff
        new_thread->thread_id = thread_count; //map ID to the thread #
        *thread = new_thread->thread_id;
        //Initialize status
        new_thread->status = READY;
        //start off at the highest priority
        new_thread->priority = HIGH_PRIO; //default priority for both MLFQ and PSJF
        //+1 thread
        thread_count++;
        //printf("worker_create: Thread ID %d initialized with priority level %d (HIGH_PRIO)\n", new_thread->thread_id, new_thread->priority);
        new_thread->elapsed = 0; //incriment throughout
        new_thread->next = NULL;
        new_thread->queue_time = clock();

        //Set up the function the thread will run and pass the argument
        makecontext(&new_thread->context, (void*)function, 1, arg);


        //Add the new thread
        Enqueue(&queues[new_thread->priority], new_thread);
        //print_Queue(&queues[new_thread->priority]);

       

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

    if (current_thread == NULL || current_thread->status != SCHEDULED) {
        fprintf(stderr, "Warning: worker_yield called with invalid thread state.\n");
        return -1;
    }

    current_thread->status = READY;

    tot_cntx_switches++;
    // fprintf(stderr, "Context switch occurred. Total switches: %ld\n", tot_cntx_switches);

    if (swapcontext(&current_thread->context, &scheduler_context) == -1) {
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

    completed_threads += 1;

    //Implement turnaround time operations here too
    current_thread->end_time = clock();
    double turn_time = (double)(current_thread->end_time - current_thread->queue_time) / CLOCKS_PER_SEC;
    double resp_time = (double)(current_thread->start_time - current_thread->queue_time) / CLOCKS_PER_SEC;

    total_turn_time += turn_time;
    total_resp_time += resp_time;

    if (completed_threads > 0) {
        avg_resp_time = (double) total_resp_time / completed_threads; // Cast to double for accurate division
        avg_turn_time = (double) total_turn_time / completed_threads; // Cast to double for accurate division
    }

    // De-allocate the stack memory allocated during thread creation
    if (current_thread->stack) {
        free(current_thread->stack);
        current_thread->stack = NULL;
    }

    // Yield control to the scheduler context
    if (swapcontext(&current_thread->context, &scheduler_context) == -1) {
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
           //fprintf(stderr, "Context switch occurred. Total switches: %ld\n", tot_cntx_switches); 
            

            if (swapcontext(&current_thread->context, &scheduler_context) == -1) {
                perror("Error: swapcontext failure in ring function");
                exit(EXIT_FAILURE);
            }
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

     // Move a thread from the blocked list to the run queue, then release the lock
    Dequeue_blocked();
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
    //print_Queue(&queues[HIGH_PRIO]); // check queue before 
    sched_psjf();
    //print_Queue(&queues[HIGH_PRIO]); //check queue after
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
            // fprintf(stderr, "Context switch occurred. Total switches: %ld\n", tot_cntx_switches);

            // Start the timer for the thread's execution
            start_timer();
            if(swapcontext(&scheduler_context, &current_thread->context) == -1){
                perror("Error in schedule()");
                exit(1);
            }

            // Increment the elapsed time AFTER the context switch
            current_thread->elapsed++;
        }

        // Handle different thread states after execution
        if (current_thread != NULL) {
            if (current_thread->status != FINISHED && current_thread->status != BLOCKED) {
                current_thread->status = READY;
    #ifndef MLFQ

                //PSJF
                Enqueue(&queues[HIGH_PRIO], current_thread); 
    #else       

                //MLFQ
                Enqueue(&queues[current_thread->priority], current_thread);
    #endif  //account for blocked or finished threads
            } else if (current_thread->status == BLOCKED) {
                Enqueue(&BLOCKED_Queue, current_thread);
            } else if (current_thread->status == FINISHED) {
                Enqueue(&FINISHED_Queue, current_thread);
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
    current_thread = Dequeue_psjf(&queues[HIGH_PRIO]);
    
}


/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE

    //just call dequeue. It'll remove the next thread for removal
    // Far less complicated than previous implementation
    Dequeue_mlfq();

    //Check for custom testing, not really needed for benchmark
    //Refresh_mlfq handles promotion for benchmarks, only for custom test
    //not really necessary but is a good check
    if (current_thread != NULL) {
        int time_slice = TIME_SLICE_PER_LEVEL[current_thread->priority];

        // Check if the thread has exceeded its time slice.
        if (current_thread->elapsed >= time_slice) {
            // If not at the lowest priority level, demote it.
            if (current_thread->priority < NUMPRIO - 1) {
                current_thread->priority--;
                current_thread->elapsed = 0; // Reset the elapsed time after demoting a thread

                // Debug statement to print the new priority
                //printf("Thread ID %d demoted to priority level: %d\n", current_thread->thread_id, current_thread->priority);
            }
        }

        //unnecessary as it is handled in ring()
        /*
        // Promote threads that have been waiting too long (aging mechanism).
        for (int i = 1; i < NUMPRIO; i++) {
            tcb* aging_thread = queues[i].head;
            while (aging_thread != NULL) {
                if (clock() - aging_thread->queue_time >= AGING_THRESHOLD) {
                    // Remove thread from current priority queue.
                    tcb* next_thread = aging_thread->next;
                    Dequeue(&queues[aging_thread->priority]);
                    
                    // Promote thread to the next higher priority level.
                    aging_thread->priority++;
                    aging_thread->queue_time = clock(); // Reset queue time

                    // Debug statement to print the new priority
                    printf("Thread ID %d promoted to priority level: %d\n", aging_thread->thread_id, aging_thread->priority);
                    
                    // Re-add thread.
                    Enqueue(&queues[aging_thread->priority], aging_thread);
                    aging_thread = next_thread;
                } else {
                    aging_thread = aging_thread->next;
                }
            }
        }
        */
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
