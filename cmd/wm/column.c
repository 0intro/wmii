/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

void
arrange_column(Page *p, Area *col)
{
	size_t i;
	unsigned int h;

	for(i = 0; (i < col->clientsz) && col->client[i]; i++);
	h = col->rect.height;
    if(i)
		h /= i;
	for(i = 0; (i < col->clientsz) && col->client[i]; i++) {
		Client *c = col->client[i];
        c->frame.rect = col->rect;
        c->frame.rect.y += i * h;
        if((i + 1 < col->clientsz) && col->client[i + 1])
            c->frame.rect.height = h;
        else
            c->frame.rect.height =
				col->rect.height - c->frame.rect.y + col->rect.y;
        resize_client(c, &c->frame.rect, 0);
	}
}

void
arrange_page(Page *p)
{
	size_t i;
	for(i = 1; i < p->narea; i++)
		arrange_column(p, p->area[i]);
}

void
attach_column(Client *c)
{
	Page *p = page[sel];
	Area *col = p->narea && (p->sel > 0) ? p->area[p->sel] : nil;

	if(!col) {
        col = cext_emallocz(sizeof(Area));
        col->rect = rect;
		p->area = (Area **)cext_array_attach((void **)p->area, col,
						sizeof(Area *), &p->areasz);
		p->sel = p->narea;
		p->narea++;
    }

	c->area = col;
	col->client = (Client **)cext_array_attach((void **)col->client, c,
						sizeof(Client *), &col->clientsz);
    arrange_column(p, col);
}

static void
update_column_width(Page *p)
{
	size_t i;
	unsigned int width;

	if(p->narea == 1)
		return;

    width = rect.width / (p->narea - 1);
	for(i = 1; i < p->narea; i++) {
        p->area[i]->rect = rect;
        p->area[i]->rect.x = i * width;
        p->area[i]->rect.width = width;
    }
    arrange_page(p);
}

void
detach_column(Client *c)
{
	Area *col = c->area;
	Page *p = col->page;

	cext_array_detach((void **)col->client, c, &col->clientsz);
	if(!col->client[0]) {
		cext_array_detach((void **)p->area, col, &p->areasz);
		p->narea--;
		free(col);
		update_column_width(p);
	}
	else
		arrange_column(p, col);
} 

static void
match_horiz(Area *col, XRectangle *r)
{
	size_t i;

	for(i = 0; (i < col->clientsz) && col->client[i]; i++) {
		Client *c = col->client[i];
        c->frame.rect.x = r->x;
        c->frame.rect.width = r->width;
        resize_client(c, &c->frame.rect, nil);
    }
}

static void
drop_resize(Client *c, XRectangle *new)
{
    Area *west = nil, *east = nil, *col = c->area;
	Page *p = col->page;
    Client *north = nil, *south = nil;
	size_t i;

	for(i = 0; (i < p->narea) && (p->area[i] != col); i++);
    west = i ? p->area[i - 1] : nil;
    east = (i < p->areasz) && p->area[i + 1] ? p->area[i + 1] : nil;

	for(i = 0; (i < col->clientsz) && col->client[i] && (col->client[i] != c); i++);
    north = i ? col->client[i - 1] : nil;
    south = (i < col->clientsz) && col->client[i + 1] ? col->client[i + 1] : nil;

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
        resize_client(north, &north->frame.rect, nil);
        resize_client(c, &c->frame.rect, nil);
    }
    if(south && (new->y + new->height != c->frame.rect.y + c->frame.rect.height)) {
        south->frame.rect.height -= new->y + new->height - south->frame.rect.y;
        south->frame.rect.y = new->y + new->height;
        c->frame.rect.y = new->y;
        c->frame.rect.height = new->height;
        resize_client(c, &c->frame.rect, nil);
        resize_client(south, &south->frame.rect, nil);
    }
}

static void
drop_moving(Client *c, XRectangle *new, XPoint * pt)
{
    Area *tgt = nil, *src = c->area;
	Page *p = src->page;
	size_t i;

    if(!pt)
        return;

	for(i = 0; (i < p->areasz) && p->area[i] &&
			!blitz_ispointinrect(pt->x, pt->y, &p->area[i]->rect); i++);
	tgt = (i < p->areasz) ? p->area[i] : nil;
    if(tgt) {
        if(tgt != src) {
			if(src->clientsz <= 1 || !src->client[1])
				return;
			cext_array_detach((void **)src->client, c, &src->clientsz);
			tgt->client = (Client **)cext_array_attach((void **)tgt->client, c,
							sizeof(Client *), &tgt->clientsz);
            arrange_column(p, src);
            arrange_column(p, tgt);
        } else {
			for(i = 0; (i < src->clientsz) && src->client[i] &&
				 !blitz_ispointinrect(pt->x, pt->y, &src->client[i]->frame.rect); i++);
			if((i < src->clientsz) && src->client[i] && (c != src->client[i])) {
				size_t j;
				for(j = 0; (j < src->clientsz) && src->client[j] && (src->client[j] != c); j++);
				src->client[j] = src->client[i];
				src->client[i] = c;
				arrange_column(p, src);
            }
        }
		focus_client(c, False);
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

void
select_column(Client *c, char *arg)
{
    Area *col = c->area;
	Page *p = col->page;
	size_t i;

	for(i = 0; (i < col->clientsz) && col->client[i] && (col->client[i] != c); i++);
	if(!strncmp(arg, "prev", 5)) {
		if(!i)
			for(i = 0; (i < col->clientsz) && col->client[i]; i++);
		focus_client(col->client[i - 1], True);
		return;
	} else if(!strncmp(arg, "next", 5)) {
		if(col->client[i + 1])
			focus_client(col->client[i + 1], True);
		else
			focus_client(col->client[0], True);
		return;
	}
   
	for(i = 0; (i < p->areasz) && p->area[i] && (p->area[i] != col); i++);
	if(!strncmp(arg, "west", 5)) {
		if(!i)
			for(i = 0; (i < p->areasz) && p->area[i]; i++);
		col = p->area[i - 1];
	} else if(!strncmp(arg, "east", 5)) {
		if(p->area[i + 1])
			col = p->area[i + 1];
		else
			col = p->area[0];
	} else {
		const char *errstr;
		for(i = 0; (i < p->areasz) && p->area[i]; i++);
		i = cext_strtonum(arg, 0, i - 1, &errstr);
		if(errstr)
			return;
		col = p->area[i];	
	}
	focus_client(col->client[col->sel], True);
}

void
new_column(Page *p)
{
	Client *c = sel_client_of_page(p);
	Area *col, *old = c ? c->area : nil;

	if(!old || old->nclient < 2)
		return;

    col = cext_emallocz(sizeof(Area));
    col->rect = rect;
	p->area = (Area **)cext_array_attach((void **)p->area, col,
					sizeof(Area *), &p->areasz);
	p->sel = p->narea;
	p->narea++;
	cext_array_detach((void **)old->client, c, &old->clientsz);
	col->client = (Client **)cext_array_attach((void **)col->client, c,
					sizeof(Client *), &col->clientsz);
	c->area = col;
	update_column_width(p);
	focus_client(c, True);
}

/*
static void
swap_client(void *obj, char *arg)
{
	Page *p = obj;
	Client *c = sel_client_of_page(p);
    Area *west = nil, *east = nil, *col = c->area;
    Client *north = nil, *south = nil;
	size_t i;

	if(!col || !arg)
		return;

	for(i = 1; i < p->narea && (p->area[i] != col); i++);
    west = i ? p->area[i - 1] : nil;
    east = (i < p->areasz) && p->area[i + 1] ? p->area[i + 1] : nil;

	for(i = 0; (i < col->nclient) && (col->client[i] != c); i++);
    north = i ? col->client[i - 1] : nil;
    south = (i + 1 < col->nclient) ? col->client[i + 1] : nil;

	if(!strncmp(arg, "north", 6) && north) {
		col->client[i] = col->client[i - 1]; 
		col->client[i - 1] = c;
		arrange_column(p, col);
	} else if(!strncmp(arg, "south", 6) && south) {
		col->client[i] = col->client[i + 1];
		col->client[i + 1] = c;
		arrange_column(p, col);
	}
	else if(!strncmp(arg, "west", 5) && west) {
		col->client[i] = west->client[west->sel];
		west->client[west->sel] = c;
		west->client[west->sel]->area = west;
		col->client[i]->area = col;
		arrange_column(p, col);
		arrange_column(p, west);
	} else if(!strncmp(arg, "east", 5) && east) {
		col->client[i] = west->client[west->sel];
		col->client[i]->area = col;
		east->client[east->sel] = c;
		east->client[east->sel]->area = east;
		arrange_column(p, col);
		arrange_column(p, east);
	}
	focus_client(c);
}

*/
