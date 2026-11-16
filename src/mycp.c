#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFFSIZE 5

int main(int argc, char **argv)
{
    /* Expect the source and destination file paths. */
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s in out\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Reuse a fixed-size buffer for each read from the input file. */
    char buf[BUFFSIZE];

    /* Open the source for reading and create/truncate the destination. */
    int in = open(argv[1], O_RDONLY);
    if (in < 0)
    {
        perror("open input");
        return EXIT_FAILURE;
    }

    int out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0)
    {
        perror("open output");
        close(in);
        return EXIT_FAILURE;
    }

    long reads = 0;
    ssize_t n;
    for (;;)
    {
        /* Read a chunk and retry if a signal interrupted the system call. */
        n = read(in, buf, sizeof(buf));

        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0)
        {
            perror("read");
            close(in);
            close(out);
            return EXIT_FAILURE;
        }
        if (n == 0)
            break;
        reads++;

        /* A write may transfer fewer bytes than requested, so finish the chunk. */
        ssize_t written = 0;
        while (written < n)
        {
            ssize_t w = write(out, buf + written, (size_t)(n - written));
            if (w < 0 && errno == EINTR)
                continue;
            if (w < 0)
            {
                perror("write");
                close(in);
                close(out);
                return EXIT_FAILURE;
            }
            if (w == 0)
            {
                errno = EIO;
                perror("write");
                close(in);
                close(out);
                return EXIT_FAILURE;
            }
            written += w;
        }
    }

    /* Report the number of non-empty reads used to copy the file. */
    printf("reads=%ld bufsize=%d\n", reads, BUFFSIZE);

    close(in);
    close(out);
    return EXIT_SUCCESS;
}
