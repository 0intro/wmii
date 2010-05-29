/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
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

int
area_idx(Area *a) {
	View *v;
	Area *ap;
	uint i;

	v = a->view;
	if(a->floating)
		return -1;
	i = 1;
	for(ap=v->areas[a->screen]; a != ap; ap=ap->next)
		i++;
	return i;
}

static Rectangle
area_rect(void *v) {
	Area *a;

	a = v;
	return a->r;
}

Area*
area_find(View *v, Rectangle r, int dir, bool wrap) {
	static Vector_ptr vec;
	Area *a;
	int s;

	vec.n = 0;
	foreach_column(v, s, a)
		vector_ppush(&vec, a);

	return findthing(r, dir, &vec, area_rect, wrap);
}

int
afmt(Fmt *f) {
	Area *a;

	a = va_arg(f->args, Area*);
	if(a == nil)
		return fmtstrcpy(f, "<nil>");
	if(a->floating)
		return fmtstrcpy(f, "~");
	if(a->screen > 0 || (f->flags & FmtSharp))
		return fmtprint(f, "%d:%d", a->screen, area_idx(a));
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
area_create(View *v, Area *pos, int scrn, uint width) {
	static ushort id = 1;
	int i, j;
	uint minwidth, index;
	int numcols;
	Area *a;

	assert(!pos || pos->screen == scrn);
	SET(index);
	if(v->areas) { /* Creating a column. */
		minwidth = column_minwidth();
		index = pos ? area_idx(pos) : 0;
		numcols = 0;
		for(a=v->areas[scrn]; a; a=a->next)
			numcols++;

		/* TODO: Need a better sizing/placing algorithm. */
		if(width == 0) {
			if(numcols >= 0) {
				width = view_newcolwidth(v, scrn, index);
				if (width == 0)
					width = Dx(v->r[scrn]) / (numcols + 1);
			}
			else
				width = Dx(v->r[scrn]);
		}

		if(width < minwidth)
			width = minwidth;
		minwidth = numcols * minwidth + minwidth;
		if(minwidth > Dx(v->r[scrn]))
			return nil;

		i = minwidth - Dx(v->pad[scrn]) - Dx(v->r[scrn]);
		if(i > 0 && Dx(v->pad[scrn])) {
			j = min(i/2, v->pad[scrn].min.x);
			v->pad[scrn].min.x -= j;
			v->pad[scrn].max.x += i - j;
		}

		view_scale(v, scrn, Dx(v->r[scrn]) - width);
	}

	a = emallocz(sizeof *a);
	a->view = v;
	a->screen = scrn;
	a->id = id++;
	a->floating = !v->floating;
	if(a->floating)
		a->mode = Coldefault;
	else
		a->mode = def.colmode;
	a->frame = nil;
	a->sel = nil;

	a->r = v->r[scrn];
	a->r.min.x = 0;
	a->r.max.x = width;

	if(a->floating) {
		v->floating = a;
		a->screen = -1;
	}
	else if(pos) {
		a->next = pos->next;
		a->prev = pos;
	}
	else {
		a->next = v->areas[scrn];
		v->areas[scrn] = a;
	}
	if(a->prev)
		a->prev->next = a;
	if(a->next)
		a->next->prev = a;

	if(v->sel == nil && !a->floating)
		area_focus(a);

	if(!a->floating)
		event("CreateColumn %ud\n", index);
	return a;
}

void
area_destroy(Area *a) {
	Area *newfocus;
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
		newfocus = a->prev;
	else
		newfocus = a->next;

	/* Can only destroy the floating area when destroying a
	 * view---after destroying all columns.
	 */
	assert(!a->floating || !v->areas[0]);
	if(a->prev)
		a->prev->next = a->next;
	else if(!a->floating)
		v->areas[a->screen] = a->next;
	else
		v->floating = nil;
	if(a->next)
		a->next->prev = a->prev;

	if(newfocus && v->sel == a)
		area_focus(newfocus);

	view_arrange(v);
	event("DestroyArea %d\n", idx);

	free(a);
}

void
area_moveto(Area *to, Frame *f) {
	Area *from;

	assert(to->view == f->view);

	if(f->client->fullscreen >= 0 && !to->floating)
		return;

	from = f->area;
	if (from == to)
		return;

	area_detach(f);

	/* Temporary kludge. */
	if(!to->floating
	&& to->floating != from->floating
	&& !eqrect(f->colr, ZR))
		column_attachrect(to, f, f->colr);
	else
		area_attach(to, f);
}

void
area_setsel(Area *a, Frame *f) {
	View *v;

	v = a->view;
	/* XXX: Stack. */
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

	if(v->sel->sel == nil && v->floating->sel)
	if(v->floating->sel->client->nofocus)
		v->sel = v->floating;

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

	if(!a->floating && view_fullscreen_p(v, a->screen))
		return;

	v->sel = a;
	if(!a->floating) {
		v->selcol = area_idx(a);
		v->selscreen = a->screen;
	}
	if(a != old_a)
		v->oldsel = nil;

	if((old_a) && (a->floating != old_a->floating)) {
		v->revert = old_a;
		if(v->floating->max)
			view_update(v);
	}

	if(v != selview)
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

