dalloc
======
A simple, thread safe, drop-in memory allocation debugging lib in C89

Usage
-----
dalloc.c and dalloc.h should be dropped into an existing project and
compiled with the `-DDALLOC` flag to define the macro that enables dalloc.
dalloc will replace free(), malloc(), calloc(), realloc(), reallocarray(),
strdup() and strndup() by a more secure version that will check for buffer
overflow and memory leak.

dalloc will also output a recap at the end of the program if the compiler
supports the destructor attribute.

vasprintf() and asprintf() are also supported when compiling for C99+.

`_DEFAULT_SOURCE` or other feature test macros may be needed to use these
functions outside of dalloc.

Functions
---------
All functions do nothing when DALLOC is not defined.

#### dalloc_check_overflow(void)
Output all memory overflow to stderr and return the sum of all overflow.

#### dalloc_check_free(void)
Output all allocation that were not freed to stderr.

#### dalloc_check_all(void)
Run both dalloc_check_free() and dalloc_check_overflow() on program's exit.

#### dalloc_ignore(void *p)
Ignore the pointer in argument for memory leak check. This can be useful when
developing an application that never stop.

#### dalloc_comment(void *p, char *comment)
Add a comment to a pointer so it is more easy to know what the pointer stands
for just by looking at the error message from dalloc.

#### dalloc_query(void *p)
Output informations about p to stderr.

Notes
-----
An error with "Unknown pointer" is may caused by a use after free or when using
free or realloc on a pointer allocated with a function not supported by
dalloc.
