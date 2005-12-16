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
typedef struct ColFrame ColFrame;

struct Acme {
	Column *sel;
	Column *columns;
	size_t ncolumns;
	Frame *frames;
	size_t nframes;
};

struct ColFrame {
	Frame *frame;
	ColFrame *next;
	ColFrame *prev;
	Column *col;
};

struct Column {
	ColFrame *sel;
	ColFrame *frames;
	size_t nframes;
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
	ColFrame *cf;
	unsigned int i = 0, h = area_rect.height / col->nframes;
	for (cf = col->frames; cf; cf = cf->next) {
		Frame *f = cf->frame;
		f->rect = col->rect;
		f->rect.y = area_rect.y + i * h;
		if (!cf->next)
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
	ColFrame *c, *cf = cext_emallocz(sizeof(ColFrame));
	Frame *fr;
	
	cf->frame = f;
	cf->col = col;
	f->aux = cf;
	for (c = col->frames; c && c->next; c = c->next);
	if (!c) 
		col->frames = cf;
	else {
		c->next = cf;
		cf->prev = c;
	}
	col->sel = cf;
	col->nframes++;

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
	ColFrame *cf = f->aux;
	Column *col = cf->col;

	if (col->sel == cf) {
		if (cf->prev)
			col->sel = cf->prev;
		else
			col->sel = nil;
	}
	if (cf->prev)
		cf->prev->next = cf->next;
	else
		col->frames = cf->next;
	if (cf->next)
		cf->next->prev = cf->prev;
	if (!col->sel)
		col->sel = col->frames;
	free(cf);
	col->nframes--;

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
	if (col->frames)
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
	ColFrame *cf;
	for (cf = col->frames; cf; cf = cf->next) {
		Frame *f = cf->frame;
		f->rect.x = r->x;
		f->rect.width = r->width;
		resize_frame(f, &f->rect, nil);
	}
}

static void drop_resize(Frame *f, XRectangle *new)
{
	Column *west = 0, *east = 0, *col = f->aux;
	ColFrame *north = 0, *south = 0;
	ColFrame *cf = f->aux;

	west = col->prev;
	east = col->next;
	north = cf->prev;
	south = cf->next;

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
	ColFrame *fcf = f->aux;
	Column *src = fcf->col, *tgt = 0;
	Acme *acme = a->aux;

	if (!pt)
		return;

	for (tgt = acme->columns; tgt && !blitz_ispointinrect(pt->x, pt->y, &tgt->rect); tgt = tgt->next);
	if (tgt) {
		if (tgt != src) {
		   	if (src->nframes < 2)
				return;
			detach_frame(a, f);
			attach_frame(a, tgt, f);
			arrange_column(src);
			arrange_column(tgt);
			focus_col(f, True);
		}
		else {
			ColFrame *cf;
		    for (cf = src->frames; cf && !blitz_ispointinrect(pt->x, pt->y, &cf->frame->rect); cf = cf->next);
			if (cf && cf != fcf) {
				Frame *tmp = cf->frame;
				cf->frame = f;
				fcf->frame = tmp;
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
	ColFrame *old, *fcf = f->aux;
	Column *col = fcf->col;

	old = col->sel;
	acme->sel = col;
	col->sel = fcf;
	sel_client(f->sel);
	a->file[A_SEL_FRAME]->content = f->file[F_PREFIX]->content;
	if (raise)
		center_pointer(f);
	if (old && old != fcf)
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
	Column *col = get_sel_column(acme);
	Frame *f;

	if (!col)
		return;

	f = cext_stack_get_top_item(&col->frames);
	if (!f || !arg)
		return;
	if (!strncmp(arg, "prev", 5))
		f = cext_list_get_prev_item(&acme->frames, f);
	else if (!strncmp(arg, "next", 5))
		f = cext_list_get_next_item(&acme->frames, f);
	else if (!strncmp(arg, "north", 6))
		f = cext_list_get_next_item(&col->frames, f);
	else if (!strncmp(arg, "south", 6))
		f = cext_list_get_next_item(&col->frames, f);
	else if (!strncmp(arg, "west", 5)) {
		col = cext_list_get_prev_item(&acme->columns, col);
		cext_stack_top_item(&acme->columns, col);
		f = cext_stack_get_top_item(&col->frames);
	}
	else if (!strncmp(arg, "east", 5)) {
		col = cext_list_get_next_item(&acme->columns, col);
		cext_stack_top_item(&acme->columns, col);
		f = cext_stack_get_top_item(&col->frames);
	}
	else 
		f = cext_list_get_item(&col->frames, blitz_strtonum(arg, 0, cext_sizeof_container(&col->frames) - 1));
	focus_col(f, True);
}

static void swap_frame(void *obj, char *arg)
{
	Area *a = obj;
	Acme *acme = a->aux;
	Column *west = 0, *east = 0, *col = get_sel_column(acme);
	Frame *north = 0, *south = 0, *f;
	XRectangle r;
	size_t ncol, nfr;
	int colidx, fidx;

	if (!col)
		return;

	f = cext_stack_get_top_item(&col->frames);

	if (!f || !arg)
		return;

	ncol = cext_sizeof_container(&acme->columns);
	nfr = cext_sizeof_container(&col->frames);
	colidx = cext_list_get_item_index(&acme->columns, col);
	fidx = cext_list_get_item_index(&col->frames, f);

	if (colidx > 0)
		west = cext_list_get_item(&acme->columns, colidx - 1);
	if (colidx + 1 < ncol)
		east = cext_list_get_item(&acme->columns, colidx + 1);
	if (fidx > 0)
		north = cext_list_get_item(&col->frames, fidx - 1);
	if (fidx + 1 < nfr)
		south = cext_list_get_item(&col->frames, fidx + 1);

	if (!strncmp(arg, "north", 6)  && north) {
		r = north->rect;
		north->rect = f->rect;
		f->rect = r;
		cext_swap_items(&col->frames, f, north);
		resize_frame(f, &f->rect, nil);
		resize_frame(north, &north->rect, nil);
		focus_col(f, True);
	}
	else if (!strncmp(arg, "south", 6) && south) {
		r = south->rect;
		south->rect = f->rect;
		f->rect = r;
		cext_swap_items(&col->frames, f, south);
		resize_frame(f, &f->rect, nil);
		resize_frame(south, &south->rect, nil);
		focus_col(f, True);
	}
	else if (!strncmp(arg, "west", 5) && west && (ncol > 1)) {
		Frame *other = cext_stack_get_top_item(&west->frames);
		r = other->rect;
		other->rect = f->rect;
		f->rect = r;
        cext_detach_item(&col->frames, f);
        cext_detach_item(&west->frames, other);
		cext_attach_item(&west->frames, f);
		cext_attach_item(&col->frames, other);
		f->aux = west;
		other->aux = col;
		resize_frame(f, &f->rect, nil);
		resize_frame(other, &other->rect, nil);
		focus_col(f, True);
	}
	else if (!strncmp(arg, "east", 5) && east && (ncol > 1)) {
		Frame *other = cext_stack_get_top_item(&east->frames);
		r = other->rect;
		other->rect = f->rect;
		f->rect = r;
        cext_detach_item(&col->frames, f);
        cext_detach_item(&east->frames, other);
		cext_attach_item(&east->frames, f);
		cext_attach_item(&col->frames, other);
		f->aux = east;
		other->aux = col;
		resize_frame(f, &f->rect, nil);
		resize_frame(other, &other->rect, nil);
		focus_col(f, True);
	}
}

static void new_col(void *obj, char *arg)
{
	Area *a = obj;
	Acme *acme = a->aux;
	Column *col = get_sel_column(acme);
	Frame *f;

	if (!col || (cext_sizeof_container(&col->frames) < 2))
		return;

	f = cext_stack_get_top_item(&col->frames);
	cext_detach_item(&col->frames, f);
	f->aux = col = cext_emallocz(sizeof(Column));
	cext_attach_item(&col->frames, f);
	cext_attach_item(&acme->columns, col);
	update_column_width(a);
	focus_col(f, True);
}

