#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

static int write_all(int fd, const char *text)
{
    size_t remaining = strlen(text);

    while (remaining > 0)
    {
        ssize_t written = write(fd, text, remaining);

        if (written > 0)
        {
            text += written;
            remaining -= (size_t)written;
        }
        else if (written < 0 && errno == EINTR)
            continue;
        else
        {
            if (written == 0)
                errno = EIO;
            syslog(LOG_ERR, "write failed: %s", strerror(errno));
            return -1;
        }
    }

    return 0;
}

static int current_time(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm local_time;

    if (now == (time_t)-1 || localtime_r(&now, &local_time) == NULL)
        return -1;
    if (strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &local_time) == 0)
    {
        errno = EOVERFLOW;
        return -1;
    }

    return 0;
}

static void run_clock(int log_fd)
{
    char timestamp[32];
    char entry[128];
    pid_t daemon_pid = getpid();

    openlog("time_backgroud", LOG_PID, LOG_DAEMON);

    if (current_time(timestamp, sizeof(timestamp)) < 0)
    {
        syslog(LOG_ERR, "could not get start time: %s", strerror(errno));
        _exit(EXIT_FAILURE);
    }
    snprintf(entry, sizeof(entry), "# daemon started pid=%ld at %s; duration=60 seconds\n",
             (long)daemon_pid, timestamp);
    if (write_all(log_fd, entry) < 0)
        _exit(EXIT_FAILURE);

    for (int second = 0; second < 60; ++second)
    {
        if (current_time(timestamp, sizeof(timestamp)) < 0)
        {
            syslog(LOG_ERR, "could not get clock time: %s", strerror(errno));
            _exit(EXIT_FAILURE);
        }
        snprintf(entry, sizeof(entry), "%s\n", timestamp);
        if (write_all(log_fd, entry) < 0)
            _exit(EXIT_FAILURE);

        struct timespec remaining = {1, 0};
        while (nanosleep(&remaining, &remaining) < 0)
        {
            if (errno == EINTR)
                continue;

            syslog(LOG_ERR, "nanosleep failed: %s", strerror(errno));
            _exit(EXIT_FAILURE);
        }
    }

    if (current_time(timestamp, sizeof(timestamp)) < 0)
    {
        syslog(LOG_ERR, "could not get stop time: %s", strerror(errno));
        _exit(EXIT_FAILURE);
    }
    snprintf(entry, sizeof(entry), "# daemon stopped pid=%ld at %s\n",
             (long)daemon_pid, timestamp);
    if (write_all(log_fd, entry) < 0)
        _exit(EXIT_FAILURE);
    if (close(log_fd) < 0)
    {
        syslog(LOG_ERR, "close failed: %s", strerror(errno));
        _exit(EXIT_FAILURE);
    }
    _exit(EXIT_SUCCESS);
}

int main(void)
{
    const char *log_path = "time_backgroud.log";
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (log_fd < 0)
    {
        perror(log_path);
        return EXIT_FAILURE;
    }
    if (log_fd <= STDERR_FILENO)
    {
        int moved_fd = fcntl(log_fd, F_DUPFD, STDERR_FILENO + 1);
        if (moved_fd < 0)
        {
            perror("fcntl");
            close(log_fd);
            return EXIT_FAILURE;
        }
        close(log_fd);
        log_fd = moved_fd;
    }

    /* The first parent returns; the child starts a new session. */
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        close(log_fd);
        return EXIT_FAILURE;
    }
    if (pid > 0)
    {
        close(log_fd);
        return EXIT_SUCCESS;
    }

    if (setsid() < 0)
    {
        dprintf(log_fd, "# setsid failed: %s\n", strerror(errno));
        _exit(EXIT_FAILURE);
    }

    /* The second fork prevents the daemon from acquiring a controlling terminal. */
    pid = fork();
    if (pid < 0)
    {
        dprintf(log_fd, "# fork failed: %s\n", strerror(errno));
        _exit(EXIT_FAILURE);
    }
    if (pid > 0)
        _exit(EXIT_SUCCESS);

    umask(0);
    if (chdir("/") < 0)
    {
        dprintf(log_fd, "# chdir failed: %s\n", strerror(errno));
        _exit(EXIT_FAILURE);
    }

    int null_fd = open("/dev/null", O_RDWR);
    if (null_fd < 0)
    {
        dprintf(log_fd, "# open /dev/null failed: %s\n", strerror(errno));
        _exit(EXIT_FAILURE);
    }
    for (int fd = STDIN_FILENO; fd <= STDERR_FILENO; ++fd)
    {
        if (dup2(null_fd, fd) < 0)
        {
            dprintf(log_fd, "# dup2 failed: %s\n", strerror(errno));
            _exit(EXIT_FAILURE);
        }
    }
    if (null_fd > STDERR_FILENO)
        close(null_fd);

    run_clock(log_fd);
}
