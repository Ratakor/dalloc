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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef DALLOC
#define free(p)                (_dalloc_free(p, __FILE__, __LINE__))
#define malloc(siz)            (_dalloc_malloc(siz, __FILE__, __LINE__))
#define calloc(nmemb, siz)     (_dalloc_calloc(nmemb, siz, __FILE__, __LINE__))
#define realloc(p, siz)        (_dalloc_realloc(p, siz, __FILE__, __LINE__))
#define reallocarray(p, n, s)  (_dalloc_reallocarray(p, n, s, __FILE__, __LINE__))
#define strdup(s)              (_dalloc_strdup(s, __FILE__, __LINE__))
#define strndup(s, n)          (_dallloc_strndup(s, n, __FILE__, __LINE__))

#define dalloc_ignore(p)       (_dalloc_ignore(p, __FILE__, __LINE__))
#define dalloc_comment(p, com) (_dalloc_comment(p, com, __FILE__, __LINE__))
#else
#define dalloc_ignore(p)
#define dalloc_comment(p, comment)
#endif /* DALLOC */

#ifdef EXITSEGV
#define exit(dummy)            (exitsegv(dummy))
#endif /* EXITSEGV */

size_t dalloc_check_overflow(void);
void dalloc_check_free(void);
void dalloc_check_all(void);
void dalloc_sighandler(int sig);

void _dalloc_ignore(void *p, char *file, int line);
void _dalloc_comment(void *p, const char *comment, char *file, int line);
void _dalloc_free(void *p, char *file, int line);
void *_dalloc_malloc(size_t siz, char *file, int line);
void *_dalloc_calloc(size_t nmemb, size_t siz, char *file, int line);
void *_dalloc_realloc(void *p, size_t siz, char *file, int line);
void *_dalloc_reallocarray(void *p, size_t n, size_t s, char *file, int line);
char *_dalloc_strdup(const char *s, char *file, int line);
char *_dalloc_strndup(const char *s, size_t n, char *file, int line);

void exitsegv(int dummy);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DALLOC_H */
