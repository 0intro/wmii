/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

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
	else if(!strncmp("stack", arg, 6))
		return COL_STACK;
	else if(!strncmp("max", arg, 4))
		return COL_STACK;
	return -1;
}

void
arrange_column(Area *col)
{
	unsigned int i, yoff;
	unsigned int h, w, hdiff, wdiff;

	if(!col->nclient)
		return;

	switch(col->mode) {
	case COL_EQUAL:
		h = col->rect.height;
		h /= col->nclient;
		for(i = 0; i < col->nclient; i++) {
			Client *c = col->client[i];
			c->frame.rect = col->rect;
			c->frame.rect.y += i * h;
			if(i + 1 < col->nclient)
				c->frame.rect.height = h;
			else
				c->frame.rect.height =
					col->rect.height - c->frame.rect.y + col->rect.y;
			resize_client(c, &c->frame.rect, 0, True);
		}
		break;
	case COL_STACK:
		yoff = col->rect.y;
		h = col->rect.height - (col->nclient - 1) * bar_height();
		for(i = 0; i < col->nclient; i++) {
			Client *c = col->client[i];
			c->frame.rect = col->rect;
			c->frame.rect.y = yoff;
			if(i == col->sel)
				c->frame.rect.height = h;
			else
				c->frame.rect.height = bar_height();
			yoff += c->frame.rect.height;
			resize_client(c, &c->frame.rect, 0, True);
		}
		break;
	case COL_MAX:
		for(i = 0; i < col->nclient; i++) {
			Client *c = col->client[i];
			c->frame.rect = col->rect;
			resize_client(c, &c->frame.rect, 0, True);
		}
		break;
	default:
		break;
	}

	/* some relaxing due to potential increment gaps */
	h = w = 0;
	for(i = 0; i < col->nclient; i++) {
		h += col->client[i]->frame.rect.height;
		if(w < col->client[i]->frame.rect.width)
			w = col->client[i]->frame.rect.width;
	}
	if(col->mode == COL_MAX)
		h = col->client[i]->frame.rect.height;
	hdiff = (col->rect.height - h) / col->nclient;
	wdiff = (col->rect.width - w) / 2; 

	yoff = col->rect.y + hdiff / 2;
	for(i = 0; i < col->nclient; i++) {
		Client *c = col->client[i];
		c->frame.rect.x += wdiff;
		c->frame.rect.y = yoff;
		yoff = c->frame.rect.y + c->frame.rect.height + hdiff;
		resize_client(c, &c->frame.rect, 0, False);
	}
}

void
arrange_page(Page *p, Bool update_colums)
{
	unsigned int i;
	unsigned int width;

	if(p->narea == 1)
		return;
	
	width = rect.width / (p->narea - 1);
	for(i = 1; i < p->narea; i++) {
		Area *a = p->area[i];
		if(update_colums) {
			update_area_geometry(a);
			a->rect.x = (i - 1) * width;
			a->rect.width = width;
		}
		arrange_column(a);
	}
}

static void
match_horiz(Area *col, XRectangle *r)
{
	unsigned int i;

	for(i = 0; i < col->nclient; i++) {
		Client *c = col->client[i];
        c->frame.rect.x = r->x;
        c->frame.rect.width = r->width;
        resize_client(c, &c->frame.rect, nil, False);
    }
}

static void
drop_resize(Client *c, XRectangle *new)
{
    Area *west = nil, *east = nil, *col = c->area;
	Page *p = col->page;
    Client *north = nil, *south = nil;
	unsigned int i;

	for(i = 0; (i < p->narea) && (p->area[i] != col); i++);
    west = i ? p->area[i - 1] : nil;
    east = i + 1 < p->narea ? p->area[i + 1] : nil;

	for(i = 0; (i < col->nclient) && (col->client[i] != c); i++);
    north = i ? col->client[i - 1] : nil;
    south = i + 1 < col->nclient ? col->client[i + 1] : nil;

    /* horizontal resize */
    if(west && (new->x != c->frame.rect.x)) {
        west->rect.width = new->x - west->rect.x;
        col->rect.width += c->frame.rect.x - new->x;
        col->rect.x = new->x;
        match_horiz(west, &west->rect);
        match_horiz(col, &col->rect);
    }
    if(east && (new->x + new->width != c->frame.rect.x + c->frame.rect.width)) {
        east->rect.width -= new->x + new->width - east->rect.x;
        east->rect.x = new->x + new->width;
        col->rect.x = new->x;
        col->rect.width = new->width;
        match_horiz(col, &col->rect);
        match_horiz(east, &east->rect);
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
        if(tgt != src)
			sendto_area(tgt, c);
        else {
			for(i = 0; (i < src->nclient) &&
				 !blitz_ispointinrect(pt->x, pt->y, &src->client[i]->frame.rect); i++);
			if((i < src->nclient) && (c != src->client[i])) {
				unsigned int j = client2index(c);
				Client *tmp = src->client[j];
				src->client[j] = src->client[i];
				src->client[i] = tmp;
				arrange_column(src);
				focus_client(c);
            }
        }
    }
}

void
resize_column(Client *c, XRectangle *r, XPoint *pt)
{
    if((c->frame.rect.width == r->width)
       && (c->frame.rect.height == r->height))
        drop_moving(c, r, pt);
    else
        drop_resize(c, r);
}

Area *
new_column(Area *old)
{
	Page *p = old->page;
	Client *c = sel_client_of_page(p);
	Area *col;

	if(!area2index(old) || (old->nclient < 2))
		return nil;

	col = alloc_area(p);
	cext_array_detach((void **)old->client, c, &old->clientsz);
	old->nclient--;
	if(old->sel == old->nclient)
		old->sel = 0;
	col->client = (Client **)cext_array_attach((void **)col->client, c,
					sizeof(Client *), &col->clientsz);
	col->nclient++;

	c->area = col;
	arrange_page(p, True);
	focus_client(c);
	return col;
}
