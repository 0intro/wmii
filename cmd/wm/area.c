/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

Area *alloc_area(Page *p, XRectangle * r, char *layout)
{
	char buf[MAX_BUF];
	Area *a = (Area *) cext_emallocz(sizeof(Area));
	size_t id = cext_sizeof(&p->areas);

	a->rect = *r;
	a->page = p;
	snprintf(buf, MAX_BUF, "/%s/a/%d", p->file[P_PREFIX]->name, id);
	a->file[A_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/%s/a/%d/f", p->file[P_PREFIX]->name,  id);
	a->file[A_FRAME_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/%s/a/%d/f/sel", p->file[P_PREFIX]->name,  id);
	a->file[A_SEL_FRAME] = ixp_create(ixps, buf);
	a->file[A_SEL_FRAME]->bind = 1;
	snprintf(buf, MAX_BUF, "/%s/a/%d/ctl", p->file[P_PREFIX]->name,  id);
	a->file[A_CTL] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/%s/a/%d/geometry", p->file[P_PREFIX]->name,  id);
	a->file[A_GEOMETRY] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/%s/a/%d/layout", p->file[P_PREFIX]->name,  id);
	a->file[A_LAYOUT] = wmii_create_ixpfile(ixps, buf, layout);
	a->layout = get_layout(layout);
	a->layout->init(a);
	cext_attach_item(&p->areas, a);
	p->file[P_SEL_AREA]->content = a->file[A_PREFIX]->content;
	return a;
}

void destroy_area(Area *a)
{
	a->layout->deinit(a);
	ixp_remove_file(ixps, a->file[A_PREFIX]);
	free(a);
}

static void iter_raise_frame(void *item, void *aux)
{
	XRaiseWindow(dpy, ((Frame *)item)->win);
}

void sel_area(Area *a)
{
	Page *p = a->page;
	Frame *f;
	Bool raise = cext_get_item_index(&p->areas, a) == 0;
	if (raise)
		cext_iterate(a->layout->get_frames(a), nil, iter_raise_frame);
	cext_top_item(&p->areas, a);
	p->file[P_SEL_AREA]->content = a->file[A_PREFIX]->content;
	if ((f = get_sel_frame_of_area(a)))
		sel_frame(f, raise);
}

void draw_area(Area *a)
{
	cext_iterate(a->layout->get_frames(a), nil, draw_frame);
}

static void iter_hide_area(void *item, void *aux)
{
	XUnmapWindow(dpy, ((Frame *)item)->win);
}

void hide_area(Area * a)
{
	cext_iterate(a->layout->get_frames(a), nil, iter_hide_area);
}

static void iter_show_area(void *item, void *aux)
{
	XMapWindow(dpy, ((Frame *)item)->win);
}

void show_area(Area * a)
{
	cext_iterate(a->layout->get_frames(a), nil, iter_show_area);
}

Area *get_sel_area()
{
	Page *p = get_sel_page();

	return p ? cext_get_top_item(&p->areas) : nil;
}

void attach_frame_to_area(Area *a, Frame *f)
{
	wmii_move_ixpfile(f->file[F_PREFIX], a->file[A_FRAME_PREFIX]);
	a->file[A_SEL_FRAME]->content = f->file[F_PREFIX]->content;
	f->area = a;
}

void detach_frame_from_area(Frame *f) {
	f->area->file[A_SEL_FRAME]->content = 0;
	f->area = 0;
}
