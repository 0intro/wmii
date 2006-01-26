/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

static void
attach_column_to_array(Column *col, Column **array, size_t *size)
{
	size_t i;
	if(!array) {
		*size = 2;
		array = cext_emallocz(sizeof(Column *) * (*size));
	}
	for(i = 0; (i < (*size)) && array[i]; i++);
	if(i >= (*size)) {
		Column **tmp = array;
		(*size) *= 2;
		array = cext_emallocz(sizeof(Column *) * (*size));
		for(i = 0; tmp[i]; i++)
			array[i] = tmp[i];
		free(tmp);
	}
	array[i] = col; 
}

static void
detach_column_from_array(Column *col, Column **array)
{
	size_t i;
	for(i = 0; array[i] != col; i++);
	for(; array[i + 1]; i++)
		array[i] = array[i + 1];
	array[i] = nil;
}

void
arrange_column(Page *p, Column *col)
{
	size_t i, nc;
	unsigned int h;

	for(nc = 0; (nc < col->clientsz) && col->client[nc]; nc++);
	h = col->rect.height;
    if(nc)
		h /= nc;
	for(i = 0; (i < col->clientsz) && col->client[i]; i++) {
		Client *c = col->client[i];
        c->frame.rect = col->rect;
        c->frame.rect.y = p->rect_column.y + i * h;
        if(col->client[i + 1])
            c->frame.rect.height =
				p->rect_column.height - c->frame.rect.y + p->rect_column.y;
        else
            c->frame.rect.height = h;
        resize_client(c, &c->frame.rect, 0);
	}
}

void
arrange_page(Page *p)
{
	size_t i;
	for(i = 0; (i < p->columnsz) && p->column[i]; i++)
		arrange_column(p, p->column[i]);
}

void
attach_column(Client *c)
{
	Page *p = page[sel_page];
	Column *col = p->columnsz ? p->column[p->sel_column] : nil;

	if(!col) {
        col = cext_emallocz(sizeof(Column));
        col->rect = p->rect_column;
		attach_column_to_array(col, p->column, &p->columnsz);
		p->sel_column = 0;
    }

	c->column = col;
	attach_client_to_array(c, col->client, &col->clientsz);
    arrange_column(p, col);
}

static void
update_column_width(Page *p)
{
	size_t i;
	unsigned int width;

	for(i = 0; (i < p->columnsz) && p->column[i]; i++);
	if(!i)
		return;

    width = p->rect_column.width / i;
	for(i = 0; (i < p->columnsz) && p->column[i]; i++) {
        p->column[i]->rect = p->rect_column;
        p->column[i]->rect.x = i * width;
        p->column[i]->rect.width = width;
    }
    arrange_page(p);
}

void
detach_column(Client *c)
{
	Page *p = c->page;
	Column *col = c->column;

	detach_client_from_array(c, col->client);
	if(!col->client[0]) {
		detach_column_from_array(col, p->column);
		free(col);
		update_column_width(p);
	}
	else
		arrange_column(p, col);
} 

static void
match_horiz(Column *col, XRectangle *r)
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
	Page *p = c->page;
    Column *west = nil, *east = nil, *col = c->column;
    Client *north = nil, *south = nil;
	size_t i;

	for(i = 0; (i < p->columnsz) && p->column[i] && (p->column[i] != col); i++);
    west = i ? p->column[i - 1] : nil;
    east = (i < p->columnsz) && p->column[i + 1] ? p->column[i + 1] : nil;

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
	Page *p = c->page;
    Column *tgt = nil, *src = c->column;
	size_t i;

    if(!pt)
        return;

	for(i = 0; (i < p->columnsz) && p->column[i] &&
			!blitz_ispointinrect(pt->x, pt->y, &p->column[i]->rect); i++);
	tgt = (i < p->columnsz) ? p->column[i] : nil;
    if(tgt) {
        if(tgt != src) {
			if(src->clientsz <= 1 || !src->client[1])
				return;
			detach_client_from_array(c, src->client);
			attach_client_to_array(c, tgt->client, &tgt->clientsz);
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
		focus_client(c);
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
	Page *p = c->page;
    Column *col = c->column;
	size_t i;

	for(i = 0; (i < col->clientsz) && col->client[i] && (col->client[i] != c); i++);
	if(!strncmp(arg, "prev", 5)) {
		if(!i)
			for(i = 0; (i < col->clientsz) && col->client[i]; i++);
		focus_client(col->client[i - 1]);
		return;
	} else if(!strncmp(arg, "next", 5)) {
		if(col->client[i + 1])
			focus_client(col->client[i + 1]);
		else
			focus_client(col->client[0]);
		return;
	}
   
	for(i = 0; (i < p->columnsz) && p->column[i] && (p->column[i] != col); i++);
	if(!strncmp(arg, "west", 5)) {
		if(!i)
			for(i = 0; (i < p->columnsz) && p->column[i]; i++);
		col = p->column[i - 1];
	} else if(!strncmp(arg, "east", 5)) {
		if(p->column[i + 1])
			col = p->column[i + 1];
		else
			col = p->column[0];
	} else {
		const char *errstr;
		for(i = 0; (i < p->columnsz) && p->column[i]; i++);
		i = cext_strtonum(arg, 0, i - 1, &errstr);
		if(errstr)
			return;
		col = p->column[i];	
	}
	focus_client(col->client[col->sel]);
}

void
new_column(Page *p)
{
	Client *c = sel_client_of_page(p);
	Column *col, *old = c ? c->column : nil;
	size_t i;

	if(!old)
		return;

	for(i = 0; (i < old->clientsz) && old->client[i]; i++);

	if(i < 2)
		return;

    col = cext_emallocz(sizeof(Column));
    col->rect = p->rect_column;
	attach_column_to_array(col, p->column, &p->columnsz);
	p->sel_column = i;
	detach_client_from_array(c, old->client);
	attach_client_to_array(c, col->client, &col->clientsz);
	c->column = col;
	update_column_width(p);
	focus_client(c);
}
