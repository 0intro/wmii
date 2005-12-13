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
static void detach_float(Area *a, Client *c, Bool unmap);
static void resize_float(Frame *f, XRectangle *new, XPoint *pt);
static void select_float(Frame *f, Bool raise);
static Container *get_frames_float(Area *a);
static Action *get_actions_float(Area *a);

static void select_frame(void *obj, char *arg);

Action lfloat_acttbl[] = {
	{"select", select_frame},
	{0, 0}
};

static Layout lfloat = { "float", init_float, deinit_float, arrange_float, attach_float,
						 detach_float, resize_float, select_float, get_frames_float, get_actions_float };

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
	cext_list_iterate(&a->clients, a, iter_attach_float);
}

static void iter_detach_float(void *client, void *area)
{
	Area *a = area;
	detach_float(a, client, a->page != get_sel_page());
}

static void deinit_float(Area *a)
{
	cext_list_iterate(&a->clients, a, iter_detach_float);
	free(a->aux);
	a->aux = nil;
}

static Bool attach_float(Area *a, Client *c)
{
	Frame *f = get_sel_frame_of_area(a);

	/* check for tabbing? */
	if (f && (((char *) f->file[F_LOCKED]->content)[0] == '1'))
		f = 0;
	if (!f) {
		if (c->rect.y < area_rect.y)
			c->rect.y = area_rect.y;
		if (c->rect.x < area_rect.x)
			c->rect.x = area_rect.x;

		f = alloc_frame(&c->rect);
		attach_frame_to_area(a, f);
		cext_attach_item((Container *)a->aux, f);
	}
	attach_client_to_frame(f, c);
	if (a->page == get_sel_page())
		XMapWindow(dpy, f->win);
	select_float(f, True);
	draw_frame(f, nil);
	return True;
}

static void detach_float(Area *a, Client *c, Bool unmap)
{
	Frame *f = c->frame;
	detach_client_from_frame(c, unmap);
	if (!cext_sizeof_container(&f->clients)) {
		detach_frame_from_area(f);
		cext_detach_item((Container *)a->aux, f);
		destroy_frame(f);
	}
}

static void resize_float(Frame *f, XRectangle *new, XPoint *pt)
{
	f->rect = *new;
}

static void select_float(Frame *f, Bool raise)
{
	Area *a = f->area;
	sel_client(cext_stack_get_top_item(&f->clients));
	cext_stack_top_item(a->aux, f);
	a->file[A_SEL_FRAME]->content = f->file[F_PREFIX]->content;
	if (raise) {
		XRaiseWindow(dpy, f->win);
		center_pointer(f);
	}
}

static Container *get_frames_float(Area *a)
{
	return a->aux;
}

static void select_frame(void *obj, char *arg)
{
	Area *a = obj;
	Container *c = a->aux;
	Frame *f, *old;

	f = old = cext_stack_get_top_item(c);
	if (!f || !arg)
		return;
	if (!strncmp(arg, "prev", 5))
		f = cext_list_get_prev_item(c, f);
	else if (!strncmp(arg, "next", 5))
		f = cext_list_get_next_item(c, f);
	else 
		f = cext_list_get_item(c, blitz_strtonum(arg, 0, cext_sizeof_container(c) - 1));
	if (old != f) {
		select_float(f, True);
		center_pointer(f);
		draw_frame(old, nil);
		draw_frame(f, nil);
	}
}

static Action *get_actions_float(Area *a)
{
	return lfloat_acttbl;
}
