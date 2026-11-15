#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    DIR *dp;
    struct dirent *dirp;

    /* Require the directory path to be provided as the only argument. */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s directory_name\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Open the requested directory for reading. */
    dp = opendir(argv[1]);
    if (dp == NULL)
    {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    /* Print each entry; clear errno to detect errors from readdir(). */
    errno = 0;
    while ((dirp = readdir(dp)) != NULL)
    {
        /* Report output failure and close the directory before exiting. */
        if (printf("%s\n", dirp->d_name) < 0)
        {
            perror("printf");
            closedir(dp);
            return EXIT_FAILURE;
        }
    }

    /* A null result means end-of-directory unless readdir set errno. */
    if (errno != 0)
    {
        perror("readdir");
        closedir(dp);
        return EXIT_FAILURE;
    }

    /* Close the directory stream after successfully listing its entries. */
    if (closedir(dp) == -1)
    {
        perror("closedir");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}