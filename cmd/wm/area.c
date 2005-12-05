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

Area *alloc_area(Page *p, XRectangle * r, char *layout)
{
	char buf[MAX_BUF];
	Area *a = (Area *) emalloc(sizeof(Area));
	int id = count_items((void **) p->area) + 1;

	*a = zero_area;
	a->rect = *r;
	a->page = p;
	snprintf(buf, MAX_BUF, "/p/%s/a/%d", p->file[P_PREFIX]->name, id);
	a->file[A_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/p/%s/a/%d/f", p->file[P_PREFIX]->name,  id);
	a->file[A_FRAME_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/p/%s/a/%d/f/sel", p->file[P_PREFIX]->name,  id);
	a->file[A_SEL_FRAME] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/p/%s/a/%d/ctl", p->file[P_PREFIX]->name,  id);
	a->file[A_CTL] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/p/%s/a/%d/geometry", p->file[P_PREFIX]->name,  id);
	a->file[A_GEOMETRY] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/p/%s/a/%d/layout", p->file[P_PREFIX]->name,  id);
	a->file[A_LAYOUT] = wmii_create_ixpfile(ixps, buf, layout);
	a->layout = get_layout(layout);
	p->area = (Area **) attach_item_end((void **) p->area, a, sizeof(Area *));
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

void sel_area(Area * a, int raise, int up, int down)
{
	Page *p = a->page;
	if (!p)
		return;

	if (down && a->frame)
		sel_frame(a->frame[a->sel], raise, 0, down);
	p->sel = index_item((void **) p->area, a);
	p->file[P_SEL_AREA]->content = a->file[A_PREFIX]->content;
	if (up)
		sel_page(p, raise, 0);
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
