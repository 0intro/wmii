/* Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <math.h>
#include <limits.h>
#include "fns.h"

Client*
area_selclient(Area *a) {               
	if(a && a->sel)
		return a->sel->client;
	return nil;
}

uint
area_idx(Area *a) {
	View *v;
	Area *ap;
	uint i;

	v = a->view;
	i = 0;
	for(ap=v->area; a != ap; ap=ap->next)
		i++;
	return i;
}

int
afmt(Fmt *f) {
	Area *a;

	a = va_arg(f->args, Area*);
	if(a == nil)
		return fmtstrcpy(f, "<nil>");
	if(a->floating)
		return fmtstrcpy(f, "~");
	return fmtprint(f, "%d", area_idx(a));
}

char*
area_name(Area *a) {

	if(a == nil)
		return "<nil>";
	if(a->floating)
		return "~";
	return sxprint("%d", area_idx(a));
}

Area*
area_create(View *v, Area *pos, uint w) {
	static ushort id = 1;
	uint areanum, i;
	uint minwidth;
	int colnum;
	Area *a;

	minwidth = Dx(v->r)/NCOL;

	i = 0;
	if(pos)
		i = area_idx(pos);
	areanum = 0;
	for(a=v->area; a; a=a->next)
		areanum++;

	/* TODO: Need a better sizing/placing algorithm.
	 */
	colnum = areanum - 1;
	if(w == 0) {
		if(colnum >= 0) {
			w = view_newcolw(v, i);
			if (w == 0)
				w = Dx(v->r) / (colnum + 1);
		}
		else
			w = Dx(v->r);
	}

	if(w < minwidth)
		w = minwidth;
	if(colnum && (colnum * minwidth + w) > Dx(v->r))
		return nil;

	if(pos)
		view_scale(v, Dx(v->r) - w);

	a = emallocz(sizeof *a);
	a->view = v;
	a->id = id++;
	if(v->area)
		a->mode = def.colmode;
	else
		a->mode = Coldefault;
	a->frame = nil;
	a->sel = nil;

	a->r = v->r;
	a->r.min.x = 0;
	a->r.max.x = w;

	if(pos) {
		a->next = pos->next;
		a->prev = pos;
	}else {
		a->next = v->area;
		v->area = a;
	}
	if(a->prev)
		a->prev->next = a;
	if(a->next)
		a->next->prev = a;

	if(a == v->area)
		a->floating = true;

	if(v->sel == nil)
		area_focus(a);

	if(!a->floating)
		event("CreateColumn %ud\n", i);
	return a;
}

void
area_destroy(Area *a) {
	Area *ta;
	View *v;
	int idx;

	v = a->view;

	if(a->frame)
		die("destroying non-empty area");

	if(v->revert == a)
		v->revert = nil;
	if(v->oldsel == a)
		v->oldsel = nil;

	idx = area_idx(a);

	if(a->prev && !a->prev->floating)
		ta = a->prev;
	else
		ta = a->next;

	/* Can only destroy the floating area when destroying a
	 * view---after destroying all columns.
	 */
	assert(a->prev || a->next == nil);
	if(a->prev)
		a->prev->next = a->next;
	if(a->next)
		a->next->prev = a->prev;

	if(ta && v->sel == a)
		area_focus(ta);
	view_arrange(v);
	event("DestroyArea %d\n", idx);
	/* Deprecated */
	event("DestroyColumn %d\n", idx);

	free(a);
}

void
area_moveto(Area *to, Frame *f) {
	Area *from;
	bool fromfloating;

	assert(to->view == f->view);

	if(f->client->fullscreen && !to->floating)
		return;

	from = f->area;
	fromfloating = from->floating;

	area_detach(f);

	/* Temporary kludge. */
	if(!to->floating
	&& to->floating != fromfloating
	&& !eqrect(f->colr, ZR)) {
		column_attachrect(to, f, f->colr);
	}else
		area_attach(to, f);
}

void
area_setsel(Area *a, Frame *f) {
	View *v;

	v = a->view;
	for(; f && f->collapsed && f->anext; f=f->anext)
		;
	for(; f && f->collapsed && f->aprev; f=f->aprev)
		;
	if(a == v->sel && f)
		frame_focus(f);
	else
		a->sel = f;
}

void
area_attach(Area *a, Frame *f) {

	f->area = a;
	if(a->floating)
		float_attach(a, f);
	else
		column_attach(a, f);

	view_arrange(a->view);

	if(btassert("4 full", a->frame && a->sel == nil))
		a->sel = a->frame;
}

void
area_detach(Frame *f) {
	View *v;
	Area *a;

	a = f->area;
	v = a->view;

	if(a->floating)
		float_detach(f);
	else
		column_detach(f);

	if(v->sel->sel == nil && v->area->sel)
		v->sel = v->area;

	view_arrange(v);
}

void
area_focus(Area *a) {
	Frame *f;
	View *v;
	Area *old_a;

	v = a->view;
	f = a->sel;
	old_a = v->sel;

	if(view_fullscreen_p(v) && !a->floating)
		return;

	v->sel = a;
	if(!a->floating)
		v->selcol = area_idx(a);
	if(a != old_a)
		v->oldsel = nil;

	if((old_a) && (a->floating != old_a->floating)) {
		v->revert = old_a;
		if(v->area->max)
			view_update(v);
	}

	if(v != screen->sel)
		return;

	move_focus(old_a->sel, f);

	if(f)
		client_focus(f->client);
	else
		client_focus(nil);

	if(a != old_a) {
		event("AreaFocus %a\n", a);
		/* Deprecated */
		if(a->floating)
			event("FocusFloating\n");
		else
			event("ColumnFocus %d\n", area_idx(a));
	}
}

