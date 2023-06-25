dalloc
======
A simple, thread safe, drop-in memory allocation debugging lib for C

Usage
-----
dalloc.c and dalloc.h should be dropped into an existing project and
compiled with the `-DDALLOC` flag to define the macro that enables dalloc.
dalloc will replace free(), malloc(), calloc(), realloc(), reallocarray(),
strdup(), and strndup() by a more secure version that will check for buffer
overflow and memory leak. It will also output a recap at the end of the program.

By defining `EXITSEGV` all exit() call will be replaced by a segmentation fault
which can be very useful to check where an overflow occur with a real debugger.

strdup, strndup and reallocarray are not standard so you'll probably need to
define `_DEFAULT_SOURCE` or equivalent to use them outside of dalloc.

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
