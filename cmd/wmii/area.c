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
	for(i = 0, ap = v->area; a != ap; ap = ap->next)
		i++;
	return i;
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
	a->mode = def.colmode;
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
		a->floating = True;

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
	Rectangle tr;
	Area *from;

	assert(to->view == f->view);

	from = f->area;

	if(to->floating != from->floating) {
		/* XXX: This must be changed. */
		tr = f->revert;
		f->revert = f->r;
		f->r = tr;
	}

	area_detach(f);
	area_attach(to, f);

	/* Temporary kludge. */
	if(!to->floating && to->floating != from->floating)
		column_resizeframe(f, &tr);
}

void
area_setsel(Area *a, Frame *f) {
	View *v;

	v = a->view;
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

	view_restack(a->view);

	if(a->frame)
		assert(a->sel);
}

void
area_detach(Frame *f) {
	Area *a;

	a = f->area;

	if(a->floating)
		float_detach(f);
	else
		column_detach(f);
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

	if((old_a) && (a->floating != old_a->floating))
		v->revert = old_a;

	if(v != screen->sel)
		return;

	move_focus(old_a->sel, f);

	if(f)
		client_focus(f->client);
	else
		client_focus(nil);

	if(a != old_a) {
		event("AreaFocus %s\n", area_name(a));
		/* Deprecated */
		if(a->floating)
			event("FocusFloating\n");
		else
			event("ColumnFocus %d\n", area_idx(a));
	}
}

