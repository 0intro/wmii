/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

static Area     zero_area = {0};

Area *alloc_area(Page *p, XRectangle * r, char *layout)
{
	char buf[MAX_BUF];
	Area *a = (Area *) cext_emalloc(sizeof(Area));
	int id = count_items((void **) p->area) + 1;

	*a = zero_area;
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
	p->area = (Area **) attach_item_end((void **) p->area, a, sizeof(Area *));
	p->file[P_SEL_AREA]->content = a->file[A_PREFIX]->content;
	p->sel = index_item((void **) p->area, a);
	return a;
}

void destroy_area(Area * a)
{
	unsigned int i;
	a->layout->deinit(a);
	for (i = 0; a->frame && a->frame[i]; i++);
	destroy_frame(a->frame[i]);
	free(a->frame);
	ixp_remove_file(ixps, a->file[A_PREFIX]);
	free(a);
}

void sel_area(Area * a, int raise)
{
	Page *p = a->page;
	if (raise && a->frame) {
		int i;
		for (i = 0; a->frame[i]; i++)
			if (i != a->sel)
				XRaiseWindow(dpy, a->frame[i]->win);
	}
	p->sel = index_item((void **) p->area, a);
	p->file[P_SEL_AREA]->content = a->file[A_PREFIX]->content;
	if (a->frame)
		sel_frame(a->frame[a->sel], raise);
}

void attach_frame_to_area(Area * a, Frame * f)
{
	wmii_move_ixpfile(f->file[F_PREFIX], a->file[A_FRAME_PREFIX]);
	a->file[A_SEL_FRAME]->content = f->file[F_PREFIX]->content;
	a->frame = (Frame **) attach_item_end((void **) a->frame, f, sizeof(Frame *));
	a->sel = index_item((void **) a->frame, f);
	f->area = a;
}

void detach_frame_from_area(Frame * f, int ignore_sel_and_destroy)
{
	Area *a = f->area;
	a->frame = (Frame **) detach_item((void **) a->frame, f, sizeof(Frame *));
	f->area = 0;
	if (a->sel)
		a->sel--;
	else
		a->sel = 0;
}

void draw_area(Area * a)
{
	int i;
	for (i = 0; a->frame && a->frame[i]; i++)
		draw_frame(a->frame[i]);
}

void hide_area(Area * a)
{
	int i;
	for (i = 0; a->frame && a->frame[i]; i++)
		XUnmapWindow(dpy, a->frame[i]->win);
}

void show_area(Area * a)
{
	int i;
	for (i = 0; a->frame && a->frame[i]; i++)
		XMapWindow(dpy, a->frame[i]->win);
}

Area *get_sel_area()
{
	Page *p = cext_get_top_item(&page);

	return p ? p->area[p->sel] : nil;
}
