/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stdlib.h>
#include <string.h>
#include "util.h"

/* Blech. */
#define VECTOR(type, nam, c) \
void                                                                    \
vector_##c##init(Vector_##nam *v) {                                     \
	memset(v, 0, sizeof *v);                                        \
}                                                                       \
									\
void                                                                    \
vector_##c##free(Vector_##nam *v) {                                     \
	free(v->ary);                                                   \
	memset(v, 0, sizeof *v);                                        \
}                                                                       \
									\
void                                                                    \
vector_##c##push(Vector_##nam *v, type val) {                           \
	if(v->n == v->size) {                                           \
		if(v->size == 0)                                        \
			v->size = 2;                                    \
		v->size <<= 2;                                          \
		v->ary = erealloc(v->ary, v->size * sizeof *v->ary);    \
	}                                                               \
	v->ary[v->n++] = val;                                           \
}                                                                       \

VECTOR(long, long, l)
VECTOR(Rectangle, rect, r)
VECTOR(void*, ptr, p)
