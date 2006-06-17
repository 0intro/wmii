/* This file is derived from src/lib9p/intmap.c from plan9port */
/* See LICENCE.p9p for terms of use */
#include <stdlib.h>
#include "ixp.h"
#define USED(v) if(v){}else{}

struct Intlist {
	unsigned long	id;
	void*	aux;
	Intlist*	link;
};

static unsigned long
hashid(Intmap *map, unsigned long id)
{
	return id%map->nhash;
}

static void
nop(void *v)
{
	USED(v);
}

void
initmap(Intmap *m, unsigned long nhash, void *hash)
{
	m->nhash = nhash;
	m->hash = hash;
}

static Intlist**
llookup(Intmap *map, unsigned long id)
{
	Intlist **lf;

	for(lf=&map->hash[hashid(map, id)]; *lf; lf=&(*lf)->link)
		if((*lf)->id == id)
			break;
	return lf;	
}

void
freemap(Intmap *map, void (*destroy)(void*))
{
	int i;
	Intlist *p, *nlink;

	if(destroy == nil)
		destroy = nop;
	for(i=0; i<map->nhash; i++){
		for(p=map->hash[i]; p; p=nlink){
			nlink = p->link;
			destroy(p->aux);
			free(p);
		}
	}
}
void
execmap(Intmap *map, void (*run)(void*))
{
	int i;
	Intlist *p, *nlink;

	for(i=0; i<map->nhash; i++){
		for(p=map->hash[i]; p; p=nlink){
			nlink = p->link;
			run(p->aux);
		}
	}
}

void*
lookupkey(Intmap *map, unsigned long id)
{
	Intlist *f;
	void *v;

	if((f = *llookup(map, id)))
		v = f->aux;
	else
		v = nil;
	return v;
}

void*
insertkey(Intmap *map, unsigned long id, void *v)
{
	Intlist *f;
	void *ov;
	unsigned long h;

	if((f = *llookup(map, id))){
		/* no decrement for ov because we're returning it */
		ov = f->aux;
		f->aux = v;
	}else{
		f = cext_emallocz(sizeof(*f));
		f->id = id;
		f->aux = v;
		h = hashid(map, id);
		f->link = map->hash[h];
		map->hash[h] = f;
		ov = nil;
	}
	return ov;	
}

int
caninsertkey(Intmap *map, unsigned long id, void *v)
{
	Intlist *f;
	int rv;
	unsigned long h;

	if(*llookup(map, id))
		rv = 0;
	else{
		f = cext_emallocz(sizeof *f);
		f->id = id;
		f->aux = v;
		h = hashid(map, id);
		f->link = map->hash[h];
		map->hash[h] = f;
		rv = 1;
	}
	return rv;	
}

void*
deletekey(Intmap *map, unsigned long id)
{
	Intlist **lf, *f;
	void *ov;

	if((f = *(lf = llookup(map, id)))){
		ov = f->aux;
		*lf = f->link;
		free(f);
	}else
		ov = nil;
	return ov;
}
