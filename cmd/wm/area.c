/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

Area *
alloc_area(Tag *t)
{
	static unsigned short id = 1;
	Area *a = cext_emallocz(sizeof(Area));
	a->tag = t;
	a->id = id++;
	update_area_geometry(a);
	t->area = (Area **)cext_array_attach((void **)t->area, a, sizeof(Area *), &t->areasz);
	t->sel = t->narea;
	fprintf(stderr, "alloc_area: t->sel == %d\n", t->sel);
	t->narea++;
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
	Tag *t = a->tag;
	if(a->nclient)
		return;
	if(a->client)
		free(a->client);
	cext_array_detach((void **)t->area, a, &t->areasz);
	t->narea--;
	if(t->sel == t->narea) {
		if(t->narea)
			t->sel = t->narea - 1;
		else 
			t->sel = 0;
	}
	free(a);
}

int
area2index(Area *a)
{
	int i;
	Tag *t = a->tag;
	for(i = 0; i < t->narea; i++)
		if(t->area[i] == a)
			return i;
	return -1;
}

int
aid2index(Tag *t, unsigned short id)
{
	int i;
	for(i = 0; i < t->narea; i++)
		if(t->area[i]->id == id)
			return i;
	return -1;
}

void
select_area(Area *a, char *arg)
{
	Area *new;
	Tag *t = a->tag;
	int i = area2index(a);
	if(i == -1)
		return;
	if(!strncmp(arg, "prev", 5)) {
		if(i == 1)
			i = t->narea - 1;
		else
			i--;
	} else if(!strncmp(arg, "next", 5)) {
		if(i + 1 < t->narea)
			i++;
		else
			i = 1;
	}
	else {
		const char *errstr;
		i = cext_strtonum(arg, 0, t->narea - 1, &errstr);
		if(errstr)
			return;
	}
	new = t->area[i];
	if(new->nclient)
		focus_client(new->client[new->sel]);
	t->sel = i;
	fprintf(stderr, "select_area: t->sel == %d\n", t->sel);
}

void
send_toarea(Area *to, Client *c)
{
	detach_fromarea(c);
	attach_toarea(to, c);
	focus_client(c);
}

void
attach_toarea(Area *a, Client *c)
{
	a->client = (Client **)cext_array_attach(
			(void **)a->client, c, sizeof(Client *), &a->clientsz);
	a->nclient++;
	c->area = a;
	if(area2index(a)) /* column */
		arrange_area(a);
	else /* floating */
		resize_client(c, &c->frect, nil, False);
}

void
detach_fromarea(Client *c)
{
	Area *a = c->area;
	cext_array_detach((void **)a->client, c, &a->clientsz);
	a->nclient--;
	if(a->nclient) {
		if(a->sel >= a->nclient)
			a->sel = 0;
		arrange_area(a);
	}
	else {
		Tag *t = a->tag;
		destroy_area(a);
		arrange_tag(t, True);
	}
}

char *
mode2str(int mode)
{
	switch(mode) {
	case Colequal: return "equal"; break;
	case Colstack: return "stack"; break;
	case Colmax: return "max"; break;
	default: break;
	}
	return nil;		
}

int
str2mode(char *arg)
{
	if(!strncmp("equal", arg, 6))
		return Colequal;
	if(!strncmp("stack", arg, 6))
		return Colstack;
	if(!strncmp("max", arg, 4))
		return Colmax;
	return -1;
}

static void
relax_area(Area *a)
{
	unsigned int i, yoff, h, hdiff;

	if(!a->nclient)
		return;

	/* some relaxing from potential increment gaps */
	h = 0;
	for(i = 0; i < a->nclient; i++) {
		Client *c = a->client[i];
		if(a->mode == Colmax) {
			if(h < c->frect.height)
				h = c->frect.height;
		}
		else
			h += c->frect.height;
	}

	/* try to add rest space to all clients if not COL_STACK mode */
	if(a->mode != Colstack) {
		for(i = 0; (h < a->rect.height) && (i < a->nclient); i++) {
			Client *c = a->client[i];
			unsigned int tmp = c->frect.height;
			c->frect.height += (a->rect.height - h);
			resize_client(c, &c->frect, 0, True);
			h += (c->frect.height - tmp);
		}
	}

	hdiff = (a->rect.height - h) / a->nclient;
	yoff = a->rect.y + hdiff / 2;
	for(i = 0; i < a->nclient; i++) {
		Client *c = a->client[i];
		c->frect.x = a->rect.x + (a->rect.width - c->frect.width) / 2;
		c->frect.y = yoff;
		if(a->mode != Colmax)
			yoff = c->frect.y + c->frect.height + hdiff;
		resize_client(c, &c->frect, 0, False);
	}
}

void
arrange_area(Area *a)
{
	unsigned int i, yoff, h;

	if(!a->nclient)
		return;

	switch(a->mode) {
	case Colequal:
		h = a->rect.height;
		h /= a->nclient;
		for(i = 0; i < a->nclient; i++) {
			Client *c = a->client[i];
			c->frect = a->rect;
			c->frect.y += i * h;
			if(i + 1 < a->nclient)
				c->frect.height = h;
			else
				c->frect.height =
					a->rect.height - c->frect.y + a->rect.y;
			resize_client(c, &c->frect, 0, True);
		}
		break;
	case Colstack:
		yoff = a->rect.y;
		h = a->rect.height - (a->nclient - 1) * bar_height();
		for(i = 0; i < a->nclient; i++) {
			Client *c = a->client[i];
			c->frect = a->rect;
			c->frect.y = yoff;
			if(i == a->sel)
				c->frect.height = h;
			else
				c->frect.height = bar_height();
			yoff += c->frect.height;
			resize_client(c, &c->frect, 0, True);
		}
		break;
	case Colmax:
		for(i = 0; i < a->nclient; i++) {
			Client *c = a->client[i];
			c->frect = a->rect;
			resize_client(c, &c->frect, 0, True);
		}
		break;
	default:
		break;
	}

	relax_area(a);
}

void
arrange_tag(Tag *t, Bool updategeometry)
{
	unsigned int i;
	unsigned int width;

	if(t->narea == 1)
		return;
	
	width = rect.width / (t->narea - 1);
	for(i = 1; i < t->narea; i++) {
		Area *a = t->area[i];
		if(updategeometry) {
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
        c->frect.x = r->x;
        c->frect.width = r->width;
        resize_client(c, &c->frect, nil, False);
    }
}

static void
drop_resize(Client *c, XRectangle *new)
{
    Area *west = nil, *east = nil, *a = c->area;
	Tag *t = a->tag;
    Client *north = nil, *south = nil;
	unsigned int i;

	for(i = 1; (i < t->narea) && (t->area[i] != a); i++);
	/* first managed area is indexed 1, thus (i > 1) ? ... */
    west = (i > 1) ? t->area[i - 1] : nil;
    east = i + 1 < t->narea ? t->area[i + 1] : nil;

	for(i = 1; (i < a->nclient) && (a->client[i] != c); i++);
    north = i ? a->client[i - 1] : nil;
    south = i + 1 < a->nclient ? a->client[i + 1] : nil;

    /* horizontal resize */
    if(west && (new->x != c->frect.x)) {
        west->rect.width = new->x - west->rect.x;
        a->rect.width += c->frect.x - new->x;
        a->rect.x = new->x;
        match_horiz(west, &west->rect);
        match_horiz(a, &a->rect);
		relax_area(west);
    }
    if(east && (new->x + new->width != c->frect.x + c->frect.width)) {
        east->rect.width -= new->x + new->width - east->rect.x;
        east->rect.x = new->x + new->width;
        a->rect.x = new->x;
        a->rect.width = new->width;
        match_horiz(a, &a->rect);
        match_horiz(east, &east->rect);
		relax_area(east);
    }

    /* vertical resize */
    if(north && (new->y != c->frect.y)) {
        north->frect.height = new->y - north->frect.y;
        c->frect.height += c->frect.y - new->y;
        c->frect.y = new->y;
        resize_client(north, &north->frect, nil, False);
        resize_client(c, &c->frect, nil, False);
    }
    if(south && (new->y + new->height != c->frect.y + c->frect.height)) {
        south->frect.height -= new->y + new->height - south->frect.y;
        south->frect.y = new->y + new->height;
        c->frect.y = new->y;
        c->frect.height = new->height;
        resize_client(c, &c->frect, nil, False);
        resize_client(south, &south->frect, nil, False);
    }
	relax_area(a);
}

static void
drop_moving(Client *c, XRectangle *new, XPoint * pt)
{
    Area *tgt = nil, *src = c->area;
	Tag *t = src->tag;
	unsigned int i;

    if(!pt || src->nclient < 2)
        return;

	for(i = 1; (i < t->narea) &&
			!blitz_ispointinrect(pt->x, pt->y, &t->area[i]->rect); i++);
	if((tgt = ((i < t->narea) ? t->area[i] : nil))) {
        if(tgt != src) {
			send_toarea(tgt, c);
			arrange_area(tgt);
		}
        else {
			for(i = 0; (i < src->nclient) && !blitz_ispointinrect(
						pt->x, pt->y, &src->client[i]->frect); i++);
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
    if((c->frect.width == r->width)
       && (c->frect.height == r->height))
        drop_moving(c, r, pt);
    else
        drop_resize(c, r);
}

Area *
new_area(Tag *t)
{
	Area *a = alloc_area(t);
	arrange_tag(t, True);
	return a;
}

