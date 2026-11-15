#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
    int c;

    /* Copy standard input to standard output one character at a time. */
    while ((c = getc(stdin)) != EOF) {
        if (putc(c, stdout) == EOF) {
            perror("output error");
            return EXIT_FAILURE;
        }
    }

    /* Check whether the input loop ended because of a read error. */
    if (ferror(stdin)) {
        perror("input error");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
