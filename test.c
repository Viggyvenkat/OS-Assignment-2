#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "thread-worker.h"
#include "thread-worker.c"


void* test_function(void* arg) {
    int thread_id = *(int*)arg;
    printf("Thread %d is running.\n", thread_id);

    // Simulate some work with multiple yields
    for (int i = 0; i < 5; i++) {
        printf("Thread %d yielding...\n", thread_id);
        worker_yield();
    }

    printf("Thread %d is finishing.\n", thread_id);
    worker_exit(NULL);
    return NULL;
}

int main() {
    worker_t threads[4];
    int thread_args[4];

    // Create 4 threads with different priorities
    for (int i = 0; i < 4; i++) {
        thread_args[i] = i;
        printf("Creating thread %d with priority %d\n", i, i);

        if (worker_create(&threads[i], NULL, test_function, &thread_args[i]) != 0) {
            fprintf(stderr, "Error creating thread %d.\n", i);
            return -1;
        }

#ifdef MLFQ
        // Set initial priority for the threads using worker_setschedprio
        worker_setschedprio(threads[i], i);
#endif
    }

    // Wait for all threads to finish
    for (int i = 0; i < 4; i++) {
        worker_join(threads[i], NULL);
    }

    // Print the application statistics after all threads are finished
    print_app_stats();

    return 0;
}