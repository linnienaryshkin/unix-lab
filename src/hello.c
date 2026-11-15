#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    /* Print a greeting with the process ID of this program. */
    printf("hello world from process ID %ld\n", (long)getpid());
    return EXIT_SUCCESS;
}
