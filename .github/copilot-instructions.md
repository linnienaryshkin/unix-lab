# Repository instructions

## Project shape

This repository is a set of small, standalone C exercises about UNIX processes, system calls, and standard library interfaces, inspired by _Advanced Programming in the UNIX Environment_. Each file in `src/` is its own executable with its own `main`; there is no shared application or library layer.

- `hello.c` demonstrates process identity with `getpid()`.
- `mycat.c` copies standard input to standard output with the `read()` and `write()` system calls.
- `mycat_stdio.c` performs the same copy using the buffered stdio functions `getc()` and `putc()`.
- `myls.c` lists a directory using `opendir()` and `readdir()`.
- `mysh.c` runs a command name in a child process using `fork()`, `execlp()`, and `waitpid()`.

When changing an exercise, preserve its focus on the UNIX or C interface it demonstrates rather than folding it into another executable.

## Implementation conventions

- Update these instructions whenever an implementation introduces a new project pattern or convention that should guide future changes.
- Keep each program self-contained and use the standard/POSIX interfaces relevant to that exercise.
- Check I/O and directory-operation failures, report them with `perror()` or a usage message on standard error, and return `EXIT_FAILURE`; successful execution returns `EXIT_SUCCESS`.
- For raw system calls, handle interrupted operations (`EINTR`) and short writes explicitly. The stdio example instead checks `putc()` and `ferror()`; retain that distinction when comparing the two approaches.
- When using `readdir()`, clear `errno` before iteration so an end-of-directory result can be distinguished from an iteration error.
- Add concise comments that explain the purpose of each logical code block, especially blocks handling system calls, errors, and resource cleanup. Avoid comments that merely repeat individual statements.
