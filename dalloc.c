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

#define DALLOC_INTERNAL
#include "dalloc.h"

#define EXIT_STATUS    EXIT_FAILURE
#define OVER_ALLOC     64
#define MAGIC_NUMBER   0xdead

#define x1             ((char)(MAGIC_NUMBER))
#define x2             x1, x1
#define x4             x2, x2
#define x8             x4, x4
#define x16            x8, x8
#define x32            x16, x16
#define x64            x32, x32
#define xCAT(X)        x##X
#define MAGIC_INIT(X)  xCAT(X)
#define OVERFLOW(p, s) (memcmp(((char *)(p)) + (s), magic_numbers, OVER_ALLOC))

typedef struct dalloc_ptr dalloc_ptr;
struct dalloc_ptr {
	void *p;
	size_t siz;
	char *comment;
	char *file;
	int ignored;
	int line;
	dalloc_ptr *next;
};

static void eprintf(const char *fmt, ...);
static char *xstrdup(const char *s, char *file, int line);
static dalloc_ptr *find_ptr(void *p, char *file, int line);
static void check_overflow(dalloc_ptr *dp, char *file, int line);

static const char magic_numbers[OVER_ALLOC] = { MAGIC_INIT(OVER_ALLOC) };
static pthread_mutex_t dalloc_mutex = PTHREAD_MUTEX_INITIALIZER;
static dalloc_ptr *head;

static void
eprintf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static char *
xstrdup(const char *s, char *file, int line)
{
	char *p;
	size_t siz;

	siz = strlen(s) + 1;
	if ((p = malloc(siz)) == NULL) {
		eprintf("%s:%d: dalloc: %s", file, line, strerror(errno));
		pthread_mutex_unlock(&dalloc_mutex);
		exit(EXIT_STATUS);
	}

	return memcpy(p, s, siz);
}

static dalloc_ptr *
find_ptr(void *p, char *file, int line)
{
	dalloc_ptr *dp;

	for (dp = head; dp && dp->p != p; dp = dp->next);

	if (dp == NULL) {
		eprintf("%s:%d: dalloc: Unknown pointer %p\n", file, line, p);
		pthread_mutex_unlock(&dalloc_mutex);
		exit(EXIT_STATUS);
	}

	return dp;
}

static void
check_overflow(dalloc_ptr *dp, char *file, int line)
{
	if (!OVERFLOW(dp->p, dp->siz))
		return;

	eprintf("%s:%d: dalloc: Memory overflow on %p, total: %zu bytes\n"
	        "The pointer ", file, line, dp->p, dp->siz);
	if (dp->comment)
		eprintf("'%s' ", dp->comment);
	eprintf("was allocated in '%s' on line %d.\n", dp->file, dp->line);
	pthread_mutex_unlock(&dalloc_mutex);
	exit(EXIT_STATUS);
}

size_t
dalloc_check_overflow(void)
{
	dalloc_ptr *dp;
	size_t sum = 0;

	pthread_mutex_lock(&dalloc_mutex);
	eprintf("Memory overflow:");
	for (dp = head; dp; dp = dp->next) {
		if (!OVERFLOW(dp->p, dp->siz))
			continue;

		sum++;
		eprintf("\n%s:%d: %p, total: %zu bytes",
		        dp->file, dp->line, dp->p, dp->siz);
		if (dp->comment)
			eprintf(" /* %s */", dp->comment);
	}
	pthread_mutex_unlock(&dalloc_mutex);

	if (sum == 0)
		eprintf(" 0 overflow :)\n");
	else
		eprintf("\nTotal overflow: %zu\n", sum);

	return sum;
}

void
dalloc_check_free(void)
{
	dalloc_ptr *dp;
	size_t n = 0, sum = 0;

	pthread_mutex_lock(&dalloc_mutex);
	eprintf("Memory allocated and not freed:");
	for (dp = head; dp; dp = dp->next) {
		if (dp->ignored)
			continue;

		n++;
		sum += dp->siz;
		eprintf("\n%s:%d: %p, %zu bytes",
		        dp->file, dp->line, dp->p, dp->siz);
		if (dp->comment)
			eprintf(" /* %s */", dp->comment);
	}
	pthread_mutex_unlock(&dalloc_mutex);

	if (sum == 0)
		eprintf(" 0 byte :)\n");
	else
		eprintf("\nTotal: %zu bytes, %zu pointers\n", sum, n);
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
	dalloc_ptr *dp;

	pthread_mutex_lock(&dalloc_mutex);
	dp = find_ptr(p, file, line);
	dp->ignored = 1;
	pthread_mutex_unlock(&dalloc_mutex);
}

void
_dalloc_comment(void *p, const char *comment, char *file, int line)
{
	dalloc_ptr *dp;

	if (comment == NULL)
		return;

	pthread_mutex_lock(&dalloc_mutex);
	dp = find_ptr(p, file, line);
	free(dp->comment);
	dp->comment = xstrdup(comment, file, line);
	pthread_mutex_unlock(&dalloc_mutex);
}

void
_dalloc_query(void *p, char *file, int line)
{
	dalloc_ptr *dp;

	pthread_mutex_lock(&dalloc_mutex);
	dp = find_ptr(p, file, line);
	fprintf(stderr, "%s:%d: dalloc: %p: %s:%d: %zu bytes",
	        file, line, dp->p, dp->file, dp->line, dp->siz);
	if (dp->comment)
		fprintf(stderr, " /* %s */", dp->comment);
	fputc('\n', stderr);
	pthread_mutex_unlock(&dalloc_mutex);
}

void
_dalloc_free(void *p, char *file, int line)
{
	dalloc_ptr *dp, *prev = NULL; /* cc whines */

	if (p == NULL)
		return;

	pthread_mutex_lock(&dalloc_mutex);
	for (dp = head; dp && dp->p != p; prev = dp, dp = dp->next);
	if (dp == NULL) {
		eprintf("%s:%d: dalloc: Unknown pointer %p\n", file, line, p);
		pthread_mutex_unlock(&dalloc_mutex);
		exit(EXIT_STATUS);
	}

	check_overflow(dp, file, line);

	if (dp == head)
		head = dp->next;
	else
		prev->next = dp->next;
	pthread_mutex_unlock(&dalloc_mutex);

	free(dp->p);
	free(dp->file);
	free(dp->comment);
	free(dp);
}

void *
_dalloc_malloc(size_t siz, char *file, int line)
{
	dalloc_ptr *dp;

	if (siz == 0) {
		eprintf("%s:%d: dalloc: malloc with size == 0\n", file, line);
		return NULL;
	}

	if (siz + OVER_ALLOC < OVER_ALLOC) {
		dp = NULL;
		errno = ENOMEM;
	} else if ((dp = calloc(1, sizeof(*dp))) != NULL) {
		dp->p = malloc(siz + OVER_ALLOC);
	}

	if (dp == NULL || dp->p == NULL) {
		eprintf("%s:%d: dalloc: %s\n", file, line, strerror(errno));
		exit(EXIT_STATUS);
	}

	memset((char *)dp->p + siz, MAGIC_NUMBER, OVER_ALLOC);
	dp->siz = siz;
	dp->line = line;

	pthread_mutex_lock(&dalloc_mutex);
	dp->file = xstrdup(file, file, line);
	dp->next = head;
	head = dp;
	pthread_mutex_unlock(&dalloc_mutex);

	return dp->p;
}

void *
_dalloc_calloc(size_t nmemb, size_t siz, char *file, int line)
{
	void *p;

	if (siz != 0 && nmemb > (size_t) -1 / siz) {
		eprintf("%s:%d: dalloc: calloc: %s\n",
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
	dalloc_ptr *dp;

	if (p == NULL)
		return _dalloc_malloc(siz, file, line);

	if (siz == 0) {
		eprintf("%s:%d: dalloc: realloc with size == 0\n", file, line);
		return NULL;
	}

	pthread_mutex_lock(&dalloc_mutex);
	dp = find_ptr(p, file, line);
	check_overflow(dp, file, line);

	if (siz + OVER_ALLOC < OVER_ALLOC) {
		dp->p = NULL;
		errno = ENOMEM;
	} else {
		dp->p = realloc(dp->p, siz + OVER_ALLOC);
	}

	if (dp->p == NULL) {
		eprintf("%s:%d: dalloc: %s\n", file, line, strerror(errno));
		pthread_mutex_unlock(&dalloc_mutex);
		exit(EXIT_STATUS);
	}

	memset((char *)dp->p + siz, MAGIC_NUMBER, OVER_ALLOC);
	dp->siz = siz;
	dp->line = line;
	free(dp->file);
	dp->file = xstrdup(file, file, line);
	pthread_mutex_unlock(&dalloc_mutex);

	return dp->p;
}

void *
_dalloc_reallocarray(void *p, size_t n, size_t s, char *file, int line)
{
	if (s != 0 && n > (size_t) -1 / s) {
		eprintf("%s:%d: dalloc: reallocarray: %s\n",
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
	char *p;
	size_t siz;

	siz = strnlen(s, n);
	p = _dalloc_malloc(siz + 1, file, line);
	memcpy(p, s, siz);
	p[siz] = '\0';

	return p;
}

#if __STDC_VERSION__ >= 199901L
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
		eprintf("%s:%d: dalloc: asprintf: %s\n",
		        file, line, strerror(errno));
		exit(EXIT_STATUS);
	}
	siz = (size_t)rv + 1;
	*p = _dalloc_malloc(siz, file, line);
	rv = vsnprintf(*p, siz, fmt, ap);
	if (rv < 0) {
		eprintf("%s:%d: dalloc: asprintf: %s\n",
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
#endif /* __STDC_VERSION__ >= 199901L */
