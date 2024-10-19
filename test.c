#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "thread-worker.h"

#define NUM_THREADS 4 //for MLFQ testing



/* A scratch program template on which to call and
 * test thread-worker library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */
// Sample function to be run by each thread
// A simple function for each thread to execute
void *compute_sum(void *arg) {
    int *data = (int *)arg;
    int sum = 0;
    for (int i = 0; i < 1000; ++i) {
        sum += data[i];
    }
    printf("Thread completed with sum: %d\n", sum);
    return NULL;
}

//Step 1.2 Test (Testing worker_yield)// Define sample_function above main
void *sample_function(void *arg) {
    int value = *(int *)arg;
    for (int i = 0; i < 3; ++i) {
        printf("Thread %d yielding, iteration %d\n", value, i);
        worker_yield(); // Yield control back to the scheduler
    }
    printf("Thread %d finished execution.\n", value);
    return NULL;
}

// Sample function for the dummy thread (timer)
void *dummy_function(void *arg) {
    int value = *(int *)arg;
    printf("Dummy thread running with value: %d\n", value);
    while (1) {
        // Keep the thread alive to observe the timer behavior
    }
    return NULL;
}

//Timer test
void test_timer() {
    printf("Setting up the timer...\n");
    setup_timer();
    
    // Loop to keep the main thread running so that we can observe the timer behavior
    while (1) {
        // You can print a message here or just keep it empty
    }
}

//Test function that the threads will execute
void *test_thread_function(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < 5; ++i) {
        printf("Thread %d executing iteration %d\n", id, i);
        worker_yield(); // Yield control back to the scheduler
    }
    printf("Thread %d finished execution.\n", id);
    return NULL;
}

//Test for PSJF scheduler
// Function to initialize and test the PSJF scheduler
void test_psjf_scheduler() {
    worker_t tid1, tid2, tid3;
    int arg1 = 1, arg2 = 2, arg3 = 3;

    // Create three threads with different identifiers
    if (worker_create(&tid1, NULL, test_thread_function, &arg1) == 0) {
        printf("Thread %u created successfully.\n", tid1);
    }

    if (worker_create(&tid2, NULL, test_thread_function, &arg2) == 0) {
        printf("Thread %u created successfully.\n", tid2);
    }

    if (worker_create(&tid3, NULL, test_thread_function, &arg3) == 0) {
        printf("Thread %u created successfully.\n", tid3);
    }

    // Assign the first thread in the runqueue as the current thread
    if (runqueue_head != NULL) {
        current_thread = runqueue_head; // Set the current thread to the first in the runqueue
    } else {
        printf("Runqueue is empty. No threads to schedule.\n");
        exit(1);
    }

    // Run the timer and scheduler
    test_timer();
}

//Antoher test for PSJF scheduler

// Sample function that threads will execute with varying workloads
void *variable_workload_function(void *arg) {
    int id = *(int *)arg;
    int workload = id * 2; // Vary workload based on thread ID (e.g., 2, 4, 6 iterations)
    for (int i = 0; i < workload; ++i) {
        printf("Thread %d executing iteration %d\n", id, i);
        worker_yield(); // Yield control back to the scheduler
    }
    printf("Thread %d finished execution.\n", id);
    return NULL;
}

// Test for PSJF scheduler with varying workloads
void test_varying_workloads() {
    worker_t tid1, tid2, tid3;
    int arg1 = 1, arg2 = 2, arg3 = 3;

    // Create three threads with varying workloads
    if (worker_create(&tid1, NULL, variable_workload_function, &arg1) == 0) {
        printf("Thread %u created successfully.\n", tid1);
    }

    if (worker_create(&tid2, NULL, variable_workload_function, &arg2) == 0) {
        printf("Thread %u created successfully.\n", tid2);
    }

    if (worker_create(&tid3, NULL, variable_workload_function, &arg3) == 0) {
        printf("Thread %u created successfully.\n", tid3);
    }

    // Start the timer and scheduler
    test_timer();
}

// Test for PSJF scheduler with two threads
void test_two_threads() {
    worker_t tid1, tid2;
    int arg1 = 1, arg2 = 2;

    // Create two threads
    if (worker_create(&tid1, NULL, test_thread_function, &arg1) == 0) {
        printf("Thread %u created successfully.\n", tid1);
    }

    if (worker_create(&tid2, NULL, test_thread_function, &arg2) == 0) {
        printf("Thread %u created successfully.\n", tid2);
    }

    // Start the timer and scheduler
    test_timer();
}

// Test for PSJF scheduler with four threads and staggered workloads
void test_four_threads_staggered() {
    worker_t tid1, tid2, tid3, tid4;
    int arg1 = 1, arg2 = 2, arg3 = 3, arg4 = 4;

    // Create four threads with staggered workloads
    if (worker_create(&tid1, NULL, variable_workload_function, &arg1) == 0) {
        printf("Thread %u created successfully.\n", tid1);
    }

    if (worker_create(&tid2, NULL, variable_workload_function, &arg2) == 0) {
        printf("Thread %u created successfully.\n", tid2);
    }

    if (worker_create(&tid3, NULL, variable_workload_function, &arg3) == 0) {
        printf("Thread %u created successfully.\n", tid3);
    }

    if (worker_create(&tid4, NULL, variable_workload_function, &arg4) == 0) {
        printf("Thread %u created successfully.\n", tid4);
    }


    // Ensure current_thread is set to the head of the runqueue before starting the timer
    if (runqueue_head != NULL) {
        current_thread = runqueue_head;
    } else {
        printf("Runqueue is empty. No threads to schedule.\n");
        exit(1);
    }

    // Start the timer and scheduler
    test_timer();
}
tcb* find_thread_by_id(worker_t thread);

// Simple thread function for MLFQ testing
void *simple_thread_function(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < 10; ++i) { // Run for 3 iterations
        printf("Thread %d executing iteration %d\n", id, i);
        worker_yield(); // Yield control back to the scheduler
    }
    printf("Thread %d finished execution.\n", id);
    // Set the thread status to finished
    tcb *own_thread = find_thread_by_id(id);
    if (own_thread != NULL) {
        own_thread->status = FINISHED;
    }
    return NULL;
}

#ifdef MLFQ
tcb* find_thread_by_id(worker_t thread);

#endif

// Test for MLFQ scheduler with two simple threads
void test_simple_mlfq_scheduler() {
    worker_t tid[2];
    int args[2] = {1, 2};

    // Create two threads
    for (int i = 0; i < 2; ++i) {
        if (worker_create(&tid[i], NULL, simple_thread_function, &args[i]) == 0) {
            printf("Thread %d created successfully.\n", args[i]);
        }
    }

    // Set the first thread as the current thread
    tcb *first_thread = find_thread_by_id(tid[0]);
    if (first_thread != NULL) {
        current_thread = first_thread;
    } else {
        printf("Error: Could not find the initial thread.\n");
        return;
    }

    // Run the timer and scheduler
    test_timer();
}

//Slightly harder MLFQ test
void *complex_thread_function(void *arg) {
    int id = *(int *)arg;


    for (int i = 0; i < 20; ++i) {
        if (current_thread == NULL || current_thread->thread_id != id) {
            printf("Error: current_thread does not match the expected thread %d.\n", id);
            return NULL;
        }

        printf("Thread %d executing iteration %d\n", id, i);
        worker_yield(); // Yield control back to the scheduler
    }

    printf("Thread %d finished execution.\n", id);
    current_thread->status = FINISHED; // Mark it as finished directly using current_thread
    return NULL;
}

// Test for more complex MLFQ behavior
void test_complex_mlfq_scheduler() {
    worker_t tid[NUM_THREADS];
    int args[NUM_THREADS] = {1, 2, 3, 4};

    // Create threads with different initial priorities and workloads
    for (int i = 0; i < NUM_THREADS; ++i) {
        if (worker_create(&tid[i], NULL, complex_thread_function, &args[i]) == 0) {
            printf("Thread %d created with priority %d\n", args[i], HIGH_PRIO);
        }
        worker_setschedprio(tid[i], HIGH_PRIO);
    }

    // Set the first thread as the current thread
    tcb *first_thread = find_thread_by_id(tid[0]);
    if (first_thread != NULL) {
        current_thread = first_thread;
    } else {
        printf("Error: Could not find the initial thread.\n");
        return;
    }

    // Run the timer and scheduler
    test_timer();
}

int main() {
    printf("Testing Complex MLFQ Scheduler...\n");
    test_complex_mlfq_scheduler();
    return 0;
}