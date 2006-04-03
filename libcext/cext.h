/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <sys/types.h>

#ifndef nil
#define nil (void *)0
#endif

/* array.c */
void ** cext_array_attach(void **array, void *p, unsigned int psize, unsigned int *size);
void cext_array_detach(void **array, void *p, unsigned int *size);

/* emallocz.c */
void *cext_emallocz(unsigned int size);

/* estrdup.c */
char *cext_estrdup(const char *s);

/* strlcat.c */
unsigned int cext_strlcat(char *dst, const char *src, unsigned int siz);

/* strlcpy.c */
unsigned int cext_strlcpy(char *dst, const char *src, unsigned int siz);

/* strtonum.c */
long long cext_strtonum(const char *numstr, long long minval,
		long long maxval, const char **errstrp);

/* tokenize.c */
unsigned int cext_tokenize(char **result, unsigned int reslen, char *str, char delim);

/* vector.c */
#define EVECTOR(name, type) \
typedef struct { \
	unsigned int size; \
	type *data; \
} name

EVECTOR(Vector, void*);

void cext_vattach(Vector *v, void *p);
void cext_vdetach(Vector *v, void *p);

