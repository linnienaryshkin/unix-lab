#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

static char type_char(mode_t mode)
{
    if (S_ISREG(mode))
        return '-';
    if (S_ISDIR(mode))
        return 'd';
    if (S_ISLNK(mode))
        return 'l';
    if (S_ISCHR(mode))
        return 'c';
    if (S_ISBLK(mode))
        return 'b';
    if (S_ISFIFO(mode))
        return 'p';
    if (S_ISSOCK(mode))
        return 's';
    return '?';
}

static void mode_str(mode_t mode, char out[11])
{
    const char *rwx = "rwxrwxrwx";

    out[0] = type_char(mode);
    for (int i = 0; i < 9; i++)
        out[i + 1] = (mode & (0400 >> i)) ? rwx[i] : '-';
    if (mode & S_ISUID)
        out[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID)
        out[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX)
        out[9] = (mode & S_IXOTH) ? 't' : 'T';
    out[10] = '\0';
}

int main(int argc, char **argv)
{
    /* Require one or more paths to inspect. */
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s file ...\n", argv[0]);
        return EXIT_FAILURE;
    }

    int status = EXIT_SUCCESS;
    for (int i = 1; i < argc; i++)
    {
        struct stat st;
        char mode[11];
        char timestamp[64];

        /* lstat reports symlink metadata without following the link. */
        if (lstat(argv[i], &st) == -1)
        {
            perror(argv[i]);
            status = EXIT_FAILURE;
            continue;
        }

        /* Format the modification time for the listing. */
        struct tm *modified = localtime(&st.st_mtime);
        if (modified == NULL || strftime(timestamp, sizeof(timestamp),
                                         "%Y-%m-%d %H:%M:%S", modified) == 0)
        {
            fprintf(stderr, "%s: cannot format modification time\n", argv[i]);
            status = EXIT_FAILURE;
            continue;
        }

        /* Print inode, permissions, link count, size, blocks, and timestamp. */
        mode_str(st.st_mode, mode);
        if (printf("%8llu %s %3lu %9lld blks=%-6lld %s  %s\n",
                   (unsigned long long)st.st_ino, mode,
                   (unsigned long)st.st_nlink, (long long)st.st_size,
                   (long long)st.st_blocks, timestamp, argv[i]) < 0)
        {
            perror("printf");
            return EXIT_FAILURE;
        }
    }

    return status;
}
