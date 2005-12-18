/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wm.h"
#include "layoutdef.h"

static void init_float(Area *a, Client *clients);
static Client *deinit_float(Area *a);
static void arrange_float(Area *a);
static Bool attach_float(Area *a, Client *c);
static void detach_float(Area *a, Client *c, Bool unmap);
static void resize_float(Frame *f, XRectangle *new, XPoint *pt);
static void focus_float(Frame *f, Bool raise);
static Frame *frames_float(Area *a);
static Frame *sel_float(Area *a);
static Action *actions_float(Area *a);

static void select_frame(void *obj, char *arg);

Action lfloat_acttbl[] = {
	{"select", select_frame},
	{0, 0}
};

typedef struct {
	Frame *frames;
	Frame *sel;
	size_t nframes;
} Float;

void init_layout_float()
{
	Layout *lp, *l = cext_emallocz(sizeof(Layout));
	l->name = "float";
	l->init = init_float;
	l->deinit = deinit_float;
	l->arrange = arrange_float;
	l->attach = attach_float;
	l->detach = detach_float;
	l->resize = resize_float;
	l->focus = focus_float;
	l->frames = frames_float;
	l->sel = sel_float;
	l->actions = actions_float;

	for (lp = layouts; lp && lp->next; lp = lp->next);
	if (lp)
		lp->next = l;
	else
		layouts = l;
}

static void arrange_float(Area *a)
{
}

static void init_float(Area *a, Client *clients)
{
	Client *n, *c = clients;
	Float *fl = cext_emallocz(sizeof(Float));
	a->aux = fl;

	while (c) {
		n = c->next;
		attach_float(a, c);
		c = n;
	}
}

static void attach_frame(Area *a, Frame *new)
{
	Float *fl = a->aux;
	Frame *f;

	for (f = fl->frames; f && f->next; f = f->next);
	if (!f) 
		fl->frames = new;
	else {
		f->next = new;
		new->prev = f;
	}
	attach_frame_to_area(a, new);
	fl->sel = new;
	fl->nframes++;
}

static void detach_frame(Area *a, Frame *old)
{
	Float *fl = a->aux;

	if (fl->sel == old) {
		if (old->prev)
			fl->sel = old->prev;
		else
			fl->sel = nil;
	}
	if (old->prev)
		old->prev->next = old->next;
	else
		fl->frames = old->next;
	if (old->next)
		old->next->prev = old->prev;
	if (!fl->sel)
		fl->sel = fl->frames;
	detach_frame_from_area(old);
	fl->nframes--;
}

static Client *deinit_float(Area *a)
{
	Float *fl = a->aux;
	Client *res = nil, *c = nil, *cl;
	Frame *f;

	while ((f = fl->frames)) {
		while ((cl = f->clients)) {
			detach_client_from_frame(cl, False);
			cl->prev = cl->next = 0;
			if (!res)
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
	free(fl);
	a->aux = nil;
	return res;
}

static Bool attach_float(Area *a, Client *c)
{
	Float *fl = a->aux;
	Frame *f = fl->sel;

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
		attach_frame(a, f);
	}
	attach_client_to_frame(f, c);
	if (a->page == selpage)
		XMapWindow(dpy, f->win);
	focus_float(f, True);
	return True;
}

static void detach_float(Area *a, Client *c, Bool unmap)
{
	Frame *f = c->frame;
	detach_client_from_frame(c, unmap);
	if (!f->clients) {
		detach_frame(a, f);
		destroy_frame(f);
	}
}

static void resize_float(Frame *f, XRectangle *new, XPoint *pt)
{
	f->rect = *new;
}

static void focus_float(Frame *f, Bool raise)
{
	Area *a = f->area;
	Float *fl = a->aux;
	Frame *old = fl->sel;

	sel_client(f->sel);
	fl->sel = f;
	a->file[A_SEL_FRAME]->content = f->file[F_PREFIX]->content;
	if (raise) {
		XRaiseWindow(dpy, f->win);
		center_pointer(f);
	}
	if (old && old != f)
		draw_frame(old);
	draw_frame(f);
}

static Frame *frames_float(Area *a)
{
	Float *fl = a->aux;
	return fl->frames;
}

static void select_frame(void *obj, char *arg)
{
	Area *a = obj;
	Float *fl = a->aux;
	Frame *f = fl->sel;

	if (!f || !arg)
		return;
	if (!strncmp(arg, "prev", 5)) {
		if (f->prev)
			f = f->prev;
		else
			for (f = fl->frames; f && f->next; f = f->next);
	}
	else if (!strncmp(arg, "next", 5)) {
		if (f->next)
			f = f->next;
		else
			f = fl->frames;
	}
	else {
		unsigned int i = 0, idx = blitz_strtonum(arg, 0, fl->nframes - 1);
		for (f = fl->frames; f && i != idx; f = f->next) i++;
	}
	if (f)
		focus_float(f, True);
}

static Frame *sel_float(Area *a) {
	Float *fl = a->aux;
	return fl->sel;
}

static Action *actions_float(Area *a)
{
	return lfloat_acttbl;
}
