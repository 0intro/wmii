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

static void
xarrange_column(Page *p, Column *col)
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
arrange_column(Page *p)
{
	size_t i;
	for(i = 0; (i < p->columnsz) && p->column[i]; i++)
		xarrange_column(p, p->column[i]);
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
    xarrange_column(p, col);
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
    arrange_column(p);
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
		xarrange_column(p, col);
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
            xarrange_column(p, src);
            xarrange_column(p, tgt);
        } else {
			for(i = 0; (i < src->clientsz) && src->client[i] &&
				 !blitz_ispointinrect(pt->x, pt->y, &src->client[i]->frame.rect); i++);
			if((i < src->clientsz) && src->client[i] && (c != src->client[i])) {
				size_t j;
				for(j = 0; (j < src->clientsz) && src->client[j] && (src->client[j] != c); j++);
				src->client[j] = src->client[i];
				src->client[i] = c;
				xarrange_column(p, src);
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

/*
static void select_frame(void *obj, char *arg);
static void max_frame(void *obj, char *arg);
static void swap_frame(void *obj, char *arg);
static void new_column(void *obj, char *arg);

static Action lcol_acttbl[] = {
    {"select", select_frame},
    {"swap", swap_frame},
    {"new", new_column},
    {"max", max_frame},
    {0, 0}
};


static void
max_frame(void *obj, char *arg)
{
    Layout *l = obj;
    Acme *acme = l->aux;
    Column *c = acme->sel;
	Cell *cell;
    Frame *f;
	
    if(!c)
        return;

    cell = c->sel;
    if(!cell)
        return;

	f = cell->frame;
	if(f->maximized) {
		f->rect = f->old;
		resize_frame(f, &f->old, nil);
		f->maximized = False;
	}
	else {
		f->old = f->rect;
		f->rect = c->rect;
		XRaiseWindow(dpy, f->win);
		resize_frame(f, &c->rect, nil);
		f->maximized = True;
	}
}

static void
select_frame(void *obj, char *arg)
{
    Layout *l = obj;
    Acme *acme = l->aux;
    Column *c, *column = acme->sel;
    Cell *cell;

    if(!column)
        return;

    cell = column->sel;
    if(!cell || !arg)
        return;
    if(!strncmp(arg, "prev", 5)) {
        if(cell->prev)
            cell = cell->prev;
        else
            for(cell = column->cells; cell && cell->next; cell = cell->next);
    } else if(!strncmp(arg, "next", 5)) {
        if(cell->next)
            cell = cell->next;
        else
            cell = column->cells;
    } else if(!strncmp(arg, "west", 5)) {
        if(column->prev)
            cell = column->prev->sel;
        else {
            for(c = acme->columns; c && c->next; c = c->next);
			cell = c->sel;
		}
    } else if(!strncmp(arg, "east", 5)) {
        if(column->next)
            cell = column->next->sel;
        else
            cell = acme->columns->sel;
    } else {
        unsigned int i = 0, idx = blitz_strtonum(arg, 0, column->ncells - 1);
        for(cell = column->cells; cell && i != idx; cell = cell->next)
            i++;
    }
    if(cell && cell != column->sel)
        focus_column(l, cell->frame->sel, True);
}

static void
swap_frame(void *obj, char *arg)
{
    Layout *l = obj;
    Acme *acme = l->aux;
    Column *west = nil, *east = nil, *column = acme->sel;
    Cell *north = nil, *south = nil, *cell = column->sel;
    Frame *f;
    XRectangle r;

    if(!column || !cell || !arg)
        return;

    west = column->prev;
    east = column->next;
    north = cell->prev;
    south = cell->next;

	if(!west)
		west = east;
	if(!east)
		east = west;

    if(!strncmp(arg, "north", 6) && north) {
        r = north->frame->rect;
        north->frame->rect = cell->frame->rect;
        cell->frame->rect = r;
        f = north->frame;
        north->frame = cell->frame;
        cell->frame = f;
        cell->frame->aux = cell;
        north->frame->aux = north;
        resize_frame(cell->frame, &cell->frame->rect, nil);
        resize_frame(north->frame, &north->frame->rect, nil);
        focus_column(l, cell->frame->sel, True);
    } else if(!strncmp(arg, "south", 6) && south) {
        r = south->frame->rect;
        south->frame->rect = cell->frame->rect;
        cell->frame->rect = r;
        f = south->frame;
        south->frame = cell->frame;
        cell->frame = f;
        cell->frame->aux = cell;
        south->frame->aux = south;
        resize_frame(cell->frame, &cell->frame->rect, nil);
        resize_frame(south->frame, &south->frame->rect, nil);
        focus_column(l, cell->frame->sel, True);
    } else if(!strncmp(arg, "west", 5) && west && column->ncells && west->ncells) {
        Cell *other = west->sel;
        r = other->frame->rect;
        other->frame->rect = cell->frame->rect;
        cell->frame->rect = r;
        f = other->frame;
        other->frame = cell->frame;
        cell->frame = f;
        other->frame->aux = other;
        cell->frame->aux = cell;
        resize_frame(cell->frame, &cell->frame->rect, nil);
        resize_frame(other->frame, &other->frame->rect, nil);
        focus_column(l, other->frame->sel, True);
    } else if(!strncmp(arg, "east", 5) && east && column->ncells && east->ncells) {
        Cell *other = east->sel;
        r = other->frame->rect;
        other->frame->rect = cell->frame->rect;
        cell->frame->rect = r;
        f = other->frame;
        other->frame = cell->frame;
        cell->frame = f;
        other->frame->aux = other;
        cell->frame->aux = cell;
        resize_frame(cell->frame, &cell->frame->rect, nil);
        resize_frame(other->frame, &other->frame->rect, nil);
        focus_column(l, other->frame->sel, True);
    }
}

static void
new_column(void *obj, char *arg)
{
    Layout *l = obj;
    Acme *acme = l->aux;
    Column *c, *new, *column = acme->sel;
    Frame *f;

    if(!column || column->ncells < 2)
        return;

    f = column->sel->frame;
    for(c = acme->columns; c && c->next; c = c->next);

    new = cext_emallocz(sizeof(Column));
    new->rect = layout_rect;
    new->prev = c;
    c->next = new;
    acme->ncolumns++;
    acme->sel = new;

    detach_frame(l, f);
    attach_frame(l, new, f);

    update_column_width(l);
    focus_column(l, f->sel, True);
}
*/
