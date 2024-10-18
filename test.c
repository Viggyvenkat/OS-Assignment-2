#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <stdint.h>
#include "thread-worker.h"


/* A scratch program template on which to call and
 * test thread-worker library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */

/* Test function for threads */
void* thread_function(void* arg) {
    int thread_id = *(int*)arg;
    for (int i = 0; i < 5; i++) {
        printf("Thread %d running, iteration %d\n", thread_id, i);
        worker_yield(); // Yield control to the scheduler
    }
    return NULL;
}

int main() {
    worker_t thread1, thread2, thread3;
    int id1 = 1, id2 = 2, id3 = 3;

    // Create three threads
    printf("Creating thread 1\n");
    worker_create(&thread1, NULL, thread_function, &id1);
    printf("Creating thread 2\n");
    worker_create(&thread2, NULL, thread_function, &id2);
    printf("Creating thread 3\n");
    worker_create(&thread3, NULL, thread_function, &id3);

    // Infinite loop allowing timer to call scheduler automatically
    while (1) {
        // If all threads have completed
        if (cur_thread == NULL && (runqueue == NULL || runqueue->front == NULL)) {
            printf("All threads completed.\n");
            break;
        }
        // To yield control manually in main
        worker_yield();
    }

    return 0;
}