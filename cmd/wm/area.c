/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>

#include "wm.h"

Area *
alloc_area(Page *p)
{
	static unsigned short id = 1;
	Area *a = cext_emallocz(sizeof(Area));
	a->id = id++;
	p->area = (Area **)cext_array_attach((void **)p->area, a, sizeof(Area *), &p->areasz);
	p->narea++;
    return a;
}

void
destroy_area(Area *a)
{
	size_t i;
	while(a->nclient)
		detach_client(client[i], False);
	free(a->client);
	free(a);
}

int
area_to_index(Page *p, Area *a)
{
	int i;
	for(i = 0; i < p->narea; i++)
		if(p->area[i] == a)
			return i;
	return -1;
}

int
aid_to_index(Page *p, unsigned short id)
{
	int i;
	for(i = 0; i < p->narea; i++)
		if(p->area[i]->id == id)
			return i;
	return -1;
}
