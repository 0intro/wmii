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
	if(p->sel >= p->narea)
		p->sel = p->narea - 1;
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
		i = cext_strtonum(arg, 0, p->narea - 1, &errstr);
		if(errstr)
			return;
	}
	new = p->area[i];
	if(new->nclient)
		focus_client(new->client[new->sel]);
	p->sel = i;
}

void
sendto_area(Area *to, Client *c)
{
	Area *a = c->area;

	cext_array_detach((void **)a->client, c, &a->clientsz);
	a->nclient--;
	if(a->sel >= a->nclient)
		a->sel = 0;

	attach_client2area(to, c);
	focus_client(c);
}

void
attach_client2area(Area *a, Client *c)
{
	Page *p = a->page;
	if(area2index(a) && a->maxclient && (a->maxclient == a->nclient)) {
		Area *to = nil;
		int i;
		for(i = p->sel; i < p->narea; i++) {
			to = p->area[i];
			if(!to->maxclient || (to->maxclient > to->nclient))
				break;
			to = nil;
		}
		if(!to) {
			to = alloc_area(p);
			sendto_area(to, a->client[a->sel]);
			arrange_page(p, True);
		}
		else
			sendto_area(to, a->client[a->sel]);
	}

	a->client = (Client **)cext_array_attach(
			(void **)a->client, c, sizeof(Client *), &a->clientsz);
	a->nclient++;
	c->area = a;
	if(p->sel > 0) /* column mode */
		arrange_column(a);
	else /* normal mode */
		resize_client(c, &c->frame.rect, nil, False);
}

