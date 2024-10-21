#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "thread-worker.h"
#include "thread-worker.c"

#define NUM_THREADS 4 //for MLFQ testing
#define MAX_WAIT_COUNT 1000 // Adjust this as needed for debugging

// Dummy function to simulate workload
void* worker_function(void* arg) {
    int thread_id = *(int*)arg;
    printf("Thread %d: Started\n", thread_id);

    // Simulate work by looping
    for (volatile int i = 0; i < 1000000 * (thread_id + 1); i++);

    printf("Thread %d: Finished work, now exiting.\n", thread_id);
    worker_exit(NULL); // Properly exit the thread
    return NULL;
}

int main() {
    worker_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    // Initialize the thread IDs and create the threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i + 1;
        worker_create(&threads[i], NULL, worker_function, &thread_ids[i]);
        printf("Thread %d created.\n", i + 1);
    }

    // Main thread waits for each created thread to finish
    for (int i = 0; i < NUM_THREADS; i++) {
        printf("Waiting for thread %d to terminate...\n", i + 1);
        int wait_count = 0;

        // Use worker_join with a timeout check
        while (worker_join(threads[i], NULL) != 0) {
            wait_count++;
            if (wait_count >= MAX_WAIT_COUNT) {
                printf("Error: Thread %d did not terminate in time. Possible deadlock or error.\n", i + 1);
                exit(EXIT_FAILURE);
            }
        }

        printf("Thread %d has terminated.\n", i + 1);
    }

    printf("All threads have finished execution.\n");
    return 0;
}