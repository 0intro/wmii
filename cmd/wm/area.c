/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

#include <cext.h>

static Area     zero_area = {0};

Area *alloc_area(Page *p, XRectangle * r)
{
	char buf[MAX_BUF];
	Area *a = (Area *) emalloc(sizeof(Area));

	*a = zero_area;
	a->rect = *r;
	a->page = p;
	snprintf(buf, MAX_BUF, "/%s/area/%d", p->files[P_PREFIX]->name, count_items((void **) p->area));
	a->files[A_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/%s/area/%d/frame/sel", p->files[P_PREFIX]->name, count_items((void **) p->area));
	a->files[A_SEL_FRAME] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/%s/area/%d/ctl", p->files[P_PREFIX]->name, count_items((void **) p->area));
	a->files[A_CTL] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/%s/area/%d/geometry", p->files[P_PREFIX]->name, count_items((void **) p->area));
	a->files[A_GEOMETRY] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/%s/area/%d/layout", p->files[P_PREFIX]->name, count_items((void **) p->area));
	a->files[A_LAYOUT] = ixp_create(ixps, buf);
	p->area =
		(Area **) attach_item_end((void **) p->area, a, sizeof(Area *));
	p->sel = index_item((void **) p->area, a);
	return a;
}

void free_area(Area * a)
{
	ixp_remove_file(ixps, a->files[A_PREFIX]);
	free(a);
}

void destroy_area(Area * a)
{
	unsigned int i;
	a->layout->deinit(a);
	for (i = 0; a->frame && a->frame[i]; i++);
	destroy_frame(a->frame[i]);
	free(a->frame);
	free_area(a);
}

void focus_area(Area * a, int raise, int up, int down)
{
	Page *p = a->page;
	if (!p)
		return;

	if (down && a->frame)
		focus_frame(a->frame[a->sel], raise, 0, down);
	p->sel = index_item((void **) p->area, a);
	p->files[P_SEL_AREA]->content = a->files[A_PREFIX]->content;
	if (up)
		focus_page(p, raise, 0);
}

void attach_frame_to_area(Area * a, Frame * f)
{

}

void detach_frame_from_area(Frame * f, int ignore_focus_and_destroy)
{

}

void draw_area(Area * a)
{
}

void hide_area(Area * a)
{
}

void show_area(Area * a)
{
}
