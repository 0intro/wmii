/* Copyright ©2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

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

void
reinit(Regex *r, char *regx) {

	refree(r);

	if(regx[0] != '\0') {
		r->regex = estrdup(regx);
		r->regc = regcomp(regx);
	}
}

void
refree(Regex *r) {

	free(r->regex);
	free(r->regc);
	r->regex = nil;
	r->regc = nil;
}

void
uniq(char **toks) {
	char **p, **q;

	q = toks;
	if(*q == nil)
		return;
	for(p=q+1; *p; p++)
		if(strcmp(*q, *p))
			*++q = *p;
	*++q = nil;
}

char**
comm(int cols, char **toka, char **tokb) {
	Vector_ptr vec;
	char **ret;
	int cmp, len;

	len = 0;
	vector_pinit(&vec);
	while(*toka || *tokb) {
		if(!*toka)
			cmp = 1;
		else if(!*tokb)
			cmp = -1;
		else
			cmp = strcmp(*toka, *tokb);
		if(cmp < 0) {
			if(cols & CLeft) {
				vector_ppush(&vec, *toka);
				len += strlen(*toka) + 1;
			}
			toka++;
		}else if(cmp > 0) {
			if(cols & CRight) {
				vector_ppush(&vec, *tokb);
				len += strlen(*tokb) + 1;
			}
			tokb++;
		}else {
			if(cols & CCenter) {
				vector_ppush(&vec, *toka);
				len += strlen(*toka) + 1;
			}
			toka++;
			tokb++;
		}
	}
	vector_ppush(&vec, nil);
	ret = strlistdup((char**)vec.ary);
	free(vec.ary);
	return ret;
}

void
grep(char **list, Reprog *re, int flags) {
	char **p, **q;
	int res;

	q = list;
	for(p=q; *p; p++) {
		res = 0;
		if(re)
			res = regexec(re, *p, nil, 0);
		if(res && !(flags & GInvert)
		|| !res && (flags & GInvert))
			*q++ = *p;
	}
	*q = nil;
}

char*
join(char **list, char *sep) {
	Fmt f;
	char **p;

	if(fmtstrinit(&f) < 0)
		abort();

	for(p=list; *p; p++) {
		if(p != list)
			fmtstrcpy(&f, sep);
		fmtstrcpy(&f, *p);
	}

	return fmtstrflush(&f);
}

