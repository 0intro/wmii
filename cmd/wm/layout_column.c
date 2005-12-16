/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wm.h"
#include "layoutdef.h"

typedef struct Acme Acme;
typedef struct Column Column;
typedef struct Cell Cell;

struct Acme {
	Column *sel;
	Column *columns;
	size_t ncolumns;
	Frame *frames;
	size_t nframes;
};

struct Cell {
	Frame *frame;
	Cell *next;
	Cell *prev;
	Column *col;
};

struct Column {
	Cell *sel;
	Cell *cells;
	size_t ncells;
	XRectangle rect;
	Column *prev;
	Column *next;
};

static void init_col(Area *a, Client *clients);
static Client *deinit_col(Area *a);
static void arrange_col(Area *a);
static Bool attach_col(Area *a, Client *c);
static void detach_col(Area *a, Client *c, Bool unmap);
static void resize_col(Frame *f, XRectangle *new, XPoint *pt);
static void focus_col(Frame *f, Bool raise);
static Frame *frames_col(Area *a);
static Frame *sel_col(Area *a);
static Action *actions_col(Area *a);

static void select_frame(void *obj, char *arg);
static void swap_frame(void *obj, char *arg);
static void new_col(void *obj, char *arg);

static Action lcol_acttbl[] = {
	{"select", select_frame},
	{"swap", swap_frame},
	{"new", new_col},
	{0, 0}
};

void init_layout_column()
{
	Layout *lp, *l = cext_emallocz(sizeof(Layout));
	l->name = "column";
	l->init = init_col;
	l->deinit = deinit_col;
	l->arrange = arrange_col;
	l->attach = attach_col;
	l->detach = detach_col;
	l->resize = resize_col;
	l->focus = focus_col;
	l->frames = frames_col;
	l->sel = sel_col;
	l->actions = actions_col;

	for (lp = layouts; lp && lp->next; lp = lp->next);
	if (lp)
		lp->next = l;
	else
		layouts = l;
}

static void arrange_column(Column *col)
{
	Cell *cell;
	unsigned int i = 0, h = area_rect.height / col->ncells;
	for (cell = col->cells; cell; cell = cell->next) {
		Frame *f = cell->frame;
		f->rect = col->rect;
		f->rect.y = area_rect.y + i * h;
		if (!cell->next)
			f->rect.height = area_rect.height - f->rect.y + area_rect.y;
		else
			f->rect.height = h;
		resize_frame(f, &f->rect, 0);
		i++;
	}
}

static void arrange_col(Area *a)
{
	Acme *acme = a->aux;
	Column *col;
	for (col = acme->columns; col; col = col->next)
		arrange_column(col);
	XSync(dpy, False);
}

static void init_col(Area *a, Client *clients)
{
	Client *n, *c = clients;

	a->aux = cext_emallocz(sizeof(Acme));
	while (c) {
		n = c->next;
		attach_col(a, c);
		c = n;
	}
}

static void attach_frame(Area *a, Column *col, Frame *f)
{
	Acme *acme = a->aux;
	Cell *c, *cell = cext_emallocz(sizeof(Cell));
	Frame *fr;
	
	cell->frame = f;
	cell->col = col;
	f->aux = cell;
	for (c = col->cells; c && c->next; c = c->next);
	if (!c) 
		col->cells = cell;
	else {
		c->next = cell;
		cell->prev = c;
	}
	col->sel = cell;
	col->ncells++;

	for (fr = acme->frames; fr && fr->next; fr = fr->next);
	if (!fr)
		acme->frames = f;
	else {
		fr->next = f;
		f->prev = fr;
	}
	attach_frame_to_area(a, f);
	acme->nframes++;

}

static void detach_frame(Area *a, Frame *f)
{
	Acme *acme = a->aux;
	Cell *cell = f->aux;
	Column *col = cell->col;

	if (col->sel == cell) {
		if (cell->prev)
			col->sel = cell->prev;
		else
			col->sel = nil;
	}
	if (cell->prev)
		cell->prev->next = cell->next;
	else
		col->cells = cell->next;
	if (cell->next)
		cell->next->prev = cell->prev;
	if (!col->sel)
		col->sel = col->cells;
	free(cell);
	col->ncells--;

	if (f->prev)
		f->prev->next = f->next;
	else
		acme->frames = f->next;
	if (f->next)
		f->next->prev = f->prev;
	f->aux = nil;
	detach_frame_from_area(f);
	acme->nframes--;
}

static Client *deinit_col(Area *a)
{
	Acme *acme = a->aux;
	Frame *f;
	Client *cl, *res = nil, *c = nil;
	Column *col = acme->columns;

	while ((f = acme->frames)) {
		while ((cl = f->clients)) {
			detach_client_from_frame(cl, False);
			cl->prev = cl->next = 0;
			if (!c)
				res = c = cl;
			else {
				c->next = cl;
				cl->prev = c;
				c = cl;	
			}
		}
		detach_frame(a, f);
		destroy_frame(f);
	}
	while ((col = acme->columns)) {
		acme->columns = col->next;
		free(col);
	}
	free(acme);
	a->aux = 0;
	return res;
}

static Bool attach_col(Area *a, Client *c)
{
	Acme *acme = a->aux;
	Column *col = acme->sel;
	Frame *f = nil;
	
	if (col && col->sel)
		f = col->sel->frame;

	if (!col) {
		col = cext_emallocz(sizeof(Column));
		col->rect = area_rect;
		acme->columns = acme->sel = col;
		acme->ncolumns++;
	}

	/* check for tabbing? */
	if (f && (((char *) f->file[F_LOCKED]->content)[0] == '1'))
		f = 0;
	if (!f) {
		f = alloc_frame(&c->rect);
		attach_frame(a, col, f);
	}
	attach_client_to_frame(f, c);
	arrange_column(col);
	if (a->page == selpage)
		XMapWindow(dpy, f->win);
	focus_col(f, True);
	return True;
}

static void update_column_width(Area *a)
{
	Acme *acme = a->aux;
	unsigned int i = 0, width;
	Column *col;

	if (!acme->ncolumns)
		return;

	width = area_rect.width / acme->ncolumns;
	for (col = acme->columns; col; col = col->next) {
		col->rect = area_rect;
		col->rect.x = i * width;
		col->rect.width = width;
		i++;
	}
	arrange_col(a);
}

static void detach_col(Area *a, Client *c, Bool unmap)
{
	Acme *acme = a->aux;
	Frame *f = c->frame;
	Column *col = f->aux;

	detach_client_from_frame(c, unmap);
	if (!f->clients) {
		detach_frame(a, f);
		destroy_frame(f);
	}
	else return;
	if (col->cells)
		arrange_column(col);
	else {
		if (acme->sel == col) {
			if (col->prev)
				acme->sel = col->prev;
			else
				acme->sel = nil;
		}
		if (col->prev)
			col->prev->next = col->next;
		else
			acme->columns = col->next;
		if (col->next)
			col->next->prev = col->prev;
		if (!acme->sel)
			acme->sel = acme->columns;
		acme->ncolumns--;
		free(col);
		update_column_width(a);
	}
}

static void match_frame_horiz(Column *col, XRectangle *r)
{
	Cell *cell;
	for (cell = col->cells; cell; cell = cell->next) {
		Frame *f = cell->frame;
		f->rect.x = r->x;
		f->rect.width = r->width;
		resize_frame(f, &f->rect, nil);
	}
}

static void drop_resize(Frame *f, XRectangle *new)
{
	Column *west = 0, *east = 0, *col = f->aux;
	Cell *north = 0, *south = 0;
	Cell *cell = f->aux;

	west = col->prev;
	east = col->next;
	north = cell->prev;
	south = cell->next;

	/* horizontal resize */
	if (west && (new->x != f->rect.x)) {
		west->rect.width = new->x - west->rect.x;
		col->rect.width += f->rect.x - new->x;
		col->rect.x = new->x;
		match_frame_horiz(west, &west->rect);
		match_frame_horiz(col, &col->rect);
	}
	if (east && (new->x + new->width != f->rect.x + f->rect.width)) {
		east->rect.width -= new->x + new->width - east->rect.x;
		east->rect.x = new->x + new->width;
		col->rect.x = new->x;
		col->rect.width = new->width;
		match_frame_horiz(col, &col->rect);
		match_frame_horiz(east, &east->rect);
	}

	/* vertical resize */
	if (north && (new->y != f->rect.y)) {
		north->frame->rect.height = new->y - north->frame->rect.y;
		f->rect.height += f->rect.y - new->y;
		f->rect.y = new->y;
		resize_frame(north->frame, &north->frame->rect, nil);
		resize_frame(f, &f->rect, nil);
	}
	if (south && (new->y + new->height != f->rect.y + f->rect.height)) {
		south->frame->rect.height -= new->y + new->height - south->frame->rect.y;
		south->frame->rect.y = new->y + new->height;
		f->rect.y = new->y;
		f->rect.height = new->height;
		resize_frame(f, &f->rect, nil);
		resize_frame(south->frame, &south->frame->rect, nil);
	}
}

static void drop_moving(Frame *f, XRectangle *new, XPoint *pt)
{
	Area *a = f->area;
	Cell *cell = f->aux;
	Column *src = cell->col, *tgt = 0;
	Acme *acme = a->aux;

	if (!pt)
		return;

	for (tgt = acme->columns; tgt && !blitz_ispointinrect(pt->x, pt->y, &tgt->rect); tgt = tgt->next);
	if (tgt) {
		if (tgt != src) {
		   	if (src->ncells < 2)
				return;
			detach_frame(a, f);
			attach_frame(a, tgt, f);
			arrange_column(src);
			arrange_column(tgt);
			focus_col(f, True);
		}
		else {
			Cell *c;
		    for (c = src->cells; c && !blitz_ispointinrect(pt->x, pt->y, &c->frame->rect); c = c->next);
			if (c && c != cell) {
				Frame *tmp = c->frame;
				c->frame = f;
				cell->frame = tmp;
				arrange_column(src);
				focus_col(f, True);
			}
		}
	}
}

static void resize_col(Frame *f, XRectangle *new, XPoint *pt)
{
	if ((f->rect.width == new->width)
		&& (f->rect.height == new->height))
		drop_moving(f, new, pt);
	else
		drop_resize(f, new);
}

static void focus_col(Frame *f, Bool raise)
{
	Area *a = f->area;
	Acme *acme = a->aux;
	Cell *old, *cell = f->aux;
	Column *col = cell->col;

	old = col->sel;
	acme->sel = col;
	col->sel = cell;
	sel_client(f->sel);
	a->file[A_SEL_FRAME]->content = f->file[F_PREFIX]->content;
	if (raise)
		center_pointer(f);
	if (old && old != cell)
		draw_frame(old->frame);
	draw_frame(f);
}

static Frame *frames_col(Area *a)
{
	Acme *acme = a->aux;
	return acme->frames;
}

static Frame *sel_col(Area *a) {
	Acme *acme = a->aux;
	Column *col = acme->sel;

	if (col && col->sel)
		return col->sel->frame;
	return nil;
}

static Action *actions_col(Area *a)
{
	return lcol_acttbl;
}	

static void select_frame(void *obj, char *arg)
{
	Area *a = obj;
	Acme *acme = a->aux;
	Column *col = acme->sel;
	Cell *cell;

	if (!col)
		return;

	cell = col->sel;
	if (!cell || !arg)
		return;
	if (!strncmp(arg, "prev", 5))
		cell = cell->prev;
	else if (!strncmp(arg, "next", 5))
		cell = cell->next;
	else if (!strncmp(arg, "west", 5))
		cell = col->prev ? col->prev->sel : nil;
	else if (!strncmp(arg, "east", 5))
		cell = col->next ? col->next->sel : nil;
	else {
		unsigned int i = 0, idx = blitz_strtonum(arg, 0, col->ncells - 1);
		for (cell = col->cells; cell && i != idx; cell = cell->next) i++;
	}
	if (cell)
		focus_col(cell->frame, True);
}

static void swap_frame(void *obj, char *arg)
{
	Area *a = obj;
	Acme *acme = a->aux;
	Column *west = 0, *east = 0, *col = acme->sel;
	Cell *north = 0, *south = 0, *cell = col->sel;
	Frame *f;
	XRectangle r;

	if (!col || !cell || !arg)
		return;

	west = col->prev;
	east = col->next;
	north = cell->prev;
	south = cell->next;

	if (!strncmp(arg, "north", 6)  && north) {
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
		focus_col(cell->frame, True);
	}
	else if (!strncmp(arg, "south", 6) && south) {
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
		focus_col(cell->frame, True);
	}
	else if (!strncmp(arg, "west", 5) && west && (col->ncells > 1)) {
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
		focus_col(cell->frame, True);
	}
	else if (!strncmp(arg, "east", 5) && east && (col->ncells > 1)) {
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
		focus_col(cell->frame, True);
	}
}

static void new_col(void *obj, char *arg)
{
	Area *a = obj;
	Acme *acme = a->aux;
	Column *col = acme->sel;
	Cell *cell;

	if (!col || col->ncells < 2)
		return;

	cell = col->sel;
	f = cext_stack_get_top_item(&col->frames);
	cext_detach_item(&col->frames, f);
	f->aux = col = cext_emallocz(sizeof(Column));
	cext_attach_item(&col->frames, f);
	cext_attach_item(&acme->columns, col);
	update_column_width(a);
	focus_col(f, True);
}

