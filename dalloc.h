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
#define free(p)                 (___free(p, __FILE__, __LINE__))
#define malloc(siz)             (___malloc(siz, __FILE__, __LINE__))
#define calloc(nmemb, siz)      (___calloc(nmemb, siz, __FILE__, __LINE__))
#define realloc(p, siz)         (___realloc(p, siz, __FILE__, __LINE__))
#define reallocarray(p, n, s)   (___reallocarray(p, n, s, __FILE__, __LINE__))
#define strdup(s)               (___strdup(s, __FILE__, __LINE__))
#define strndup(s, n)           (___strndup(s, n, __FILE__, __LINE__))
#endif /* DALLOC */

#ifdef EXITSEGV
#define exit(dummy)             (exitsegv(dummy))
#endif /* EXITSEGV */

#define dalloc_ignore(p)        (___dalloc_ignore(p, __FILE__, __LINE__))
#define dalloc_comment(p, com)  (___dalloc_comment(p, com, __FILE__, __LINE__))

size_t dalloc_check_overflow(void);
void dalloc_check_free(void);
void dalloc_check_all(void);
void dalloc_sighandler(int sig);

void ___dalloc_ignore(void *p, char *file, int line);
void ___dalloc_comment(void *p, const char *comment, char *file, int line);
void ___free(void *p, char *file, int line);
void *___malloc(size_t siz, char *file, int line);
void *___calloc(size_t nmemb, size_t siz, char *file, int line);
void *___realloc(void *p, size_t siz, char *file, int line);
void *___reallocarray(void *p, size_t nmemb, size_t siz, char *file, int line);
char *___strdup(const char *s, char *file, int line);
char *___strndup(const char *s, size_t n, char *file, int line);

void exitsegv(int dummy);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DALLOC_H */
