/*
 * Copyright © 2023, Ratakor <ratakor@disroot.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DALLOC_INTERNAL
#include "dalloc.h"

#define EXIT_STATUS   9

#ifdef DALLOC
#define MIN(X, Y)     ((X) < (Y) ? (X) : (Y))
#define OVER_ALLOC    64
#define MAGIC_NUMBER  0x99
#define MAX_POINTERS  512
#define COMMENT_MAX   128

struct Pointer {
	void *p;
	size_t siz;
	char comment[COMMENT_MAX];
	int ignored;
	char file[FILENAME_MAX];
	int line;
};

static int overflow(void *p, size_t siz);
static size_t find_pointer_index(void *p, char *file, int line);
extern void dalloc_check_all(void) __attribute__((destructor));

static pthread_mutex_t dalloc_mutex = PTHREAD_MUTEX_INITIALIZER;
static char magic_numbers[OVER_ALLOC];
static struct Pointer pointers[MAX_POINTERS];
static size_t npointers;

static int
overflow(void *p, size_t siz)
{
	if (*magic_numbers == 0)
		memset(magic_numbers, MAGIC_NUMBER, OVER_ALLOC);

	return memcmp((char *)p + siz, magic_numbers, OVER_ALLOC);
}

static size_t
find_pointer_index(void *p, char *file, int line)
{
	size_t i = npointers;

	while (i-- > 0 && p != pointers[i].p);

	if (i == (size_t) -1) {
		fprintf(stderr, "%s:%d: dalloc: Unknown pointer %p\n",
		        file, line, p);
		pthread_mutex_unlock(&dalloc_mutex);
		exit(EXIT_STATUS);
	}

	return i;
}

size_t
dalloc_check_overflow(void)
{
	size_t i, sum = 0;

	pthread_mutex_lock(&dalloc_mutex);
	fprintf(stderr, "Memory overflow:");
	for (i = 0; i < npointers; i++) {
		if (!overflow(pointers[i].p, pointers[i].siz))
			continue;

		sum++;
		fprintf(stderr, "\n%s:%d: %p, total: %zu bytes",
		        pointers[i].file, pointers[i].line,
		        pointers[i].p, pointers[i].siz);
		if (pointers[i].comment[0])
			fprintf(stderr, " /* %s */", pointers[i].comment);
	}
	pthread_mutex_unlock(&dalloc_mutex);

	if (sum == 0)
		fprintf(stderr, " 0 overflow :)\n");
	else
		fprintf(stderr, "\nTotal overflow: %zu\n", sum);

	return sum;
}

void
dalloc_check_free(void)
{
	size_t i, n = 0, sum = 0;

	pthread_mutex_lock(&dalloc_mutex);
	fprintf(stderr, "Memory allocated and not freed:");
	for (i = 0; i < npointers; i++) {
		if (pointers[i].ignored)
			continue;

		n++;
		sum += pointers[i].siz;
		fprintf(stderr, "\n%s:%d: %p, %zu bytes",
		        pointers[i].file, pointers[i].line,
		        pointers[i].p, pointers[i].siz);
		if (pointers[i].comment[0])
			fprintf(stderr, " /* %s */", pointers[i].comment);
	}
	pthread_mutex_unlock(&dalloc_mutex);

	if (sum == 0)
		fprintf(stderr, " 0 byte :)\n");
	else
		fprintf(stderr, "\nTotal: %zu bytes, %zu pointers\n", sum, n);
}

void
dalloc_check_all(void)
{
	dalloc_check_overflow();
	dalloc_check_free();
}

void
_dalloc_ignore(void *p, char *file, int line)
{
	size_t i;

	pthread_mutex_lock(&dalloc_mutex);
	i = find_pointer_index(p, file, line);
	pointers[i].ignored = 1;
	pthread_mutex_unlock(&dalloc_mutex);
}

void
_dalloc_comment(void *p, const char *comment, char *file, int line)
{
	size_t i, siz;

	if (comment == NULL)
		return;

	siz = MIN(strlen(comment), COMMENT_MAX - 1);
	pthread_mutex_lock(&dalloc_mutex);
	i = find_pointer_index(p, file, line);
	memcpy(pointers[i].comment, comment, siz);
	pointers[i].comment[siz] = '\0';
	pthread_mutex_unlock(&dalloc_mutex);
}

void
_dalloc_free(void *p, char *file, int line)
{
	size_t i;

	if (p == NULL)
		return;

	pthread_mutex_lock(&dalloc_mutex);
	i = find_pointer_index(p, file, line);
	if (overflow(pointers[i].p, pointers[i].siz)) {
		fprintf(stderr, "%s:%d: dalloc: "
		        "Memory overflow on %p, total: %zu bytes\n",
		        file, line, pointers[i].p, pointers[i].siz);
		fprintf(stderr, "The pointer ");
		if (pointers[i].comment[0])
			fprintf(stderr, "'%s' ", pointers[i].comment);
		fprintf(stderr, "was allocated in '%s' on line %d.\n",
		        pointers[i].file, pointers[i].line);
		pthread_mutex_unlock(&dalloc_mutex);
		exit(EXIT_STATUS);
	}

	for (i++; i < npointers; i++)
		pointers[i - 1] = pointers[i];
	npointers--;
	free(p);
	pthread_mutex_unlock(&dalloc_mutex);
}

void *
_dalloc_malloc(size_t siz, char *file, int line)
{
	void *p = NULL;
	size_t sizfname;

	if (siz == 0) {
		fprintf(stderr, "%s:%d: dalloc: malloc with size == 0\n",
		        file, line);
		return NULL;
	}

	if (npointers == MAX_POINTERS) {
		fprintf(stderr, "dalloc: Too much pointers (max:%d)\n",
		        MAX_POINTERS);
		exit(EXIT_STATUS);
	}

	if (siz + OVER_ALLOC < OVER_ALLOC)
		errno = ENOMEM;
	else
		p = malloc(siz + OVER_ALLOC);

	if (p == NULL) {
		fprintf(stderr, "%s:%d: dalloc: %s\n",
		        file, line, strerror(errno));
		exit(EXIT_STATUS);
	}

	sizfname = MIN(strlen(file), FILENAME_MAX - 1);
	memset((char *)p + siz, MAGIC_NUMBER, OVER_ALLOC);
	pthread_mutex_lock(&dalloc_mutex);
	pointers[npointers].p = p;
	pointers[npointers].siz = siz;
	memcpy(pointers[npointers].file, file, sizfname);
	pointers[npointers].file[sizfname] = '\0';
	pointers[npointers].line = line;
	npointers++;
	pthread_mutex_unlock(&dalloc_mutex);

	return p;
}

void *
_dalloc_calloc(size_t nmemb, size_t siz, char *file, int line)
{
	void *p;

	if (nmemb == 0 || siz == 0) {
		fprintf(stderr, "%s:%d: dalloc: calloc with size == 0\n",
		        file, line);
		return NULL;
	}

	if (nmemb > -1 / siz) {
		fprintf(stderr, "%s:%d: dalloc: calloc: %s\n",
		        file, line, strerror(ENOMEM));
		exit(EXIT_STATUS);
	}

	siz *= nmemb;
	p = _dalloc_malloc(siz, file, line);
	memset(p, 0, siz);

	return p;
}

void *
_dalloc_realloc(void *p, size_t siz, char *file, int line)
{
	size_t i, sizfname;

	if (p == NULL)
		return _dalloc_malloc(siz, file, line);

	if (siz == 0) {
		_dalloc_free(p, file, line);
		return NULL;
	}

	pthread_mutex_lock(&dalloc_mutex);
	i = find_pointer_index(p, file, line);

	if (overflow(pointers[i].p, pointers[i].siz)) {
		fprintf(stderr, "%s:%d: dalloc: "
		        "Memory overflow on %p, total: %zu bytes\n",
		        file, line, pointers[i].p, pointers[i].siz);
		fprintf(stderr, "The pointer ");
		if (pointers[i].comment[0])
			fprintf(stderr, "'%s' ", pointers[i].comment);
		fprintf(stderr, "was allocated in '%s' on line %d.\n",
		        pointers[i].file, pointers[i].line);
		pthread_mutex_unlock(&dalloc_mutex);
		exit(EXIT_STATUS);
	}

	if (siz + OVER_ALLOC < OVER_ALLOC) {
		p = NULL;
		errno = ENOMEM;
	} else {
		p = realloc(p, siz + OVER_ALLOC);
	}

	if (p == NULL) {
		fprintf(stderr, "%s:%d: dalloc: %s\n",
		        file, line, strerror(errno));
		pthread_mutex_unlock(&dalloc_mutex);
		exit(EXIT_STATUS);
	}

	sizfname = MIN(strlen(file), FILENAME_MAX - 1);
	memset((char *)p + siz, MAGIC_NUMBER, OVER_ALLOC);
	pointers[i].p = p;
	pointers[i].siz = siz;
	memcpy(pointers[i].file, file, sizfname);
	pointers[i].file[sizfname] = '\0';
	pointers[i].line = line;
	pthread_mutex_unlock(&dalloc_mutex);

	return p;
}

void *
_dalloc_reallocarray(void *p, size_t n, size_t s, char *file, int line)
{
	if (n == 0 || s == 0) {
		fprintf(stderr, "%s:%d: dalloc: reallocarray with size == 0\n",
		        file, line);
		return NULL;
	}

	if (n > -1 / s) {
		fprintf(stderr, "%s:%d: dalloc: reallocarray: %s\n",
		        file, line, strerror(ENOMEM));
		exit(EXIT_STATUS);
	}

	return _dalloc_realloc(p, n * s, file, line);
}

char *
_dalloc_strdup(const char *s, char *file, int line)
{
	char *p;
	size_t siz;

	siz = strlen(s) + 1;
	p = _dalloc_malloc(siz, file, line);
	memcpy(p, s, siz);

	return p;
}

char *
_dalloc_strndup(const char *s, size_t n, char *file, int line)
{
	const char *end;
	char *p;
	size_t siz;

	end = memchr(s, '\0', n);
	siz = (end ? (size_t)(end - s) : n) + 1;
	p = _dalloc_malloc(siz, file, line);
	memcpy(p, s, siz - 1);
	p[siz - 1] = '\0';

	return p;
}

int
_dalloc_vasprintf(char **p, const char *fmt, va_list ap, char *file, int line)
{
	va_list ap2;
	size_t siz;
	int rv;

	va_copy(ap2, ap);
	rv = vsnprintf(NULL, 0, fmt, ap2);
	va_end(ap2);
	if (rv < 0) {
		fprintf(stderr, "%s:%d: dalloc: asprintf: %s\n",
		        file, line, strerror(errno));
		exit(EXIT_STATUS);
	}
	siz = rv + 1;
	*p = _dalloc_malloc(siz, file, line);
	rv = vsnprintf(*p, siz, fmt, ap);
	if (rv < 0) {
		fprintf(stderr, "%s:%d: dalloc: asprintf: %s\n",
		        file, line, strerror(errno));
		exit(EXIT_STATUS);
	}

	return rv;
}

int
_dalloc_asprintf(char **p, char *file, int line, const char *fmt, ...)
{
	int rv;
	va_list ap;

	va_start(ap, fmt);
	rv = _dalloc_vasprintf(p, fmt, ap, file, line);
	va_end(ap);

	return rv;
}
#endif /* DALLOC */

void
dalloc_sighandler(int sig)
{
#ifdef _WIN32
	fprintf(stderr, "dalloc: signal %d\n", sig);
#else
	fprintf(stderr, "dalloc: %s\n", strsignal(sig));
#endif /* _WIN32 */

	exit(EXIT_STATUS);
}

void
exitsegv(int dummy)
{
	int *x = NULL;

	dalloc_check_all();
	*x = dummy;
}
