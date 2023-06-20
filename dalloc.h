/*
 * Copyright © 2023, Ratakor <ratakor@disroot.org>
 *
 * This library is free software. You can redistribute it and/or modify it
 * under the terms of the ISC license. See dalloc.c for details.
 */

#ifndef DALLOC_H
#define DALLOC_H

#include <stdlib.h>
#include <string.h>

#ifdef DALLOC
#define free(p)                 (dfree(p, __FILE__, __LINE__))
#define malloc(siz)             (dmalloc(siz, __FILE__, __LINE__))
#define calloc(nmemb, siz)      (dcalloc(nmemb, siz, __FILE__, __LINE__))
#define realloc(p, siz)         (drealloc(p, siz, __FILE__, __LINE__))
#define reallocarray(p, n, siz) (dreallocarray(p, n, siz, __FILE__, __LINE__))
#define strdup(s)               (dstrdup(s, __FILE__, __LINE__))
#define strndup(s, n)           (dstrndup(s, n, __FILE__, __LINE__))
#endif /* DALLOC */

#ifdef EXITSEGV
#define exit(dummy)             (exitsegv(dummy))
#endif /* EXITSEGV */

size_t dalloc_check_overflow(void);
void dalloc_check_free(void);
void dalloc(void);

void dfree(void *p, char *file, int line);
void *dmalloc(size_t siz, char *file, int line);
void *dcalloc(size_t nmemb, size_t siz, char *file, int line);
void *drealloc(void *p, size_t siz, char *file, int line);
void *dreallocarray(void *p, size_t nmemb, size_t siz, char *file, int line);
char *dstrdup(const char *s, char *file, int line);
char *dstrndup(const char *s, size_t n, char *file, int line);
void exitsegv(int dummy);

#endif /* DEBUGMEM_H */
