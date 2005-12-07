/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wm.h"
#include "layout.h"

static void init_float(Area *a);
static void deinit_float(Area *a);
static void arrange_float(Area *a);
static void attach_float(Area *a, Client *c);
static void detach_float(Area *a, Client *c);
static void resize_float(Frame *f, XRectangle *new, XPoint *pt);

static Layout lfloat = { "float", init_float, deinit_float, arrange_float, attach_float, detach_float, resize_float };

void init_layout_float()
{
	cext_attach_item(&layouts, &lfloat);
}


static void arrange_float(Area *a)
{
}

static void init_float(Area *a)
{
}

static void deinit_float(Area *a)
{
}

static void attach_float(Area *a, Client *c)
{
	Frame *f = get_sel_frame_of_area(a);
	/* check for tabbing? */
	if (f && (((char *) f->file[F_LOCKED]->content)[0] == '1'))
		f = 0;
	if (!f) {
		f = alloc_frame(&c->rect);
		attach_frame_to_area(a, f);
	}
	attach_client_to_frame(f, c);
	if (a->page == get_sel_page())
		XMapRaised(dpy, f->win);
	draw_frame(f, nil);
}

static void detach_float(Area *a, Client *c)
{
	Frame *f = c->frame;
	detach_client_from_frame(c);
	if (!cext_sizeof(&f->clients)) {
		detach_frame_from_area(f, 0);
		destroy_frame(f);
	}
}

static void resize_float(Frame *f, XRectangle *new, XPoint *pt)
{
	f->rect = *new;
}
