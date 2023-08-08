/*
 * Copyright © 2023, Ratakor <ratakor@disroot.org>
 *
 * This library is free software. You can redistribute it and/or modify it
 * under the terms of the ISC license. See dalloc.c for details.
 */

#ifndef DALLOC_H
#define DALLOC_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(DALLOC) && !defined(DALLOC_INTERNAL)
#define free(p)                (_dalloc_free(p, __FILE__, __LINE__))
#define malloc(siz)            (_dalloc_malloc(siz, __FILE__, __LINE__))
#define calloc(nmemb, siz)     (_dalloc_calloc(nmemb, siz, __FILE__, __LINE__))
#define realloc(p, siz)        (_dalloc_realloc(p, siz, __FILE__, __LINE__))
#define reallocarray(p, n, s)  (_dalloc_reallocarray(p, n, s, __FILE__, __LINE__))
#define strdup(s)              (_dalloc_strdup(s, __FILE__, __LINE__))
#define strndup(s, n)          (_dalloc_strndup(s, n, __FILE__, __LINE__))
#if __STDC_VERSION__ >= 199901L
#define vasprintf(p, fmt, ap)  (_dalloc_vasprintf(p, fmt, ap, __FILE__, __LINE__))
#define asprintf(p, ...)       (_dalloc_asprintf(p, __FILE__, __LINE__, __VA_ARGS__))
#endif /* __STDC_VERSION__ >= 199901L */

#define dalloc_ignore(p)       (_dalloc_ignore(p, __FILE__, __LINE__))
#define dalloc_comment(p, str) (_dalloc_comment(p, str, __FILE__, __LINE__))
#define dalloc_query(p)        (_dalloc_query(p, __FILE__, __LINE__))
#endif /* DALLOC && !DALLOC_INTERNAL */

#if defined(DALLOC) || defined(DALLOC_INTERNAL)
size_t dalloc_check_overflow(void);
void dalloc_check_free(void);
void dalloc_check_all(void) __attribute__((destructor));

void _dalloc_ignore(void *p, char *file, int line);
void _dalloc_comment(void *p, const char *comment, char *file, int line);
void _dalloc_query(void *p, char *file, int line);
void _dalloc_free(void *p, char *file, int line);
void *_dalloc_malloc(size_t siz, char *file, int line);
void *_dalloc_calloc(size_t nmemb, size_t siz, char *file, int line);
void *_dalloc_realloc(void *p, size_t siz, char *file, int line);
void *_dalloc_reallocarray(void *p, size_t n, size_t s, char *file, int line);
char *_dalloc_strdup(const char *s, char *file, int line);
char *_dalloc_strndup(const char *s, size_t n, char *file, int line);
#if __STDC_VERSION__ >= 199901L
int _dalloc_vasprintf(char **p, const char *fmt, va_list ap, char *file, int line);
int _dalloc_asprintf(char **p, char *file, int line, const char *fmt, ...);
#endif /* __STDC_VERSION__ >= 199901L */
#else
#define dalloc_check_overflow() 0
#define dalloc_check_free()
#define dalloc_check_all()
#define dalloc_ignore(p)
#define dalloc_comment(p, str)
#define dalloc_query(p)
#endif /* DALLOC || DALLOC_INTERNAL */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DALLOC_H */
