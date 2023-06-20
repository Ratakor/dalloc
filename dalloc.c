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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DALLOC
void dalloc(void) __attribute__((destructor));
#undef DALLOC
#endif /* DALLOC */
#include "dalloc.h"

#define MIN(X, Y)     ((X) < (Y) ? (X) : (Y))
#define OVER_ALLOC    32
#define MAGIC_NUMBER  0x99
#define EXIT_STATUS   9
#define MAX_POINTERS  1024
#ifndef PATH_MAX
#define PATH_MAX      256
#endif

struct Pointer {
	void *p;
	size_t siz;
	char file[PATH_MAX];
	int line;
};

static int overflow(unsigned char *p, size_t siz);

struct Pointer pointers[MAX_POINTERS];
static size_t npointers;

static int
overflow(unsigned char *p, size_t siz)
{
	size_t i;

	for (i = 0; i < OVER_ALLOC; i++) {
		if (p[siz + i] != MAGIC_NUMBER)
			break;
	}

	return i < OVER_ALLOC;
}

size_t
dalloc_check_overflow(void)
{
	size_t i, sum = 0;

	for (i = 0; i < npointers; i++) {
		if (!overflow(pointers[i].p, pointers[i].siz))
			continue;

		sum++;
		fprintf(stderr,
		        "%s:%d: dalloc: Memory overflow on %p size: %zu\n",
		        pointers[i].file,
		        pointers[i].line,
		        pointers[i].p,
		        pointers[i].siz);
	}
	if (sum > 0)
		fprintf(stderr, "Total overflow: %zu\n", sum);

	return sum;
}

void
dalloc_check_free(void)
{
	size_t i, sum = 0;

	fprintf(stderr, "Memory allocated and not freed:");
	for (i = 0; i < npointers; i++) {
		sum += pointers[i].siz;
		fprintf(stderr, "\n%s:%d: %zu bytes",
		        pointers[i].file, pointers[i].line, pointers[i].siz);
	}
	if (sum == 0)
		fprintf(stderr, " 0 byte\n");
	else
		fprintf(stderr, "\nTotal: %zu bytes %zu pointers\n", sum, i);
}

void
dalloc(void)
{
	fprintf(stderr, "dalloc recap:\n");
	dalloc_check_overflow();
	dalloc_check_free();
}

void
dfree(void *p, char *file, int line)
{
	size_t i, j, k;

	if (p == NULL)
		return;

	for (i = 0; i < npointers; i++) {
		if (p == pointers[i].p)
			break;
	}

	if (i == npointers) {
		fprintf(stderr,
		        "%s:%d: dalloc: Try to free a non allocated pointer\n",
		        file, line);
		exit(EXIT_STATUS);
	}

	if (overflow(pointers[i].p, pointers[i].siz)) {
		fprintf(stderr,
		        "%s:%d: dalloc: Memory overflow on %p size: %zu\n",
		        pointers[i].file, pointers[i].line,
		        pointers[i].p, pointers[i].siz);
		exit(EXIT_STATUS);
	}

	/* TODO: ugly */
	for (j = i + 1; j < npointers; j++) {
		pointers[j - 1].p = pointers[j].p;
		pointers[j - 1].siz = pointers[j].siz;
		for (k = 0; k < PATH_MAX && pointers[j].file[k] != '\0'; k++)
			pointers[j - 1].file[k] = pointers[j].file[k];
		pointers[j - 1].file[k] = '\0';
		pointers[j - 1].line = pointers[j].line;
	}
	npointers--;
	free(p);
}


void *
dmalloc(size_t siz, char *file, int line)
{
	void *p = NULL;
	size_t i;

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
		fprintf(stderr, "%s:%d: dalloc: %s\nsize: %zu\n",
		        file, line, strerror(errno), siz);
		exit(EXIT_STATUS);
	}

	memset((unsigned char *)p + siz, MAGIC_NUMBER, OVER_ALLOC);
	pointers[npointers].p = p;
	pointers[npointers].siz = siz;
	for (i = 0; i < PATH_MAX - 1 && file[i] != '\0'; i++)
		pointers[npointers].file[i] = file[i];
	pointers[npointers].file[i] = '\0';
	pointers[npointers].line = line;
	npointers++;

	return p;
}

void *
dcalloc(size_t nmemb, size_t siz, char *file, int line)
{
	void *p;

	if (siz != 0 && nmemb > -1 / siz) {
		fprintf(stderr, "%s:%d: dalloc: calloc: %s\n",
		        file, line, strerror(ENOMEM));
		fprintf(stderr, "nmemb: %zu size: %zu\n", nmemb, siz);
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
	size_t i;

	if (p == NULL)
		return dmalloc(siz, file, line);

	for (i = 0; i < npointers; i++) {
		if (p == pointers[i].p)
			break;
	}
	if (i == npointers) {
		fprintf(stderr, "%s:%d: dalloc: realloc: Unknown pointer %p\n",
		        file, line, p);
		exit(EXIT_STATUS);
	}

	np = dmalloc(siz, file, line);
	memcpy(np, p, MIN(pointers[i].siz, siz));
	dfree(p, file, line);

	return np;
}

void *
dreallocarray(void *p, size_t nmemb, size_t siz, char *file, int line)
{
	if (siz != 0 && nmemb > -1 / siz) {
		fprintf(stderr, "%s:%d: dalloc: reallocarray: %s\n",
		        file, line, strerror(ENOMEM));
		fprintf(stderr, "nmemb: %zu size: %zu\n", nmemb, siz);
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
	*x = dummy;
}
