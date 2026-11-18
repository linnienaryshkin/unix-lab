#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned long target = 1000000UL;
/*
 * These variables are shared by both threads. Atomic operations update a
 * value indivisibly, preventing conflicts between threads. Ordinary
 * counter++ could lose an update because it separates read, add, and write.
 */
static atomic_ulong counter = 0;
static atomic_int winner = 0;

/* Both workers run this function and share counter and winner. */
static void *increment_until_target(void *arg)
{
    int id = *(int *)arg;
    unsigned long progress_interval = target / 100;
    /*
     * Relaxed ordering keeps this load atomic without ordering unrelated data;
     * the counter itself is the only shared value this operation coordinates.
     */
    unsigned long current = atomic_load_explicit(&counter, memory_order_relaxed);

    if (progress_interval == 0)
        progress_interval = 1;

    /*
     * Compare-and-exchange changes counter to current + 1 only if counter
     * still equals current. Otherwise it refreshes current (or may fail
     * spuriously), and the worker retries. This prevents lost increments and
     * ensures the counter cannot pass target.
     */
    while (current < target)
    {
        if (atomic_compare_exchange_weak_explicit(
                &counter, &current, current + 1,
                memory_order_relaxed, memory_order_relaxed))
        {
            unsigned long progress = current + 1;

            /* Report progress once per percent without flooding the terminal. */
            if (progress % progress_interval == 0)
            {
                if (printf("Thread %d progress: %lu/%lu\n", id, progress, target) < 0 ||
                    fflush(stdout) == EOF)
                {
                    perror("printf");
                    return PTHREAD_CANCELED;
                }
            }

            /* The worker that claims the final increment is the winner. */
            if (progress == target)
            {
                atomic_store_explicit(&winner, id, memory_order_relaxed);
                return NULL;
            }
        }
    }

    return NULL;
}

int main(void)
{
    pthread_t threads[2];
    /* Keep a distinct ID for each worker; the values stay valid until both joins. */
    int thread_ids[2] = {1, 2};

    /* Start both threads; they compete using the same global counter. */
    for (int i = 0; i < 2; ++i)
    {
        int error = pthread_create(&threads[i], NULL, increment_until_target, &thread_ids[i]);
        if (error != 0)
        {
            fprintf(stderr, "pthread_create: %s\n", strerror(error));
            for (int j = 0; j < i; ++j)
            {
                error = pthread_join(threads[j], NULL);
                if (error != 0)
                    fprintf(stderr, "pthread_join: %s\n", strerror(error));
            }
            return EXIT_FAILURE;
        }
    }

    /*
     * Joining waits for each worker to finish and lets main safely report the
     * result after both workers have stopped.
     */
    for (int i = 0; i < 2; ++i)
    {
        void *thread_result;
        int error = pthread_join(threads[i], &thread_result);
        if (error != 0)
        {
            fprintf(stderr, "pthread_join: %s\n", strerror(error));
            return EXIT_FAILURE;
        }
        if (thread_result == PTHREAD_CANCELED)
            return EXIT_FAILURE;
    }

    {
        int winning_thread = atomic_load_explicit(&winner, memory_order_relaxed);
        if (winning_thread == 0)
        {
            fprintf(stderr, "no thread reached target\n");
            return EXIT_FAILURE;
        }
        if (printf("Thread %d wins\n", winning_thread) < 0 ||
            fflush(stdout) == EOF)
        {
            perror("printf");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
