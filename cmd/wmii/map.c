/* Written by Kris Maglione */
/* Public domain */
#include "dat.h"
#include "fns.h"

/* Edit s/^([a-zA-Z].*)\n([a-z].*) {/\1 \2;/g  x/^([^a-zA-Z]|static|$)/-+d  s/ (\*map|val|*str)//g */

struct MapEnt {
	ulong		hash;
	const char*	key;
	void*		val;
	MapEnt*		next;
};

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
insert(MapEnt **e, ulong val, const char *key) {
	MapEnt *te;
	
	te = emallocz(sizeof *te);
	te->hash = val;
	te->key = key;
	te->next = *e;
	*e = te;
}

static MapEnt**
map_getp(Map *map, ulong val, int create) {
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
hash_getp(Map *map, const char *str, int create) {
	MapEnt **e;
	ulong h;
	int cmp;
	
	h = hash(str);
	e = map_getp(map, h, create);
	if(*e && (*e)->key == nil)
		(*e)->key = str;
	else {
		SET(cmp);
		for(; *e; e = &(*e)->next)
			if((*e)->hash > h || (cmp = strcmp((*e)->key, str)) >= 0)
				break;
		if(*e == nil || (*e)->hash > h || cmp > 0)
			if(create)
				insert(e, h, str);
	}
	return e;
}

void**
map_get(Map *map, ulong val, bool create) {
	MapEnt *e;
	
	e = *map_getp(map, val, create);
	return e ? &e->val : nil;
}

void**
hash_get(Map *map, const char *str, bool create) {
	MapEnt *e;
	
	e = *hash_getp(map, str, create);
	return e ? &e->val : nil;
}

void*
map_rm(Map *map, ulong val) {
	MapEnt **e, *te;
	void *ret;
	
	ret = nil;
	e = map_getp(map, val, 0);
	if(*e) {
		te = *e;
		ret = te->val;
		*e = te->next;
		free(te);
	}
	return ret;
}

void*
hash_rm(Map *map, const char *str) {
	MapEnt **e, *te;
	void *ret;
	
	ret = nil;
	e = hash_getp(map, str, 0);
	if(*e) {
		te = *e;
		ret = te->val;
		*e = te->next;
		free(te);
	}
	return ret;
}

