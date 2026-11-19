#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * Two child processes race to send TARGET requests. The parent is the referee:
 * it receives requests over one socket per child, advances the shared counter,
 * and replies CONTINUE or WIN. This keeps the counter race-free while showing
 * how processes can exchange messages through Unix-domain sockets.
 */
enum
{
    WORKER_COUNT = 2,
    TARGET = 1000
};

/* Workers send REQUEST; the parent answers CONTINUE, WIN, or STOP. */
enum message_type
{
    REQUEST = 1,
    CONTINUE,
    WIN,
    STOP
};

struct message
{
    uint32_t type;  /* One of message_type. */
    uint32_t value; /* Worker ID in a request; counter value in a reply. */
};

static int send_message(int socket_fd, const struct message *message)
{
    ssize_t result;

    /* Datagram sockets preserve message boundaries; retry if interrupted. */
    do
    {
        result = send(socket_fd, message, sizeof(*message), 0);
    } while (result < 0 && errno == EINTR);

    if (result < 0)
    {
        perror("send");
        return -1;
    }
    if ((size_t)result != sizeof(*message))
    {
        fprintf(stderr, "short socket message sent\n");
        return -1;
    }
    return 0;
}

static int receive_message(int socket_fd, struct message *message)
{
    ssize_t result;

    /* Each receive must produce exactly one complete protocol message. */
    do
    {
        result = recv(socket_fd, message, sizeof(*message), 0);
    } while (result < 0 && errno == EINTR);

    if (result < 0)
    {
        perror("recv");
        return -1;
    }
    if ((size_t)result != sizeof(*message))
    {
        fprintf(stderr, "unexpected socket message size\n");
        return -1;
    }
    return 0;
}

static int run_worker(int socket_fd, uint32_t id)
{
    struct message request = {REQUEST, id};

    /*
     * A worker can only make progress after the parent acknowledges its
     * request. The first worker to receive WIN is the race winner.
     */
    for (;;)
    {
        struct message reply;

        if (send_message(socket_fd, &request) < 0 ||
            receive_message(socket_fd, &reply) < 0)
            return EXIT_FAILURE;

        if (reply.type == WIN || reply.type == STOP)
            return EXIT_SUCCESS;
        if (reply.type != CONTINUE)
        {
            fprintf(stderr, "worker %u received an invalid reply\n", id);
            return EXIT_FAILURE;
        }
    }
}

static int close_sockets(int sockets[WORKER_COUNT][2])
{
    int status = EXIT_SUCCESS;

    /* Socket descriptors are set to -1 after close to make cleanup repeatable. */
    for (int i = 0; i < WORKER_COUNT; ++i)
    {
        for (int end = 0; end < 2; ++end)
        {
            if (sockets[i][end] >= 0 && close(sockets[i][end]) < 0)
            {
                perror("close");
                status = EXIT_FAILURE;
            }
            sockets[i][end] = -1;
        }
    }
    return status;
}

static int wait_for_workers(const pid_t workers[WORKER_COUNT], int count)
{
    int status = EXIT_SUCCESS;

    /* Reap every child, even if an earlier child failed. */
    for (int i = 0; i < count; ++i)
    {
        int child_status;
        pid_t result;

        do
        {
            result = waitpid(workers[i], &child_status, 0);
        } while (result < 0 && errno == EINTR);

        if (result < 0)
        {
            perror("waitpid");
            status = EXIT_FAILURE;
        }
        else if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != EXIT_SUCCESS)
        {
            fprintf(stderr, "worker %d failed\n", i + 1);
            status = EXIT_FAILURE;
        }
    }
    return status;
}

int main(void)
{
    int sockets[WORKER_COUNT][2] = {{-1, -1}, {-1, -1}};
    pid_t workers[WORKER_COUNT];
    int worker_count = 0;
    int status = EXIT_SUCCESS;
    uint32_t counter = 0;
    uint32_t progress_interval = TARGET / 100;

    /* Keep progress readable even if TARGET is configured below 100. */
    if (progress_interval == 0)
        progress_interval = 1;

    /*
     * SOCK_DGRAM makes each request/reply a distinct message. Each socketpair
     * is local to this process tree and connects one worker directly to parent.
     */
    for (int i = 0; i < WORKER_COUNT; ++i)
    {
        if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets[i]) < 0)
        {
            perror("socketpair");
            close_sockets(sockets);
            return EXIT_FAILURE;
        }
    }

    /*
     * Fork the contenders. Closing unrelated endpoints is important: it keeps
     * each process's ownership clear and makes socket closure meaningful.
     */
    for (int i = 0; i < WORKER_COUNT; ++i)
    {
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
            status = EXIT_FAILURE;
            break;
        }
        if (pid == 0)
        {
            for (int j = 0; j < WORKER_COUNT; ++j)
            {
                close(sockets[j][0]);
                if (j != i)
                    close(sockets[j][1]);
            }
            {
                int worker_status = run_worker(sockets[i][1], (uint32_t)i + 1);
                close(sockets[i][1]);
                _exit(worker_status);
            }
        }
        workers[worker_count++] = pid;
    }

    if (worker_count != WORKER_COUNT)
    {
        /* Release any child already started if a later fork failed. */
        for (int i = 0; i < worker_count; ++i)
        {
            const struct message stop = {STOP, 0};
            if (send_message(sockets[i][0], &stop) < 0)
                status = EXIT_FAILURE;
        }
        close_sockets(sockets);
        if (wait_for_workers(workers, worker_count) != EXIT_SUCCESS)
            status = EXIT_FAILURE;
        return status;
    }

    /* The parent keeps only the referee ends of the socket pairs. */
    for (int i = 0; i < WORKER_COUNT; ++i)
    {
        if (close(sockets[i][1]) < 0)
        {
            perror("close");
            status = EXIT_FAILURE;
        }
        sockets[i][1] = -1;
    }

    /*
     * Wait for whichever worker requests service first. The parent serializes
     * increments, so exactly one request can claim the final counter value.
     */
    while (status == EXIT_SUCCESS && counter < TARGET)
    {
        struct pollfd ready[WORKER_COUNT];
        int result;

        /* poll avoids favoring a worker by blocking on its socket alone. */
        for (int i = 0; i < WORKER_COUNT; ++i)
        {
            ready[i].fd = sockets[i][0];
            ready[i].events = POLLIN;
            ready[i].revents = 0;
        }

        do
        {
            result = poll(ready, WORKER_COUNT, -1);
        } while (result < 0 && errno == EINTR);

        if (result < 0)
        {
            perror("poll");
            status = EXIT_FAILURE;
            break;
        }

        for (int i = 0; i < WORKER_COUNT && counter < TARGET; ++i)
        {
            struct message request;
            struct message reply;

            if ((ready[i].revents & POLLIN) == 0)
            {
                if (ready[i].revents != 0)
                {
                    fprintf(stderr, "worker socket reported an error\n");
                    status = EXIT_FAILURE;
                    break;
                }
                continue;
            }

            if (receive_message(sockets[i][0], &request) < 0)
            {
                status = EXIT_FAILURE;
                break;
            }
            if (request.type != REQUEST || request.value != (uint32_t)i + 1)
            {
                fprintf(stderr, "invalid request from worker %d\n", i + 1);
                status = EXIT_FAILURE;
                break;
            }

            /* The worker whose request claims TARGET receives the win message. */
            ++counter;
            reply.type = counter == TARGET ? WIN : CONTINUE;
            reply.value = counter;
            if (send_message(sockets[i][0], &reply) < 0)
            {
                status = EXIT_FAILURE;
                break;
            }

            /* Report progress about once per percent, as in race_threads.c. */
            if (counter % progress_interval == 0)
            {
                if (printf("Worker %d progress: %u/%u\n", i + 1, counter, TARGET) < 0 ||
                    fflush(stdout) == EOF)
                {
                    perror("printf");
                    status = EXIT_FAILURE;
                    break;
                }
            }

            if (counter == TARGET)
            {
                const struct message stop = {STOP, counter};
                int other = 1 - i;

                /* Tell the losing worker to exit instead of waiting for a turn. */
                if (send_message(sockets[other][0], &stop) < 0)
                    status = EXIT_FAILURE;
                else if (printf("Worker %d wins at %u requests\n", i + 1, counter) < 0)
                {
                    perror("printf");
                    status = EXIT_FAILURE;
                }
            }
        }
    }

    if (status != EXIT_SUCCESS)
    {
        /* A stop message lets workers exit if the referee encounters an error. */
        const struct message stop = {STOP, counter};
        for (int i = 0; i < WORKER_COUNT; ++i)
        {
            if (sockets[i][0] >= 0 && send_message(sockets[i][0], &stop) < 0)
                status = EXIT_FAILURE;
        }
    }

    /* Reap children before final socket cleanup. */
    if (wait_for_workers(workers, worker_count) != EXIT_SUCCESS)
        status = EXIT_FAILURE;
    if (close_sockets(sockets) != EXIT_SUCCESS)
        status = EXIT_FAILURE;

    return status;
}
