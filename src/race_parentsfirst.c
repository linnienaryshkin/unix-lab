#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static int charatatime(const char *str);

int main(void)
{
    /* The pipe carries a one-byte release signal from parent to child. */
    int sync_pipe[2];
    pid_t pid;
    int status = EXIT_SUCCESS;
    int child_status;
    char token = 'x';

    /* Keep each character visible immediately. */
    if (setvbuf(stdout, NULL, _IONBF, 0) != 0)
    {
        fprintf(stderr, "could not disable stdout buffering\n");
        return EXIT_FAILURE;
    }

    /* The child waits on this pipe until the parent has printed first. */
    if (pipe(sync_pipe) < 0)
    {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid = fork();
    if (pid < 0)
    {
        perror("fork");
        /* No child was created, so the parent must release both pipe ends. */
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        return EXIT_FAILURE;
    }

    if (pid == 0)
    {
        ssize_t result;

        /* The child only reads; EOF or one byte determines whether it may proceed. */
        close(sync_pipe[1]);
        do
        {
            result = read(sync_pipe[0], &token, sizeof(token));
        } while (result < 0 && errno == EINTR);

        if (result < 0)
        {
            perror("read");
            close(sync_pipe[0]);
            _exit(EXIT_FAILURE);
        }
        if (result == 0)
        {
            fprintf(stderr, "parent closed synchronization pipe without signaling\n");
            close(sync_pipe[0]);
            _exit(EXIT_FAILURE);
        }
        if (close(sync_pipe[0]) < 0)
        {
            perror("close");
            _exit(EXIT_FAILURE);
        }

        /* The successful read means the parent has finished printing. */
        if (charatatime("output from child\n") < 0)
            _exit(EXIT_FAILURE);
        _exit(EXIT_SUCCESS);
    }

    /* Print before releasing the child, then close the unused pipe endpoint. */
    if (close(sync_pipe[0]) < 0)
    {
        perror("close");
        status = EXIT_FAILURE;
    }
    if (charatatime("output from parent\n") < 0)
        status = EXIT_FAILURE;

    /* Signal the child even if parent output failed, so it cannot remain blocked. */
    /* Retry an interrupted write; the pipe is large enough for this single byte. */
    {
        ssize_t result;
        do
        {
            result = write(sync_pipe[1], &token, sizeof(token));
        } while (result < 0 && errno == EINTR);

        if (result != (ssize_t)sizeof(token))
        {
            if (result < 0)
                perror("write");
            else
                fprintf(stderr, "short write to synchronization pipe\n");
            status = EXIT_FAILURE;
        }
    }

    /* Closing the write end also ensures the child cannot wait forever on errors. */
    if (close(sync_pipe[1]) < 0)
    {
        perror("close");
        status = EXIT_FAILURE;
    }

    /* Reap the child and report its failure to the invoking process. */
    while (waitpid(pid, &child_status, 0) < 0)
    {
        if (errno == EINTR)
            continue;
        perror("waitpid");
        return EXIT_FAILURE;
    }

    if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != EXIT_SUCCESS)
        return EXIT_FAILURE;
    return status;
}

static int charatatime(const char *str)
{
    /* Emit one character at a time to make process output interleaving observable. */
    for (const char *ptr = str; *ptr != '\0'; ++ptr)
    {
        if (putc((unsigned char)*ptr, stdout) == EOF)
        {
            perror("putc");
            return -1;
        }
    }
    return ferror(stdout) ? -1 : 0;
}
