/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wm.h"
#include "layoutdef.h"

static void init_float(Area *a);
static void deinit_float(Area *a);
static void arrange_float(Area *a);
static Bool attach_float(Area *a, Client *c);
static void detach_float(Area *a, Client *c);
static void resize_float(Frame *f, XRectangle *new, XPoint *pt);
static Container *get_frames_float(Area *a);

static Layout lfloat = { "float", init_float, deinit_float, arrange_float, attach_float,
						 detach_float, resize_float, get_frames_float };

void init_layout_float()
{
	cext_attach_item(&layouts, &lfloat);
}

static void arrange_float(Area *a)
{
}

static void iter_attach_float(void *client, void *area)
{
	attach_float(area, client);
}

static void init_float(Area *a)
{
	Container *c = cext_emallocz(sizeof(Container));
	a->aux = c;
	cext_iterate(&a->clients, a, iter_attach_float);
}

static void iter_detach_float(void *client, void *area)
{
	detach_float(area, client);
}

static void deinit_float(Area *a)
{
	cext_iterate(&a->clients, a, iter_detach_float);
	free(a->aux);
	a->aux = nil;
}

static Bool attach_float(Area *a, Client *c)
{
	Frame *f = get_sel_frame_of_area(a);
	fprintf(stderr, "attach_float() frame=0x%x\n", f);
	cext_attach_item(&a->clients, c);
	/* check for tabbing? */
	if (f && (((char *) f->file[F_LOCKED]->content)[0] == '1'))
		f = 0;
	if (!f) {
		f = alloc_frame(&c->rect);
		attach_frame_to_area(a, f);
		cext_attach_item((Container *)a->aux, f);
	}
	attach_client_to_frame(f, c);
	if (a->page == get_sel_page())
		XMapRaised(dpy, f->win);
	draw_frame(f, nil);
	return True;
}

static void detach_float(Area *a, Client *c)
{
	Frame *f = c->frame;
	detach_client_from_frame(c);
	cext_detach_item(&a->clients, c);
	if (!cext_sizeof(&f->clients)) {
		detach_frame_from_area(f);
		cext_detach_item((Container *)a->aux, f);
		destroy_frame(f);
	}
}

static void resize_float(Frame *f, XRectangle *new, XPoint *pt)
{
	f->rect = *new;
}

static Container *get_frames_float(Area *a) {
	return a->aux;
}
