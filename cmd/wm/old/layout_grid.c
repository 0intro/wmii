/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wm.h"
#include "layout.h"

#include <cext.h>

/* grid layout definition */
static void     arrange_grid(Page * p);
static void     init_grid(Page * p);
static void     deinit_grid(Page * p);
static void     manage_grid(Frame * f);
static void     unmanage_grid(Frame * f);
static void     resize_grid(Frame * f, XRectangle * new, XPoint * pt);
static Frame   *select_grid(Frame * f, char *what);

static Layout   grid =
{"grid", init_grid, deinit_grid, arrange_grid, manage_grid,
	unmanage_grid, resize_grid, select_grid
};

void 
init_layout_grid()
{
	layouts =
	(Layout **) attach_item_end((void **) layouts, &grid,
				    sizeof(Layout *));
}

static void 
arrange_grid(Page * p)
{
	unsigned int    i, ic, ir, tw, th, rows, cols;

	if (!p->managed)
		return;

	blitz_getbasegeometry((void **) p->managed, &ic, &cols, &rows);
	th = p->managed_rect.height / rows;
	tw = p->managed_rect.width / cols;

	i = 0;
	for (ir = 0; ir < rows; ir++) {
		for (ic = 0; ic < cols; ic++) {
			if (p->managed[i]) {
				XRectangle     *r = (XRectangle *) p->managed[i]->aux;
				r->x = p->managed_rect.x + ic * tw;
				r->y = p->managed_rect.y + ir * th;
				r->width = tw;
				r->height = th;
				p->managed[i]->managed_rect = *r;
				resize_frame(p->managed[i], &p->managed[i]->managed_rect,
					     0, 1);
			} else
				break;
			i++;
		}
	}
}

static void 
init_grid(Page * p)
{
	int             i;
	for (i = 0; p->managed && p->managed[i]; i++)
		p->managed[i]->aux = emalloc(sizeof(XRectangle));
}

static void 
deinit_grid(Page * p)
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
manage_grid(Frame * f)
{
	f->aux = emalloc(sizeof(XRectangle));
	if (f->page)
		arrange_grid(f->page);
}

static void 
unmanage_grid(Frame * f)
{
	if (f->aux) {
		free(f->aux);
		f->aux = 0;
	}
	if (f->page)
		arrange_grid(f->page);
}

#define THRESHOLD 30

static void 
drop_resize(Frame * f, XRectangle * new)
{
	int             diff;
	unsigned int    i, rows, cols, cr, cc, num, idx;
	Page           *p = f->page;
	XRectangle     *r;

	if (!p || !p->managed)
		return;
	blitz_getbasegeometry((void **) p->managed, &i, &cols, &rows);

	num = index_item((void **) p->managed, f);

	cr = num / cols;
	cc = num - cr * cols;

	/* horizontal resize */
	if (f->managed_rect.x == new->x && f->managed_rect.width != new->width) {
		/* east direction resize */
		if (cc == cols - 1)
			return;
		if (p->managed[cc + 1]
		&& (new->x + new->width > p->managed[cc + 1]->managed_rect.x
		    + p->managed[cc + 1]->managed_rect.width - THRESHOLD))
			return;
		diff = new->width - ((XRectangle *) f->aux)->width;
		for (i = 0; i < rows; i++) {
			idx = (i * cols) + cc;
			if (p->managed[idx]) {
				r = (XRectangle *) p->managed[idx]->aux;
				r->width = new->width;
				p->managed[idx]->managed_rect = *r;
				resize_frame(p->managed[idx],
				      &p->managed[idx]->managed_rect, 0, 1);
			}
			idx = (i * cols) + (cc + 1);
			if (p->managed[idx]) {
				r = (XRectangle *) p->managed[idx]->aux;
				r->x += diff;
				r->width -= diff;
				p->managed[idx]->managed_rect = *r;
				resize_frame(p->managed[idx],
				      &p->managed[idx]->managed_rect, 0, 1);
			}
		}
	}
	if (f->managed_rect.x != new->x) {
		/* west direction resize */
		if (!cc)
			return;
		if (new->x < p->managed[cc - 1]->managed_rect.x + THRESHOLD)
			return;
		diff = new->width - ((XRectangle *) f->aux)->width;
		for (i = 0; i < rows; i++) {
			idx = (i * cols) + cc;
			if (p->managed[idx]) {
				r = (XRectangle *) p->managed[idx]->aux;
				r->x -= diff;
				r->width += diff;
				p->managed[idx]->managed_rect = *r;
				resize_frame(p->managed[idx],
				      &p->managed[idx]->managed_rect, 0, 1);
			}
			idx = (i * cols) + (cc - 1);
			if (p->managed[idx]) {
				r = (XRectangle *) p->managed[idx]->aux;
				r->width -= diff;
				p->managed[idx]->managed_rect = *r;
				resize_frame(p->managed[idx],
				      &p->managed[idx]->managed_rect, 0, 1);
			}
		}
	}
	/* vertical resize */
	if (f->managed_rect.y == new->y
	    && f->managed_rect.height != new->height) {
		/* south direction resize */
		if (cr == rows - 1)
			return;
		if (p->managed[cr + 1]
		    && (new->y + new->height >
			p->managed[(cr + 1) * rows]->managed_rect.y +
			p->managed[(cr + 1) * rows]->managed_rect.height -
			THRESHOLD))
			return;
		diff = new->height - ((XRectangle *) f->aux)->height;
		for (i = 0; i < cols; i++) {
			idx = (cr * rows) + i;
			if (p->managed[idx]) {
				r = (XRectangle *) p->managed[idx]->aux;
				r->height = new->height;
				p->managed[idx]->managed_rect = *r;
				resize_frame(p->managed[idx],
				      &p->managed[idx]->managed_rect, 0, 1);
			}
			idx = (cr + 1) * rows + i;
			if (p->managed[idx]) {
				r = (XRectangle *) p->managed[idx]->aux;
				r->y += diff;
				r->height -= diff;
				p->managed[idx]->managed_rect = *r;
				resize_frame(p->managed[idx],
				      &p->managed[idx]->managed_rect, 0, 1);
			}
		}
	} else if (f->managed_rect.y != new->y) {
		/* north direction resize */
		if (!cr)
			return;
		if (new->y <
		    p->managed[(cr - 1) * rows]->managed_rect.y + THRESHOLD)
			return;
		diff = new->height - ((XRectangle *) f->aux)->height;
		for (i = 0; i < cols; i++) {
			idx = (cr * rows) + i;
			if (p->managed[idx]) {
				r = (XRectangle *) p->managed[idx]->aux;
				r->y -= diff;
				r->height += diff;
				p->managed[idx]->managed_rect = *r;
				resize_frame(p->managed[idx],
				      &p->managed[idx]->managed_rect, 0, 1);
			}
			idx = (cr - 1) * rows + i;
			if (p->managed[idx]) {
				r = (XRectangle *) p->managed[idx]->aux;
				r->height -= diff;
				p->managed[idx]->managed_rect = *r;
				resize_frame(p->managed[idx],
				      &p->managed[idx]->managed_rect, 0, 1);
			}
		}
	}
}

static void 
resize_grid(Frame * f, XRectangle * new, XPoint * pt)
{
	if ((f->managed_rect.width == new->width)
	    && (f->managed_rect.height == new->height))
		drop_move(f, new, pt);
	else
		drop_resize(f, new);
}

static unsigned int 
get_current_frame_position(Frame * f)
{
	Page           *p = f->page;
	unsigned int    i;

	for (i = 0; i < count_items((void **) p->managed); ++i)
		if (p->managed[i] == f)
			return i;
	return -1;
}

static Frame   *
select_grid(Frame * f, char *what)
{
	Page           *p = f->page;
	unsigned int    ic, cols, rows, pos;
	int             idx;

	blitz_getbasegeometry((void **) p->managed, &ic, &cols, &rows);
	pos = get_current_frame_position(f);

	if (!strncmp(what, "prev", 5)) {
		idx = index_prev_item((void **) p->managed, f);
		if (idx >= 0)
			return p->managed[idx];
	} else if (!strncmp(what, "next", 5)) {
		idx = index_next_item((void **) p->managed, f);
		if (idx >= 0)
			return p->managed[idx];
	} else if (!strncmp(what, "south", 6)) {
		if ((pos + cols) <= ic)
			pos += cols;
		else
			pos %= cols;
		return p->managed[pos];
	} else if (!strncmp(what, "north", 6)) {
		if (pos < cols)
			pos = (rows - 1) * cols + pos;
		else
			pos -= cols;
		return p->managed[pos];
	} else if (!strncmp(what, "west", 5)) {
		if (pos % cols == 0) {
			pos += (cols - 1);
			if (pos >= ic)
				pos = ic - 1;
		} else
			pos--;
		return p->managed[pos];
	} else if (!strncmp(what, "east", 5)) {
		if (pos % cols == (cols - 1))
			pos -= (cols - 1);
		else {
			pos++;
			if (pos >= ic)
				pos = ic - 1;
		}
		return p->managed[pos];
	}
	return 0;
}
