/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <sys/types.h>

#ifndef nil
#define nil (void *)0
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/* array.c */
void          **attach_item_end(void **old, void *item, size_t size_item);
void          **attach_item_begin(void **old, void *item, size_t size_item);
void          **detach_item(void **old, void *item, size_t size_item);
int             index_item(void **items, void *item);
int             index_next_item(void **items, void *item);
int             index_prev_item(void **items, void *item);
int             count_items(void **items);

/* emalloc.c */
void           *emalloc(size_t size);

/* estrdup.c */
char           *estrdup(const char *s);

/* strlcat.c */
size_t          _strlcat(char *dst, const char *src, size_t siz);

/* strlcpy.c */
size_t          _strlcpy(char *dst, const char *src, size_t siz);

/* strtonum.c */
long long 
__strtonum(const char *numstr, long long minval,
	   long long maxval, const char **errstrp);

/* tokenize.c */
size_t          tokenize(char **result, size_t reslen, char *str, char delim);
