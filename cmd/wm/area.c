/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

static void handle_after_write_area(IXPServer * s, File * f);

Area *alloc_area(Page *p, char *layout)
{
	char buf[MAX_BUF];
	Area *a = (Area *) cext_emallocz(sizeof(Area));
	size_t id = cext_sizeof_container(&p->areas);

	a->page = p;
	snprintf(buf, MAX_BUF, "/%s/layout/%d", p->file[P_PREFIX]->name, id);
	a->file[A_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/%s/layout/%d/frame", p->file[P_PREFIX]->name,  id);
	a->file[A_FRAME_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/%s/layout/%d/frame/sel", p->file[P_PREFIX]->name,  id);
	a->file[A_SEL_FRAME] = ixp_create(ixps, buf);
	a->file[A_SEL_FRAME]->bind = 1;
	snprintf(buf, MAX_BUF, "/%s/layout/%d/ctl", p->file[P_PREFIX]->name,  id);
	a->file[A_CTL] = ixp_create(ixps, buf);
	a->file[A_CTL]->after_write = handle_after_write_area;
	snprintf(buf, MAX_BUF, "/%s/layout/%d/name", p->file[P_PREFIX]->name,  id);
	a->file[A_LAYOUT] = wmii_create_ixpfile(ixps, buf, layout);
	a->file[A_LAYOUT]->after_write = handle_after_write_area; 
	a->layout = get_layout(layout);
	a->layout->init(a);
	cext_attach_item(&p->areas, a);
	cext_attach_item(areas, a);
	p->file[P_SEL_AREA]->content = a->file[A_PREFIX]->content;
	return a;
}

void destroy_area(Area *a)
{
	Client *c;
	a->layout->deinit(a);
	while ((c = cext_stack_get_top_item(&a->clients))) {
		cext_detach_item(&a->clients, c);
		cext_attach_item(detached, c);
	}
	ixp_remove_file(ixps, a->file[A_PREFIX]);
	cext_detach_item(areas, a);
	free(a);
}

void sel_area(Area *a)
{
	Page *p = a->page;
	Frame *f;
	cext_stack_top_item(&p->areas, a);
	p->file[P_SEL_AREA]->content = a->file[A_PREFIX]->content;
	if ((f = get_sel_frame_of_area(a)))
		a->layout->select(f, False);
}

static void iter_hide_area(void *item, void *aux)
{
	XUnmapWindow(dpy, ((Frame *)item)->win);
}

void hide_area(Area * a)
{
	cext_list_iterate(a->layout->get_frames(a), nil, iter_hide_area);
}

static void iter_show_area(void *item, void *aux)
{
	XMapWindow(dpy, ((Frame *)item)->win);
}

void show_area(Area * a)
{
	cext_list_iterate(a->layout->get_frames(a), nil, iter_show_area);
}

Area *get_sel_area()
{
	Page *p = get_sel_page();

	return p ? cext_stack_get_top_item(&p->areas) : nil;
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

static void iter_after_write_area(void *item, void *aux)
{
	Area *a = item;
	File *file = aux;
	if (file == a->file[A_CTL]) {
		run_action(file, a, a->layout->get_actions(a));
		return;
	}
	if (file == a->file[A_LAYOUT]) {
		Layout *l = get_layout(file->content);
		if (l) {
			a->layout->deinit(a);
			a->layout = l;
			a->layout->init(a);
			invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
		}
		return;
	}
}

static void handle_after_write_area(IXPServer *s, File *f) {
	cext_list_iterate(areas, f, iter_after_write_area);
}



