#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFFSIZE 4096

int main(void)
{
    char buf[BUFFSIZE];
    ssize_t n;

    /* Read standard input until end-of-file. */
    for (;;)
    {
        n = read(STDIN_FILENO, buf, sizeof(buf));

        /* Retry a read interrupted by a signal. */
        if (n < 0 && errno == EINTR)
            continue;

        /* Stop if reading standard input failed. */
        if (n < 0)
        {
            perror("read error");
            return EXIT_FAILURE;
        }

        /* A zero-byte read means there is no more input. */
        if (n == 0)
            break;

        ssize_t written = 0;

        /* Write the whole chunk, accounting for partial writes. */
        while (written < n)
        {
            ssize_t result = write(STDOUT_FILENO, buf + written,
                                   (size_t)(n - written));

            /* Retry a write interrupted by a signal. */
            if (result < 0 && errno == EINTR)
                continue;

            /* Stop if writing standard output failed. */
            if (result < 0)
            {
                perror("write error");
                return EXIT_FAILURE;
            }

            /* Treat a zero-byte write as an I/O error to avoid a stalled loop. */
            if (result == 0)
            {
                errno = EIO;
                perror("write error");
                return EXIT_FAILURE;
            }

            written += result;
        }
    }

    return EXIT_SUCCESS;
}
