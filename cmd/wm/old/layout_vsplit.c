/*
 * (C)opyright 2005 Jonas WUSTRACK <doez at supinfo dot com>
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wm.h"
#include "layout.h"

#include <cext.h>

static void     arrange_vsplit(Page * p);
static void     init_vsplit(Page * p);
static void     deinit_vsplit(Page * p);
static void     manage_vsplit(Frame * f);
static void     unmanage_vsplit(Frame * f);
static void     resize_vsplit(Frame * f, XRectangle * new, XPoint * pt);
static Frame   *select_vsplit(Frame * f, char *what);

static Layout   vsplit =
{"vsplit", init_vsplit, deinit_vsplit, arrange_vsplit, manage_vsplit,
	unmanage_vsplit, resize_vsplit, select_vsplit
};

void 
init_layout_vsplit()
{
	layouts =
	(Layout **) attach_item_end((void **) layouts, &vsplit,
				    sizeof(Layout *));
}

static void
get_base_geometry_vsplit(void **items, unsigned int *size,
			 unsigned int *cols, unsigned int *rows)
{
	/* float sq, dummy; */

	*size = count_items((void **) items);
	*cols = 1;
	*rows = *size;
}


static void 
arrange_vsplit(Page * p)
{
	unsigned int    i, ic, tw, th, rows, cols;

	if (!p->managed)
		return;

	get_base_geometry_vsplit((void **) p->managed, &ic, &cols, &rows);
	th = p->managed_rect.height / rows;
	tw = p->managed_rect.width;

	for (i = 0; i < rows; i++) {
		if (p->managed[i]) {
			XRectangle     *r = (XRectangle *) p->managed[i]->aux;
			r->x = p->managed_rect.x;
			r->y = p->managed_rect.y + i * th;
			r->width = tw;
			r->height = th;
			p->managed[i]->managed_rect = *r;
			resize_frame(p->managed[i], &p->managed[i]->managed_rect, 0,
				     1);
		}
	}
}

static void 
init_vsplit(Page * p)
{
	int             i;
	for (i = 0; p->managed && p->managed[i]; i++)
		p->managed[i]->aux = emalloc(sizeof(XRectangle));
}

static void 
deinit_vsplit(Page * p)
{
	int             i;
	for (i = 0; p->managed && p->managed[i]; i++) {
		if (p->managed[i]->aux) {
			free(p->managed[i]->aux);
			p->managed[i]->aux = 0;
		}
	}
}

static void 
manage_vsplit(Frame * f)
{
	f->aux = emalloc(sizeof(XRectangle));
	if (f->page)
		arrange_vsplit(f->page);
}

static void 
unmanage_vsplit(Frame * f)
{
	if (f->aux) {
		free(f->aux);
		f->aux = 0;
	}
	if (f->page)
		arrange_vsplit(f->page);
}

#define THRESHOLD 30

static void 
drop_resize(Frame * f, XRectangle * new)
{
	int             diff;
	unsigned int    i, rows, cols, num, idx;
	Page           *p = f->page;
	XRectangle     *r;

	if (!p || !p->managed)
		return;
	get_base_geometry_vsplit((void **) p->managed, &i, &cols, &rows);

	num = index_item((void **) p->managed, f);

	/* vertical resize */
	if (f->managed_rect.y == new->y
	    && f->managed_rect.height != new->height) {
		/* south direction resize */
		if (num == rows - 1)
			return;
		if (p->managed[num + 1]
		    && (new->y + new->height > p->managed[num + 1]->managed_rect.y
		    + p->managed[num + 1]->managed_rect.height - THRESHOLD))
			return;
		diff = new->height - ((XRectangle *) f->aux)->height;
		idx = num;
		if (p->managed[idx]) {
			r = (XRectangle *) p->managed[idx]->aux;
			r->height = new->height;
			p->managed[idx]->managed_rect = *r;
			resize_frame(p->managed[idx], &p->managed[idx]->managed_rect,
				     0, 1);
		}
		idx = num + 1;
		if (p->managed[idx]) {
			r = (XRectangle *) p->managed[idx]->aux;
			r->y += diff;
			r->height -= diff;
			p->managed[idx]->managed_rect = *r;
			resize_frame(p->managed[idx], &p->managed[idx]->managed_rect,
				     0, 1);
		}
	} else if (f->managed_rect.y != new->y) {
		/* north direction resize */
		if (!num)
			return;
		if (new->y < p->managed[num - 1]->managed_rect.y + THRESHOLD)
			return;
		diff = new->height - ((XRectangle *) f->aux)->height;
		idx = num;
		if (p->managed[idx]) {
			r = (XRectangle *) p->managed[idx]->aux;
			r->y -= diff;
			r->height += diff;
			p->managed[idx]->managed_rect = *r;
			resize_frame(p->managed[idx], &p->managed[idx]->managed_rect,
				     0, 1);
		}
		idx = num - 1;
		if (p->managed[idx]) {
			r = (XRectangle *) p->managed[idx]->aux;
			r->height -= diff;
			p->managed[idx]->managed_rect = *r;
			resize_frame(p->managed[idx], &p->managed[idx]->managed_rect,
				     0, 1);
		}
	}
}

static void 
resize_vsplit(Frame * f, XRectangle * new, XPoint * pt)
{
	if ((f->managed_rect.width == new->width)
	    && (f->managed_rect.height == new->height))
		drop_move(f, new, pt);
	else
		drop_resize(f, new);
}

static Frame   *
select_vsplit(Frame * f, char *what)
{
	Page           *p = f->page;
	int             idx;
	if (!strncmp(what, "prev", 5)) {
		idx = index_prev_item((void **) p->managed, f);
		if (idx >= 0)
			return p->managed[idx];
	} else if (!strncmp(what, "next", 5)) {
		idx = index_next_item((void **) p->managed, f);
		if (idx >= 0)
			return p->managed[idx];
	} else if (!strncmp(what, "zoomed", 7)) {
		idx = index_item((void **) p->managed, f);
		if (idx == 0 && p->managed[1])
			idx = 1;
		if (idx > 0)
			swap((void **) &p->managed[0], (void **) &p->managed[idx]);
		p->managed_stack = (Frame **)
			attach_item_begin(detach_item
				 ((void **) p->managed_stack, p->managed[0],
				  sizeof(Frame *)), p->managed[0],
					  sizeof(Frame *));
		arrange_vsplit(p);
		return p->managed_stack[0];
	}
	return 0;
}
