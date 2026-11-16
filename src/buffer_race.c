/* Mix stdio and raw write(); the output order reveals stdout's buffer mode. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    printf("S1 "); /* no newline: stays in buffer if line-buffered */
    write(1, "W1 ", 3);
    printf("S2\n"); /* newline: flushes only if line-buffered */
    write(1, "W2\n", 3);
    fprintf(stderr, "E(unbuffered)\n");
    return 0;

    // printf("before fork\n"); /* newline, but fully buffered if stdout is a file */
    // pid_t p = fork();
    // if (p == 0)
    // {
    //     printf("child\n");
    //     exit(0);
    // }
    // waitpid(p, NULL, 0);
    // printf("parent\n");
    // return 0;
}
