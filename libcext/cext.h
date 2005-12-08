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

/* container.c */
typedef struct Container Container;
typedef struct CItem CItem;

struct CItem {
	void *item;
	CItem *next;
	CItem *up;
	CItem *down;
};

struct Container {
	CItem *list;
	CItem *stack;
};

void cext_attach_item(Container *c, void *item);
void cext_detach_item(Container *c, void *item);
void *cext_find_item(Container *c, void *pattern, int (*comp)(void *pattern, void *item));
void cext_iterate(Container *c, void *aux, void (*iter)(void *, void *aux));
void cext_swap_items(Container *c, void *item1, void *item2);
size_t cext_sizeof(Container *c);
void cext_stack_top_item(Container *c, void *item);
void *cext_stack_get_top_item(Container *c);
void *cext_stack_get_down_item(Container *c, void *item);
void *cext_stack_get_up_item(Container *c, void *item);
void *cext_list_get_item(Container *c, size_t index);
int cext_list_get_item_index(Container *c, void *item);
void *cext_list_get_next_item(Container *c, void *item);
void *cext_list_get_prev_item(Container *c, void *item);

/* emallocz.c */
void *cext_emallocz(size_t size);

/* estrdup.c */
char *cext_estrdup(const char *s);

/* strlcat.c */
size_t cext_strlcat(char *dst, const char *src, size_t siz);

/* strlcpy.c */
size_t cext_strlcpy(char *dst, const char *src, size_t siz);

/* strtonum.c */
long long cext_strtonum(const char *numstr, long long minval, long long maxval, const char **errstrp);

/* tokenize.c */
size_t cext_tokenize(char **result, size_t reslen, char *str, char delim);
