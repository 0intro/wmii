/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <sys/types.h>

#ifndef nil
#define nil (void *)0
#endif

/* emallocz.c */
void *cext_emallocz(unsigned int size);

/* estrdup.c */
char *cext_estrdup(const char *s);

/* strlcat.c */
unsigned int cext_strlcat(char *dst, const char *src, unsigned int siz);

/* strlcpy.c */
unsigned int cext_strlcpy(char *dst, const char *src, unsigned int siz);

/* tokenize.c */
unsigned int cext_tokenize(char **result, unsigned int reslen, char *str, char delim);

/* trim.c */
void cext_trim(char *str, const char *chars);

/* vector.c */
#define VECTOR(name, type) \
typedef struct { \
	unsigned int size; \
	type *data; \
} name
VECTOR(Vector, void *);

void cext_vattach(Vector *v, void *p);
void cext_vattachat(Vector *v, void *p, unsigned int pos);
void cext_vdetach(Vector *v, void *p);
