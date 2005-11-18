/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wm.h"
#include "layout.h"

static void     arrange_max(Page * p);
static void     init_max(Page * p);
static void     deinit_max(Page * p);
static void     manage_max(Frame * f);
static void     unmanage_max(Frame * f);
static void     resize_max(Frame * f, XRectangle * new, XPoint * pt);
static Frame   *select_max(Frame * f, char *what);

static Layout   max = {"max", init_max, deinit_max, arrange_max, manage_max,
	unmanage_max, resize_max, select_max
};

void 
init_layout_max()
{
	layouts =
	(Layout **) attach_item_end((void **) layouts, &max,
				    sizeof(Layout *));
}


static void 
arrange_max(Page * p)
{
	int             i = 0;
	if (!p->managed)
		return;

	for (i = 0; p->managed[i]; i++) {
		p->managed[i]->managed_rect = p->managed_rect;
		resize_frame(p->managed[i], &p->managed[i]->managed_rect, 0, 1);
	}
	if (p->managed_stack)
		XRaiseWindow(dpy, p->managed_stack[0]->win);

	/* raise floatings */
	for (i = 0; p->floating && p->floating[i]; i++)
		XRaiseWindow(dpy, p->floating[i]->win);
}

static void 
init_max(Page * p)
{
	/* max has nothing to init */
	arrange_max(p);
}

static void 
deinit_max(Page * p)
{
	/* max has nothing to free */
}

static void 
manage_max(Frame * f)
{
	Page           *p = f->page;
	int             idx;
	if (!p)
		return;
	idx = index_next_item((void **) p->managed, f);
	if (idx > 0)
		swap((void **) &p->managed[0], (void **) &p->managed[idx]);
	arrange_max(f->page);
}

static void 
unmanage_max(Frame * f)
{
	/* nothing todo */
}

static void 
resize_max(Frame * f, XRectangle * new, XPoint * pt)
{
	if (f->page)
		f->managed_rect = f->page->managed_rect;
}

static Frame   *
select_max(Frame * f, char *what)
{
	Page           *p = f->page;
	Frame          *res = 0;
	int             i, idx;

	if (!strncmp(what, "prev", 5)) {
		idx = index_prev_item((void **) p->managed, f);
		if (idx >= 0)
			res = p->managed[idx];
	} else if (!strncmp(what, "next", 5)) {
		idx = index_next_item((void **) p->managed, f);
		if (idx >= 0)
			res = p->managed[idx];
	}
	if (res)
		XRaiseWindow(dpy, res->win);

	/* raise floatings */
	for (i = 0; p->floating && p->floating[i]; i++)
		XRaiseWindow(dpy, p->floating[i]->win);
	return res;
}
