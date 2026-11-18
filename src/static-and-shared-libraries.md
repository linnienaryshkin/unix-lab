# Static and shared libraries

On Linux, compare the size of `hello.c` when linked statically and with the
compiler's default (usually shared-library) settings:

```sh
cc -static src/hello.c -o hello-static
ls -l hello-static
size hello-static
```

> The static command requires a toolchain and static versions of the required libraries. It does not work with the standard macOS toolchain: macOS does not support fully static executables, and `cc -static` may fail with a missing startup object such as `crt0.o`. On macOS, you can still inspect the default shared-library executable:

```sh
cc src/hello.c -o hello-shared
ls -l hello-shared
size hello-shared
```

The statically linked executable includes the library code it needs, so its
file size and `size` totals are usually much larger. The shared-library
executable instead relies on libraries installed on the system. Exact values
depend on the source, compiler, operating system, and libraries. Remove any
generated executables when you're done:

```sh
rm -f hello-static hello-shared
```
