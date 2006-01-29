/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <sys/types.h>

#ifndef nil
#define nil (void *)0
#endif

/* array.c */
void ** cext_array_attach(void **array, void *p, size_t psize, size_t *size);
void cext_array_detach(void **array, void *p, size_t *size);

/* emallocz.c */
void *cext_emallocz(size_t size);

/* estrdup.c */
char *cext_estrdup(const char *s);

/* strlcat.c */
size_t cext_strlcat(char *dst, const char *src, size_t siz);

/* strlcpy.c */
size_t cext_strlcpy(char *dst, const char *src, size_t siz);

/* strtonum.c */
long long cext_strtonum(const char *numstr, long long minval,
                        long long maxval, const char **errstrp);

/* tokenize.c */
size_t cext_tokenize(char **result, size_t reslen, char *str, char delim);
