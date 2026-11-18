#define _POSIX_C_SOURCE 200809L

/*
 * Usage:
 *   cc src/signalcatcher.c -o signalcatcher
 *   ./signalcatcher       # Press Ctrl+C for SIGINT or Ctrl+\ for SIGQUIT.
 *
 * To test SIGUSR1 or SIGUSR2, start it in the background instead:
 *   ./signalcatcher &
 *   pid=$!
 *   kill -USR1 "$pid"  # or: kill -USR2 "$pid"
 *   kill -TERM "$pid"  # stop the program
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void sig_usr(int);

int main(void)
{
    /* Install the same handler for both user-defined signals. */
    if (signal(SIGUSR1, sig_usr) == SIG_ERR)
    {
        perror("signal SIGUSR1");
        return EXIT_FAILURE;
    }

    if (signal(SIGUSR2, sig_usr) == SIG_ERR)
    {
        perror("signal SIGUSR2");
        return EXIT_FAILURE;
    }

    /* Remain available to handle signals until the process is stopped. */
    for (;;)
        pause();
}

static void sig_usr(int signo)
{
    const char *message;
    size_t remaining;

    if (signo == SIGUSR1)
        message = "received SIGUSR1\n";
    else if (signo == SIGUSR2)
        message = "received SIGUSR2\n";
    else if (signo == SIGINT)
        message = "received SIGINT\n";
    else if (signo == SIGQUIT)
        message = "received SIGQUIT\n";
    else
        message = "received unexpected signal\n";

    /* Use write rather than stdio because this runs in a signal handler. */
    remaining = 0;
    while (message[remaining] != '\0')
        ++remaining;

    while (remaining > 0)
    {
        ssize_t written = write(STDOUT_FILENO, message, remaining);

        if (written > 0)
        {
            message += written;
            remaining -= (size_t)written;
        }
        else if (written < 0 && errno == EINTR)
            continue;
        else
            break;
    }
}
