dalloc
======
A simple, thread safe, drop-in memory allocation debugging lib for C

Usage
-----
dalloc.c and dalloc.h should be dropped into an existing project and
compiled with the `-DDALLOC` flag to define the macro that enables dalloc.
dalloc will replace free(), malloc(), calloc(), realloc(), reallocarray(),
strdup(), strndup(), vasprintf() and asprintf() by a more secure version that
will check for buffer overflow and memory leak. It will also output a recap at
the end of the program.

By defining `EXITSEGV` all exit() call will be replaced by a segmentation fault
which can be very useful to check where an overflow occur with a real debugger.

strdup, strndup, reallocarray, vasprintf and asprintf are not standard so you
will probably need to define `_DEFAULT_SOURCE` or equivalent to use them
outside of dalloc.

Functions
---------
#### dalloc_check_overflow(void)
Output all memory overflow to stderr and return the sum of all overflow.

#### dalloc_check_free(void)
Output all allocation that were not freed to stderr.

#### dalloc_check_all(void)
Run both dalloc_check_free() and dalloc_check_overflow() on program exit.

#### dalloc_sighandler(int sig)
Output signal meaning and exit. To be used with signal() from signal.h.
e.g.: `signal(SIG, dalloc_sighandler);` Require `_DEFAULT_SOURCE` or equivalent.

#### dalloc_ignore(void *p)
Ignore the pointer in argument for memory leak check. This can be useful when
developing an application that never stop.
When `DALLOC` is not defined this function does nothing.

#### dalloc_comment(void *p, char *comment)
Add a comment to a pointer so it is more easy to know what the pointer stands
for just by looking at the error message from dalloc.
When `DALLOC` is not defined this function does nothing.

Notes
-----
An error with "Unknown pointer" can be caused by a double free or when freeing
a pointer allocated with a function not supported by dalloc.
