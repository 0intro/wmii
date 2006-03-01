/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

Area *
alloc_area(Page *p)
{
	static unsigned short id = 1;
	Area *a = cext_emallocz(sizeof(Area));
	a->page = p;
	a->id = id++;
	update_area_geometry(a);
	p->area = (Area **)cext_array_attach((void **)p->area, a, sizeof(Area *), &p->areasz);
	p->sel = p->narea;
	p->narea++;
    return a;
}

void
update_area_geometry(Area *a)
{
	a->rect = rect;
	a->rect.height -= brect.height;
}

void
destroy_area(Area *a)
{
	Page *p = a->page;
	if(a->client)
		free(a->client);
	cext_array_detach((void **)p->area, a, &p->areasz);
	p->narea--;
	free(a);
}

int
area2index(Area *a)
{
	int i;
	Page *p = a->page;
	for(i = 0; i < p->narea; i++)
		if(p->area[i] == a)
			return i;
	return -1;
}

int
aid2index(Page *p, unsigned short id)
{
	int i;
	for(i = 0; i < p->narea; i++)
		if(p->area[i]->id == id)
			return i;
	return -1;
}

void
select_area(Area *a, char *arg)
{
	Area *new;
	Page *p = a->page;
	int i = area2index(a);
	if(i == -1)
		return;
	if(!strncmp(arg, "prev", 5)) {
		if(i == 1)
			i = p->narea - 1;
		else
			i--;
	} else if(!strncmp(arg, "next", 5)) {
		if(i + 1 < p->narea)
			i++;
		else
			i = 1;
	}
	else {
		const char *errstr;
		i = cext_strtonum(arg, 1, p->narea, &errstr);
		if(errstr)
			return;
		i--;
	}
	new = p->area[i];
	if(new->nclient)
		focus_client(new->client[new->sel]);
	p->sel = i;
}

void
sendto_area(Area *new, Client *c)
{
	Area *a = c->area;

	cext_array_detach((void **)a->client, c, &a->clientsz);
	a->nclient--;
	if(a->sel >= a->nclient)
		a->sel = 0;

	new->client = (Client **)cext_array_attach(
			(void **)new->client, c, sizeof(Client *), &new->clientsz);
	new->nclient++;

	c->area = new;
	arrange_column(a);
	arrange_column(new);
	focus_client(c);
}
