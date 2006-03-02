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
	detach_client_area(c);
	attach_client2area(to, c);
	focus_client(c);
}

void
attach_client2area(Area *a, Client *c)
{
	Page *p = a->page;
	if(area2index(a) && a->capacity && (a->capacity == a->nclient)) {
		Area *to = nil;
		int i;
		for(i = p->sel; i < p->narea; i++) {
			to = p->area[i];
			if(!to->capacity || (to->capacity > to->nclient))
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
	if(p->sel > 0) /* area mode */
		arrange_area(a);
	else /* normal mode */
		resize_client(c, &c->frame.rect, nil, False);
}

void
detach_client_area(Client *c)
{
	Area *a = c->area;
	Page *p = a->page;
	int i = area2index(a);
	cext_array_detach((void **)a->client, c, &a->clientsz);
	a->nclient--;
	if(a->sel >= a->nclient)
		a->sel = 0;
	if(i) { /* area */
		if(a->capacity && (a->nclient < a->capacity)) {
			for(++i; i < p->narea; i++) {
				Area *tmp = p->area[i];
				if(!tmp->capacity)
					sendto_area(a, tmp->client[0]);
				else
					continue;
				if(!tmp->nclient)
					destroy_area(tmp);
				arrange_page(p, True);
				break;
			}
		}
		if(!a->nclient) {
			if(p->narea > 2) {
				destroy_area(a);
				arrange_page(p, True);
			}
		}
		else
			arrange_area(a);
	}
}

void
match_capacity(Area *a)
{
	while(a->nclient > a->capacity) {
		Client *c = a->client[a->nclient - 1];
		detach_client_area(c);
		attach_client(c);
	}
}

char *
colmode2str(ColumnMode mode)
{
	switch(mode) {
	case COL_EQUAL: return "equal"; break;
	case COL_STACK: return "stack"; break;
	case COL_MAX: return "max"; break;
	default: break;
	}
	return nil;		
}

ColumnMode
str2colmode(char *arg)
{
	if(!strncmp("equal", arg, 6))
		return COL_EQUAL;
	if(!strncmp("stack", arg, 6))
		return COL_STACK;
	if(!strncmp("max", arg, 4))
		return COL_MAX;
	return -1;
}

static void
relax_area(Area *a)
{
	unsigned int i, yoff, h, w, hdiff, wdiff;

	if(!a->nclient)
		return;

	/* some relaxing from potential increment gaps */
	h = w = 0;
	for(i = 0; i < a->nclient; i++) {
		Client *c = a->client[i];
		if(a->mode == COL_MAX) {
			if(h < c->frame.rect.height)
				h = c->frame.rect.height;
		}
		else
			h += c->frame.rect.height;
		if(w < c->frame.rect.width)
			w = c->frame.rect.width;
	}
	wdiff = (a->rect.width - w) / 2; 

	/* try to add rest space to all clients if not COL_STACK mode */
	if(a->mode != COL_STACK) {
		for(i = 0; (h < a->rect.height) && (i < a->nclient); i++) {
			Client *c = a->client[i];
			unsigned int tmp = c->frame.rect.height;
			c->frame.rect.height += (a->rect.height - h);
			resize_client(c, &c->frame.rect, 0, True);
			h += (c->frame.rect.height - tmp);
		}
	}

	hdiff = (a->rect.height - h) / a->nclient;
	yoff = a->rect.y + hdiff / 2;
	for(i = 0; i < a->nclient; i++) {
		Client *c = a->client[i];
		c->frame.rect.x = a->rect.x + wdiff;
		c->frame.rect.y = yoff;
		if(a->mode != COL_MAX)
			yoff = c->frame.rect.y + c->frame.rect.height + hdiff;
		resize_client(c, &c->frame.rect, 0, False);
	}
}

void
arrange_area(Area *a)
{
	unsigned int i, yoff, h;

	if(!a->nclient)
		return;

	switch(a->mode) {
	case COL_EQUAL:
		h = a->rect.height;
		h /= a->nclient;
		for(i = 0; i < a->nclient; i++) {
			Client *c = a->client[i];
			c->frame.rect = a->rect;
			c->frame.rect.y += i * h;
			if(i + 1 < a->nclient)
				c->frame.rect.height = h;
			else
				c->frame.rect.height =
					a->rect.height - c->frame.rect.y + a->rect.y;
			resize_client(c, &c->frame.rect, 0, True);
		}
		break;
	case COL_STACK:
		yoff = a->rect.y;
		h = a->rect.height - (a->nclient - 1) * bar_height();
		for(i = 0; i < a->nclient; i++) {
			Client *c = a->client[i];
			c->frame.rect = a->rect;
			c->frame.rect.y = yoff;
			if(i == a->sel)
				c->frame.rect.height = h;
			else
				c->frame.rect.height = bar_height();
			yoff += c->frame.rect.height;
			resize_client(c, &c->frame.rect, 0, True);
		}
		break;
	case COL_MAX:
		for(i = 0; i < a->nclient; i++) {
			Client *c = a->client[i];
			c->frame.rect = a->rect;
			resize_client(c, &c->frame.rect, 0, True);
		}
		break;
	default:
		break;
	}

	relax_area(a);
}

void
arrange_page(Page *p, Bool update_areas)
{
	unsigned int i;
	unsigned int width;

	if(p->narea == 1)
		return;
	
	width = rect.width / (p->narea - 1);
	for(i = 1; i < p->narea; i++) {
		Area *a = p->area[i];
		if(update_areas) {
			update_area_geometry(a);
			a->rect.x = (i - 1) * width;
			a->rect.width = width;
		}
		arrange_area(a);
	}
}

static void
match_horiz(Area *a, XRectangle *r)
{
	unsigned int i;

	for(i = 0; i < a->nclient; i++) {
		Client *c = a->client[i];
        c->frame.rect.x = r->x;
        c->frame.rect.width = r->width;
        resize_client(c, &c->frame.rect, nil, False);
    }
}

static void
drop_resize(Client *c, XRectangle *new)
{
    Area *west = nil, *east = nil, *a = c->area;
	Page *p = a->page;
    Client *north = nil, *south = nil;
	unsigned int i;

	for(i = 1; (i < p->narea) && (p->area[i] != a); i++);
	/* first managed area is indexed 1, thus (i > 1) ? ... */
    west = (i > 1) ? p->area[i - 1] : nil;
    east = i + 1 < p->narea ? p->area[i + 1] : nil;

	for(i = 1; (i < a->nclient) && (a->client[i] != c); i++);
    north = i ? a->client[i - 1] : nil;
    south = i + 1 < a->nclient ? a->client[i + 1] : nil;

    /* horizontal resize */
    if(west && (new->x != c->frame.rect.x)) {
        west->rect.width = new->x - west->rect.x;
        a->rect.width += c->frame.rect.x - new->x;
        a->rect.x = new->x;
        match_horiz(west, &west->rect);
        match_horiz(a, &a->rect);
		relax_area(west);
    }
    if(east && (new->x + new->width != c->frame.rect.x + c->frame.rect.width)) {
        east->rect.width -= new->x + new->width - east->rect.x;
        east->rect.x = new->x + new->width;
        a->rect.x = new->x;
        a->rect.width = new->width;
        match_horiz(a, &a->rect);
        match_horiz(east, &east->rect);
		relax_area(east);
    }

    /* vertical resize */
    if(north && (new->y != c->frame.rect.y)) {
        north->frame.rect.height = new->y - north->frame.rect.y;
        c->frame.rect.height += c->frame.rect.y - new->y;
        c->frame.rect.y = new->y;
        resize_client(north, &north->frame.rect, nil, False);
        resize_client(c, &c->frame.rect, nil, False);
    }
    if(south && (new->y + new->height != c->frame.rect.y + c->frame.rect.height)) {
        south->frame.rect.height -= new->y + new->height - south->frame.rect.y;
        south->frame.rect.y = new->y + new->height;
        c->frame.rect.y = new->y;
        c->frame.rect.height = new->height;
        resize_client(c, &c->frame.rect, nil, False);
        resize_client(south, &south->frame.rect, nil, False);
    }
	relax_area(a);
}

static void
drop_moving(Client *c, XRectangle *new, XPoint * pt)
{
    Area *tgt = nil, *src = c->area;
	Page *p = src->page;
	unsigned int i;

    if(!pt || src->nclient < 2)
        return;

	for(i = 1; (i < p->narea) &&
			!blitz_ispointinrect(pt->x, pt->y, &p->area[i]->rect); i++);
	if((tgt = ((i < p->narea) ? p->area[i] : nil))) {
        if(tgt != src) {
			sendto_area(tgt, c);
			arrange_area(tgt);
		}
        else {
			for(i = 0; (i < src->nclient) &&
				 !blitz_ispointinrect(pt->x, pt->y, &src->client[i]->frame.rect); i++);
			if((i < src->nclient) && (c != src->client[i])) {
				unsigned int j = client2index(c);
				Client *tmp = src->client[j];
				src->client[j] = src->client[i];
				src->client[i] = tmp;
				arrange_area(src);
				focus_client(c);
            }
        }
    }
}

void
resize_area(Client *c, XRectangle *r, XPoint *pt)
{
    if((c->frame.rect.width == r->width)
       && (c->frame.rect.height == r->height))
        drop_moving(c, r, pt);
    else
        drop_resize(c, r);
}

Area *
new_area(Area *old)
{
	Page *p = old->page;
	Client *c = sel_client_of_page(p);
	Area *a;

	if(!area2index(old) || (old->nclient < 2))
		return nil;

	a = alloc_area(p);
	cext_array_detach((void **)old->client, c, &old->clientsz);
	old->nclient--;
	if(old->sel == old->nclient)
		old->sel = 0;
	a->client = (Client **)cext_array_attach((void **)a->client, c,
					sizeof(Client *), &a->clientsz);
	a->nclient++;

	c->area = a;
	arrange_page(p, True);
	focus_client(c);
	return a;
}
