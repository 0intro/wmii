/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "wm.h"

static void handle_after_write_area(IXPServer *s, File *file);

Area *alloc_area(Page *p, char *layout)
{
	char buf[MAX_BUF];
	Area *a = (Area *) cext_emallocz(sizeof(Area));

	a->page = p;
	snprintf(buf, MAX_BUF, "/%s/layout/%s", p->file[P_PREFIX]->name, layout);
	a->file[A_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/%s/layout/%s/frame", p->file[P_PREFIX]->name, layout);
	a->file[A_FRAME_PREFIX] = ixp_create(ixps, buf);
	snprintf(buf, MAX_BUF, "/%s/layout/%s/frame/sel", p->file[P_PREFIX]->name, layout);
	a->file[A_SEL_FRAME] = ixp_create(ixps, buf);
	a->file[A_SEL_FRAME]->bind = 1;
	snprintf(buf, MAX_BUF, "/%s/layout/%s/ctl", p->file[P_PREFIX]->name, layout);
	a->file[A_CTL] = ixp_create(ixps, buf);
	a->file[A_CTL]->after_write = handle_after_write_area;
	snprintf(buf, MAX_BUF, "/%s/layout/%s/name", p->file[P_PREFIX]->name, layout);
	a->file[A_LAYOUT] = wmii_create_ixpfile(ixps, buf, layout);
	a->file[A_LAYOUT]->after_write = handle_after_write_area; 
	a->layout = match_layout(layout);
	a->layout->init(a, nil);
	p->file[P_SEL_AREA]->content = a->file[A_PREFIX]->content;
	return a;
}

void destroy_area(Area *a)
{
	a->layout->deinit(a);
	ixp_remove_file(ixps, a->file[A_PREFIX]);
	free(a);
}

void focus_area(Area *a)
{
	Page *p = a->page;
	Frame *f;
	p->sel = a;
	p->file[P_SEL_AREA]->content = a->file[A_PREFIX]->content;
	if ((f = a->layout->sel(a)))
		a->layout->focus(f, False);
}

void hide_area(Area * a)
{
	Frame *f;
	for (f = a->layout->frames(a); f; f = f->next)
		XUnmapWindow(dpy, f->win);
}

void show_area(Area *a, Bool raise)
{
	Frame *f;
	for (f = a->layout->frames(a); f; f = f->next)
		if (raise)
			XMapRaised(dpy, f->win);
		else 
			XMapWindow(dpy, f->win);
}

Area *sel_area()
{
	return selpage ? selpage->sel : nil;
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

static void handle_after_write_area(IXPServer *s, File *file) {
	Page *p;
	for (p = pages; p; p = p->next) {
		if (file == p->managed->file[A_CTL]) {
			run_action(file, p->managed, p->managed->layout->actions(p->managed));
			return;
		}
		else if (file == p->floating->file[A_CTL]) {
			run_action(file, p->floating, p->floating->layout->actions(p->floating));
			return;
		}
		else if (file == p->managed->file[A_LAYOUT]) {
			Layout *l = match_layout(file->content);
			if (l) {
				Client *clients = p->managed->layout->deinit(p->managed);
				p->managed->layout = l;
				p->managed->layout->init(p->managed, clients);
				invoke_wm_event(def[WM_EVENT_PAGE_UPDATE]);
			}
		}
	}
}
