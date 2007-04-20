/* Written by Kris Maglione */
/* Public domain */
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include "dat.h"

/* Edit s/^([a-zA-Z].*)\n([a-z].*) {/\1 \2;/g  x/^([^a-zA-Z]|static|$)/-+d  s/ (\*map|val|*str)//g */

MapEnt *NM;

/* By Dan Bernstein. Public domain. */
static ulong
hash(const char *str) {
	ulong h;
	
	h = 5381;
	while (*str != '\0') {
		h += h << 5; /* h *= 33 */
		h ^= *str++;
	}
	return h;
}

static void
insert(MapEnt **e, ulong val, char *key) {
	MapEnt *te;
	
	te = emallocz(sizeof *te);
	te->hash = val;
	te->key = key;
	te->next = *e;
	*e = te;
}

static MapEnt**
mapgetp(Map *map, ulong val, int create) {
	MapEnt **e;

	e = &map->bucket[val%map->nhash];
	for(; *e; e = &(*e)->next)
		if((*e)->hash >= val) break;
	if(*e == nil || (*e)->hash != val) {
		if(create)
			insert(e, val, nil);
		else
			e = &NM;
	}
	return e;
}

static MapEnt**
hashgetp(Map *map, char *str, int create) {
	MapEnt **e;
	ulong h;
	int cmp;
	
	h = hash(str);
	e = mapgetp(map, h, create);
	if(*e && (*e)->key == nil)
		(*e)->key = str;
	else {
		for(; *e; e = &(*e)->next)
			if((*e)->hash > h || (cmp = strcmp((*e)->key, str)) >= 0)
				break;
		if(*e == nil || (*e)->hash > h || cmp > 0)
			if(create)
				insert(e, h, str);
	}
	return e;
}

MapEnt*
mapget(Map *map, ulong val, int create) {
	MapEnt **e;
	
	e = mapgetp(map, val, create);
	return *e;
}

MapEnt*
hashget(Map *map, char *str, int create) {
	MapEnt **e;
	
	e = hashgetp(map, str, create);
	return *e;
}

void*
maprm(Map *map, ulong val) {
	MapEnt **e, *te;
	void *ret;
	
	ret = nil;
	e = mapgetp(map, val, 0);
	if(*e) {
		te = *e;
		ret = te->val;
		*e = te->next;
		free(te);
	}
	return ret;
}

void*
hashrm(Map *map, char *str) {
	MapEnt **e, *te;
	void *ret;
	
	e = hashgetp(map, str, 0);
	if(*e) {
		te = *e;
		ret = te->val;
		*e = te->next;
		free(te);
	}
	return ret;
}
