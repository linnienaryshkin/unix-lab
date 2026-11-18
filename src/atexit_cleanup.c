/*
 * Usage:
 *   cc src/atexit_demo.c -o atexit_demo
 *   ./atexit_demo
 */

#include <stdio.h>
#include <stdlib.h>

static char *buffer;

static void cleanup_buffer(void)
{
    free(buffer);
    buffer = NULL;
    puts("freed allocated memory");
}

int main(void)
{
    buffer = malloc(1024);
    if (buffer == NULL)
    {
        perror("malloc");
        return EXIT_FAILURE;
    }

    /* Register the cleanup before using the allocated memory. */
    if (atexit(cleanup_buffer) != 0)
    {
        fprintf(stderr, "could not register memory cleanup\n");
        free(buffer);
        return EXIT_FAILURE;
    }

    /* Use the allocated memory as an example application resource. */
    snprintf(buffer, 1024, "application data");
    printf("using: %s\n", buffer);

    return EXIT_SUCCESS;
}
