#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    DIR *dp;
    struct dirent *dirp;

    if (argc != 2) {
        fprintf(stderr, "usage: %s directory_name\n", argv[0]);
        return EXIT_FAILURE;
    }

    dp = opendir(argv[1]);
    if (dp == NULL) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    errno = 0;
    while ((dirp = readdir(dp)) != NULL) {
        if (printf("%s\n", dirp->d_name) < 0) {
            perror("printf");
            closedir(dp);
            return EXIT_FAILURE;
        }
    }

    if (errno != 0) {
        perror("readdir");
        closedir(dp);
        return EXIT_FAILURE;
    }

    if (closedir(dp) == -1) {
        perror("closedir");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}