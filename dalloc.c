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
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DALLOC
#define NO_DALLOC     "dalloc: Define `DALLOC` to enable dalloc"
#else
void dalloc_check_all(void) __attribute__((destructor));
#undef DALLOC
#endif /* DALLOC */
#include "dalloc.h"

#define MIN(X, Y)     ((X) < (Y) ? (X) : (Y))
#define OVER_ALLOC    64
#define MAGIC_NUMBER  0x99
#define EXIT_STATUS   9
#define MAX_POINTERS  1024
#ifndef PATH_MAX
#define PATH_MAX      256
#endif /* PATH_MAX */

struct Pointer {
	void *p;
	size_t siz;
	char file[PATH_MAX];
	int line;
};

static int overflow(unsigned char *p, size_t siz);

static pthread_mutex_t dalloc_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct Pointer pointers[MAX_POINTERS];
static size_t npointers;

static int
overflow(unsigned char *p, size_t siz)
{
	size_t i = 0;

	while (p[siz + i] == MAGIC_NUMBER && ++i < OVER_ALLOC);

	return i < OVER_ALLOC;
}

size_t
dalloc_check_overflow(void)
{
	size_t i, sum = 0;

#ifdef NO_DALLOC
	fprintf(stderr, "%s\n", NO_DALLOC);
	return 0;
#endif /* NO_DALLOC */

	pthread_mutex_lock(&dalloc_mutex);
	fprintf(stderr, "Memory overflow:");
	for (i = 0; i < npointers; i++) {
		if (!overflow(pointers[i].p, pointers[i].siz))
			continue;

		sum++;
		fprintf(stderr, "\n%s:%d: %p, total: %zu bytes",
		        pointers[i].file, pointers[i].line,
		        pointers[i].p, pointers[i].siz);
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
	size_t i, sum = 0;

#ifdef NO_DALLOC
	fprintf(stderr, "%s\n", NO_DALLOC);
	return;
#endif /* NO_DALLOC */

	pthread_mutex_lock(&dalloc_mutex);
	fprintf(stderr, "Memory allocated and not freed:");
	for (i = 0; i < npointers; i++) {
		sum += pointers[i].siz;
		fprintf(stderr, "\n%s:%d: %p, %zu bytes",
		        pointers[i].file, pointers[i].line,
		        pointers[i].p, pointers[i].siz);
	}
	pthread_mutex_unlock(&dalloc_mutex);

	if (sum == 0)
		fprintf(stderr, " 0 byte :)\n");
	else
		fprintf(stderr, "\nTotal: %zu bytes, %zu pointers\n", sum, i);
}

void
dalloc_check_all(void)
{
#ifdef NO_DALLOC
	fprintf(stderr, "%s\n", NO_DALLOC);
	return;
#endif /* NO_DALLOC */

	dalloc_check_overflow();
	dalloc_check_free();
}

void
dalloc_sighandler(int sig)
{
	fprintf(stderr, "dalloc: %s\n", strsignal(sig));
	exit(EXIT_STATUS);
}

void
dfree(void *p, char *file, int line)
{
	size_t i = 0, j;

	if (p == NULL)
		return;

	pthread_mutex_lock(&dalloc_mutex);
	while (p != pointers[i].p && ++i < npointers);
	if (i == npointers) {
		fprintf(stderr, "%s:%d: dalloc: Double free\n", file, line);
		pthread_mutex_unlock(&dalloc_mutex);
		exit(EXIT_STATUS);
	}

	if (overflow(pointers[i].p, pointers[i].siz)) {
		fprintf(stderr, "%s:%d: dalloc: "
		        "Memory overflow on %p, total: %zu bytes\n",
		        file, line, pointers[i].p, pointers[i].siz);
		fprintf(stderr,
		        "The pointer was allocated in '%s' on line %d\n",
		        pointers[i].file, pointers[i].line);
		pthread_mutex_unlock(&dalloc_mutex);
		exit(EXIT_STATUS);
	}

	for (i++; i < npointers; i++) {
		pointers[i - 1].p = pointers[i].p;
		pointers[i - 1].siz = pointers[i].siz;
		for (j = 0; j < PATH_MAX && pointers[i].file[j] != '\0'; j++)
			pointers[i - 1].file[j] = pointers[i].file[j];
		pointers[i - 1].file[j] = '\0';
		pointers[i - 1].line = pointers[i].line;
	}
	npointers--;
	free(p);
	pthread_mutex_unlock(&dalloc_mutex);
}


void *
dmalloc(size_t siz, char *file, int line)
{
	void *p = NULL;
	size_t i;

	if (siz == 0)
		return NULL;

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

	memset((unsigned char *)p + siz, MAGIC_NUMBER, OVER_ALLOC);
	pthread_mutex_lock(&dalloc_mutex);
	pointers[npointers].p = p;
	pointers[npointers].siz = siz;
	for (i = 0; i < PATH_MAX - 1 && file[i] != '\0'; i++)
		pointers[npointers].file[i] = file[i];
	pointers[npointers].file[i] = '\0';
	pointers[npointers].line = line;
	npointers++;
	pthread_mutex_unlock(&dalloc_mutex);

	return p;
}

void *
dcalloc(size_t nmemb, size_t siz, char *file, int line)
{
	void *p;

	if (siz != 0 && nmemb > -1 / siz) {
		fprintf(stderr, "%s:%d: dalloc: calloc: %s\n",
		        file, line, strerror(ENOMEM));
		exit(EXIT_STATUS);
	}

	siz *= nmemb;
	p = dmalloc(siz, file, line);
	memset(p, 0, siz);

	return p;
}

void *
drealloc(void *p, size_t siz, char *file, int line)
{

	void *np;
	size_t i = 0, osiz;

	if (p == NULL)
		return dmalloc(siz, file, line);

	if (siz == 0) {
		dfree(p, file, line);
		return NULL;
	}

	pthread_mutex_lock(&dalloc_mutex);
	while (p != pointers[i].p && ++i < npointers);
	if (i == npointers) {
		fprintf(stderr, "%s:%d: dalloc: realloc: Unknown pointer %p\n",
		        file, line, p);
		pthread_mutex_unlock(&dalloc_mutex);
		exit(EXIT_STATUS);
	}
	osiz = pointers[i].siz;
	pthread_mutex_unlock(&dalloc_mutex);

	np = dmalloc(siz, file, line);
	memcpy(np, p, MIN(osiz, siz));
	dfree(p, file, line);

	return np;
}

void *
dreallocarray(void *p, size_t nmemb, size_t siz, char *file, int line)
{
	if (siz != 0 && nmemb > -1 / siz) {
		fprintf(stderr, "%s:%d: dalloc: reallocarray: %s\n",
		        file, line, strerror(ENOMEM));
		exit(EXIT_STATUS);
	}

	return drealloc(p, nmemb * siz, file, line);
}

char *
dstrdup(const char *s, char *file, int line)
{
	char *p;
	size_t siz;

	siz = strlen(s) + 1;
	p = dmalloc(siz, file, line);
	memcpy(p, s, siz);

	return p;
}

char *
dstrndup(const char *s, size_t n, char *file, int line)
{
	const char *end;
	char *p;
	size_t siz;

	end = memchr(s, '\0', n);
	siz = (end ? (size_t)(end - s) : n) + 1;
	p = dmalloc(siz, file, line);
	memcpy(p, s, siz - 1);
	p[siz - 1] = '\0';

	return p;
}

void
exitsegv(int dummy)
{
	int *x = NULL;

	dalloc_check_all();
	*x = dummy;
}
