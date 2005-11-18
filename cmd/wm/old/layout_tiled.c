/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"
#include "layout.h"

#include <cext.h>

/* tiled layout definition */
static void     arrange_tiled(Page * p);
static void     init_tiled(Page * p);
static void     deinit_tiled(Page * p);
static void     manage_tiled(Frame * f);
static void     unmanage_tiled(Frame * f);
static void     resize_tiled(Frame * f, XRectangle * new, XPoint * pt);
static Frame   *select_tiled(Frame * f, char *what);

static Layout   tiled =
{"tiled", init_tiled, deinit_tiled, arrange_tiled, manage_tiled,
	unmanage_tiled, resize_tiled, select_tiled
};

void 
init_layout_tiled()
{
	layouts =
	(Layout **) attach_item_end((void **) layouts, &tiled,
				    sizeof(Layout *));
}

static void 
arrange_tiled(Page * p)
{
	int             tw, th, i, size = 0;

	if (!p->managed)
		return;

	/* determing num of frame and size of page */
	size = count_items((void **) p->managed);

	if (size > 1) {
		tw = (p->managed_rect.width * *((int *) p->aux)) / 100;
		th = p->managed_rect.height / (size - 1);
	} else {
		tw = p->managed_rect.width;
		th = p->managed_rect.height;
	}

	/* master tile */
	p->managed[0]->managed_rect.x = p->managed_rect.x;
	p->managed[0]->managed_rect.y = p->managed_rect.y;
	p->managed[0]->managed_rect.width = tw;
	p->managed[0]->managed_rect.height = p->managed_rect.height;
	resize_frame(p->managed[0], &p->managed[0]->managed_rect, 0, 1);

	if (size == 1)
		return;
	for (i = 1; i < size; i++) {
		p->managed[i]->managed_rect.x = p->managed_rect.x + tw;
		p->managed[i]->managed_rect.y = p->managed_rect.y + (i - 1) * th;
		p->managed[i]->managed_rect.width = p->managed_rect.width - tw;
		p->managed[i]->managed_rect.height = th;
		resize_frame(p->managed[i], &p->managed[i]->managed_rect, 0, 1);
	}
}

static void 
init_tiled(Page * p)
{
	p->aux = emalloc(sizeof(int));
	*((int *) p->aux) =
		_strtonum(core_files[CORE_PAGE_TILE_WIDTH]->content, 5, 95);
}

static void 
deinit_tiled(Page * p)
{
	p->aux = 0;
}

static void 
manage_tiled(Frame * f)
{
	Page           *p = f->page;
	if (!p)
		return;
	arrange_tiled(p);
}

static void 
unmanage_tiled(Frame * f)
{
	manage_tiled(f);
}

static void 
drop_resize(Frame * f, XRectangle * new)
{
	Page           *p = f->page;
	int             num = 0;
	int             rearrange = 0;

	if (!p)
		return;
	/* determing num of frame and size of page */
	num = index_item((void **) p->managed, f);

	if (!num) {		/* master tile */
		if ((f->managed_rect.x == new->x)
		    && (f->managed_rect.width != new->width)) {
			f->managed_rect = *new;
			rearrange = 1;
		}
	} else if (f->managed_rect.x != new->x) {
		int             diff = f->managed_rect.width - new->width;
		p->managed[0]->managed_rect.width += diff;
		rearrange = 1;
	}
	if (rearrange) {
		int             tw =
		(p->managed[0]->managed_rect.width * 100) /
		p->managed_rect.width;
		if (tw > 95)
			tw = 95;
		else if (tw < 5)
			tw = 5;
		*((int *) p->aux) = tw;
		arrange_tiled(p);
	}
}

static void 
resize_tiled(Frame * f, XRectangle * new, XPoint * pt)
{
	if ((f->managed_rect.width == new->width)
	    && (f->managed_rect.height == new->height))
		drop_move(f, new, pt);
	else
		drop_resize(f, new);
}

static Frame   *
select_tiled(Frame * f, char *what)
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
		arrange_tiled(p);
		return p->managed_stack[0];
	}
	return 0;
}
