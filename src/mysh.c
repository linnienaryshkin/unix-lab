#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void sig_int(int); /* our signal-catching function */

int main(void)
{
    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;

    if (signal(SIGINT, sig_int) == SIG_ERR)
    {
        perror("signal");
        return EXIT_FAILURE;
    }

    /* Print the initial prompt. */
    fputs("% ", stdout);
    fflush(stdout);

    /* Read one command name per line until end-of-file. */
    while ((length = getline(&line, &capacity, stdin)) != -1)
    {
        /* Remove the newline so it is not included in the command name. */
        if (length > 0 && line[length - 1] == '\n')
            line[length - 1] = '\0';

        /* Ignore empty lines; otherwise run the command in a child process. */
        if (line[0] != '\0')
        {
            /* Create a child process to run the command. */
            pid_t pid = fork();

            /* Stop if the operating system could not create the child. */
            if (pid < 0)
            {
                perror("fork");
                free(line);
                return EXIT_FAILURE;
            }

            /* In the child, replace this process with the requested command. */
            if (pid == 0)
            {
                execlp(line, line, (char *)NULL);
                /* Reaching this point means execlp failed. */
                perror(line);
                _exit(127);
            }

            /* The parent waits for this command, retrying if interrupted. */
            while (waitpid(pid, NULL, 0) < 0)
            {
                if (errno == EINTR)
                    continue;

                /* Report other wait errors and release the input buffer. */
                perror("waitpid");
                free(line);
                return EXIT_FAILURE;
            }
        }

        /* Prompt again after the command finishes (or an empty line is read). */
        fputs("% ", stdout);
        fflush(stdout);
    }

    /* Distinguish normal end-of-file from an input error. */
    if (ferror(stdin))
    {
        /* Report the read error and release the input buffer. */
        perror("getline");
        free(line);
        return EXIT_FAILURE;
    }

    /* Release the buffer allocated by getline before normal exit. */
    free(line);
    return EXIT_SUCCESS;
}

void sig_int(int signo)
{
    (void)signo;
    printf("interrupt\n%% ");
    exit(EXIT_FAILURE);
}