/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <sys/types.h>

#ifndef nil
#define nil (void *)0
#endif

/* asprintf.c */
extern int cext_asprintf(char **str, char const *fmt, ...);

/* malloc.c */
extern void *cext_emallocz(unsigned int size);
extern void *cext_emalloc(unsigned int size);
extern void *cext_erealloc(void *ptr, unsigned int size);
extern char *cext_estrdup(const char *str);

/* strlcat.c */
extern unsigned int cext_strlcat(char *dst, const char *src, unsigned int siz);

/* strlcpy.c */
extern unsigned int cext_strlcpy(char *dst, const char *src, unsigned int siz);

/* tokenize.c */
extern unsigned int cext_tokenize(char **result, unsigned int reslen, char *str, char delim);

/* trim.c */
extern void cext_trim(char *str, const char *chars);

/* assert.c */
#define cext_assert(a) do { \
		if(!(a)) \
			cext_failed_assert(#a, __FILE__, __LINE__); \
	} while (0)
extern void cext_failed_assert(char *a, char *file, int line);
