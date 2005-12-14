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

struct Acme {
	Container columns;
	Container frames;
};

struct Column {
	Container frames;
	XRectangle rect;
};

static void init_col(Area * a);
static void deinit_col(Area * a);
static void arrange_col(Area * a);
static Bool attach_col(Area * a, Client * c);
static void detach_col(Area * a, Client * c, Bool unmap);
static void resize_col(Frame *f, XRectangle * new, XPoint * pt);
static void select_col(Frame *f, Bool raise);
static Container *get_frames_col(Area *a);
static Action *get_actions_col(Area *a);

static void select_frame(void *obj, char *arg);
static void swap_frame(void *obj, char *arg);
static void new_col(void *obj, char *arg);

static Action lcol_acttbl[] = {
	{"select", select_frame},
	{"swap", swap_frame},
	{"new", new_col},
	{0, 0}
};

static Layout lcol = { "col", init_col, deinit_col, arrange_col, attach_col, detach_col,
   				       resize_col, select_col, get_frames_col, get_actions_col };

void init_layout_column()
{
	cext_attach_item(&layouts, &lcol);
}

static Column *get_sel_column(Acme *acme)
{
	return cext_stack_get_top_item(&acme->columns);
}

static void iter_arrange_column_frame(void *frame, void *height)
{
	Frame *f = frame;
	Column *col = f->aux;
	size_t size = cext_sizeof_container(&col->frames);
	unsigned int h = *(unsigned int *)height;
	int idx = cext_list_get_item_index(&col->frames, f) ;

	f->rect = col->rect;
	f->rect.y = area_rect.y + idx * h;
	if (idx + 1 == size)
		f->rect.height = area_rect.height - f->rect.y + area_rect.y;
	else
		f->rect.height = h;

	resize_frame(f, &f->rect, 0);
}

static void iter_arrange_column(void *column, void *area)
{
	Column *col = column;
	size_t size = cext_sizeof_container(&col->frames);
	unsigned int height;
   
	if (size) {
		height= area_rect.height / size;
		cext_list_iterate(&col->frames, &height, iter_arrange_column_frame);
	}
}

static void arrange_col(Area *a)
{
	Acme *acme = a->aux;
	cext_list_iterate(&acme->columns, a, iter_arrange_column);
	XSync(dpy, False);
}

static void iter_attach_col(void *client, void *area)
{
	attach_col(area, client);
}

static void init_col(Area *a)
{
	Acme *acme = cext_emallocz(sizeof(Acme));
	a->aux = acme;
	cext_list_iterate(&a->clients, a, iter_attach_col);
}

static void iter_detach_client(void *client, void *area)
{
	Area *a = area;
	detach_col(a, (Client *)client, a->page != get_sel_page());
}

static void deinit_col(Area *a)
{
	Acme *acme = a->aux;
	Column *col;
	cext_list_iterate(&a->clients, a, iter_detach_client);
	while ((col = get_sel_column(acme)))
	{
		cext_detach_item(&acme->columns, col);
		free(col);
	}
	free(acme);
	a->aux = 0;
}

static Bool attach_col(Area *a, Client *c)
{
	Acme *acme = a->aux;
	Column *col = get_sel_column(acme);
	Frame *f = get_sel_frame_of_area(a);

	if (!col) {
		col = cext_emallocz(sizeof(Column));
		col->rect = area_rect;
		cext_attach_item(&acme->columns, col);
	}

	/* check for tabbing? */
	if (f && (((char *) f->file[F_LOCKED]->content)[0] == '1'))
		f = 0;
	if (!f) {
		f = alloc_frame(&c->rect);
		attach_frame_to_area(a, f);
		f->aux = col;
		cext_attach_item(&acme->frames, f);
		cext_attach_item(&col->frames, f);
	}
	attach_client_to_frame(f, c);
	iter_arrange_column(col, a);
	if (a->page == get_sel_page())
		XMapWindow(dpy, f->win);
	select_col(f, True);
	return True;
}

static void update_column_width(Area *a)
{
	Acme *acme = a->aux;
	size_t size = cext_sizeof_container(&acme->columns);
	unsigned int i, width;

	if (!size)
		return;

	width = area_rect.width / size;
	for (i = 0; i < size; i++) {
		Column *col = cext_list_get_item(&acme->columns, i);
		col->rect = area_rect;
		col->rect.x = i * width;
		col->rect.width = width;
	}
	arrange_col(a);
}

static void detach_col(Area *a, Client *c, Bool unmap)
{
	Acme *acme = a->aux;
	Frame *f = c->frame;
	Column *col = f->aux;

	detach_client_from_frame(c, unmap);
	if (!cext_sizeof_container(&f->clients)) {
		detach_frame_from_area(f);
		cext_detach_item(&acme->frames, f);
		cext_detach_item(&col->frames, f);
		destroy_frame(f);
	}
	if (cext_sizeof_container(&col->frames))
		iter_arrange_column(col, a);
	else {
		cext_detach_item(&acme->columns, col);
		free(col);
		update_column_width(a);
	}
}

static void iter_match_frame_horiz(void *frame, void *rect)
{
	Frame *f = frame;
	XRectangle *r = rect;
	f->rect.x = r->x;
	f->rect.width = r->width;
	resize_frame(f, &f->rect, nil);
}

static void drop_resize(Frame *f, XRectangle *new)
{
	Column *west = 0, *east = 0, *col = f->aux;
	Frame *north = 0, *south = 0;
	Acme *acme = f->area->aux;
	size_t ncol = cext_sizeof_container(&acme->columns);
	size_t nfr = cext_sizeof_container(&col->frames);
	int colidx = cext_list_get_item_index(&acme->columns, col);
	int fidx = cext_list_get_item_index(&col->frames, f);

	if (colidx > 0)
		west = cext_list_get_item(&acme->columns, colidx - 1);
	if (colidx + 1 < ncol)
		east = cext_list_get_item(&acme->columns, colidx + 1);
	if (fidx > 0)
		north = cext_list_get_item(&col->frames, fidx - 1);
	if (fidx + 1 < nfr)
		south = cext_list_get_item(&col->frames, fidx + 1);

	/* horizontal resize */
	if (west && (new->x != f->rect.x)) {
		west->rect.width = new->x - west->rect.x;
		col->rect.width += f->rect.x - new->x;
		col->rect.x = new->x;
		cext_list_iterate(&west->frames, &west->rect, iter_match_frame_horiz);
		cext_list_iterate(&col->frames, &col->rect, iter_match_frame_horiz);
	}
	if (east && (new->x + new->width != f->rect.x + f->rect.width)) {
		east->rect.width -= new->x + new->width - east->rect.x;
		east->rect.x = new->x + new->width;
		col->rect.x = new->x;
		col->rect.width = new->width;
		cext_list_iterate(&col->frames, &col->rect, iter_match_frame_horiz);
		cext_list_iterate(&east->frames, &east->rect, iter_match_frame_horiz);
	}

	/* vertical resize */
	if (north && (new->y != f->rect.y)) {
		north->rect.height = new->y - north->rect.y;
		f->rect.height += f->rect.y - new->y;
		f->rect.y = new->y;
		resize_frame(north, &north->rect, nil);
		resize_frame(f, &f->rect, nil);
	}
	if (south && (new->y + new->height > f->rect.y + f->rect.height)) {
		south->rect.height -= new->y + new->height - south->rect.y;
		south->rect.y = new->y + new->height;
		f->rect.y = new->y;
		f->rect.height = new->height;
		resize_frame(f, &f->rect, nil);
		resize_frame(south, &south->rect, nil);
	}
}

static int comp_pointer_frame(void *point, void *frame)
{
	Frame *f = frame;
	XPoint *pt = point;

	fprintf(stderr, "frame: %d,%d in %d,%d,%d,%d ?\n", pt->x, pt->y, f->rect.x, f->rect.y, f->rect.width, f->rect.height);
	return blitz_ispointinrect(pt->x, pt->y, &f->rect);
}

static int comp_pointer_col(void *point, void *column)
{
	Column *col = column;
	XPoint *pt = point;

	fprintf(stderr, "col: %d,%d in %d,%d,%d,%d ?\n", pt->x, pt->y, col->rect.x, col->rect.y, col->rect.width, col->rect.height);
	return blitz_ispointinrect(pt->x, pt->y, &col->rect);
}

static void drop_moving(Frame *f, XRectangle *new, XPoint *pt)
{
	Acme *acme = f->area->aux;
	Column *src = f->aux, *tgt = 0;

	if (!pt)
		return;

	if ((tgt = cext_find_item(&acme->columns, pt, comp_pointer_col))) {
		if (tgt != src) {
		   	if (cext_sizeof_container(&src->frames) < 2)
				return;
			cext_detach_item(&src->frames, f);
			cext_attach_item(&tgt->frames, f);
			f->aux = tgt;
			iter_arrange_column(tgt, f->area);
			iter_arrange_column(src, f->area);
			select_col(f, True);
		}
		else {
			Frame *other = cext_find_item(&tgt->frames, pt, comp_pointer_frame);
			if (other != f) {
				cext_swap_items(&tgt->frames, f, other);
				iter_arrange_column(tgt, f->area);
				select_col(f, True);
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

static void select_col(Frame *f, Bool raise)
{
	Area *a = f->area;
	Acme *acme = a->aux;
	Column *col = f->aux;
	Frame *old = get_sel_frame();

	cext_stack_top_item(&acme->columns, col);
	sel_client(cext_stack_get_top_item(&f->clients));
	cext_stack_top_item(&col->frames, f);
	cext_stack_top_item(&acme->frames, f);
	a->file[A_SEL_FRAME]->content = f->file[F_PREFIX]->content;
	if (raise)
		center_pointer(f);
	if (old != f)
		draw_frame(old);
	draw_frame(f);
}

static Container *get_frames_col(Area *a)
{
	Acme *acme = a->aux;
	return &acme->frames;
}

static Action *get_actions_col(Area *a)
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
		f = cext_list_get_prev_item(&col->frames, f);
	else if (!strncmp(arg, "next", 5))
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
	select_col(f, True);
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
		select_col(f, True);
	}
	else if (!strncmp(arg, "south", 6) && south) {
		r = south->rect;
		south->rect = f->rect;
		f->rect = r;
		cext_swap_items(&col->frames, f, south);
		resize_frame(f, &f->rect, nil);
		resize_frame(south, &south->rect, nil);
		select_col(f, True);
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
		select_col(f, True);
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
		select_col(f, True);
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
	select_col(f, True);
}
