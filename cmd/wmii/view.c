/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

static bool
empty_p(View *v) {
	Area *a;
	for(a=v->area; a; a=a->next)
		if(a->frame)
			return False;
	return True;
}

static void
_view_select(View *v) {
	if(screen->sel != v) {
		if(screen->sel)
			event("UnfocusTag %s\n",screen->sel->name);
		screen->sel = v;
		event("FocusTag %s\n", screen->sel->name);
		ewmh_updateview();
	}
}

Client*
view_selclient(View *v) {
	return v->sel && v->sel->sel ? v->sel->sel->client : nil;
}

bool
view_fullscreen_p(View *v) {
	Frame *f;

	for(f=v->area->frame; f; f=f->anext)
		if(f->client->fullscreen)
			return true;
	return false;
}

View*
view_create(const char *name) {
	static ushort id = 1;
	View **i, *v;

	for(v=view; v; v=v->next)
		if(!strcmp(name, v->name))
			return v;

	v = emallocz(sizeof(View));
	v->id = id++;

	utflcpy(v->name, name, sizeof(v->name));

	event("CreateTag %s\n", v->name);
	area_create(v, nil, 0);
	column_new(v, v->area, 0);
	
	area_focus(v->area->next);

	for(i=&view; *i; i=&(*i)->next)
		if(strcmp((*i)->name, name) >= 0)
			break;
	v->next = *i;
	*i = v;

	if(!screen->sel)
		_view_select(v);
	ewmh_updateviews();
	return v;
}

void
view_destroy(View *v) {
	Area *a;
	View **i, *tv;

	while((a = v->area->next))
		area_destroy(a);
	area_destroy(v->area);

	for(i=&view; *i; i=&(*i)->next)
		if(*i == v) break;
	*i = v->next;

	event("DestroyTag %s\n", v->name);

	if(v == screen->sel) {
		for(tv=view; tv; tv=tv->next)
			if(tv->next == *i) break;
		if(tv == nil)
			tv = view;
		if(tv)
			view_focus(screen, tv);
	}
	free(v);
	ewmh_updateviews();
}

static void
update_frame_selectors(View *v) {
	Area *a;
	Frame *f;

	for(a=v->area; a; a=a->next)
		for(f=a->frame; f; f=f->anext)
			f->client->sel = f;
}

void
view_focus(WMScreen *s, View *v) {
	Frame *f;
	Client *c;
	
	USED(s);

	XGrabServer(display);

	_view_select(v);
	update_frame_selectors(v);
	div_update_all();
	for(c=client; c; c=c->next)
		if((f = c->sel)) {
			if(f->view == v)
				client_resize(c, f->r);
			else {
				unmap_frame(c);
				client_unmap(c, IconicState);
			}
			ewmh_updatestate(c);
			ewmh_updateclient(c);
		}

	view_restack(v);
	area_focus(v->sel);
	frame_draw_all();

	sync();
	XUngrabServer(display);
	flushevents(EnterWindowMask, False);
}

void
view_select(const char *arg) {
	char buf[256];

	utflcpy(buf, arg, sizeof(buf));
	trim(buf, " \t+/");

	if(buf[0] == '\0')
		return;
	if(!strcmp(buf, ".") || !strcmp(buf, ".."))
		return;

	_view_select(view_create(buf));
	view_update_all(); /* performs view_focus */
}

void
view_attach(View *v, Frame *f) {
	Client *c;
	Area *a;
	
	c = f->client;
	c->revert = nil;

	a = v->sel;
	if(c->trans || c->floating || c->fixedsize
	|| c->titleless || c->borderless || c->fullscreen
	|| (c->w.ewmh.type & TypeDialog))
		a = v->area;
	else if(starting && v->sel->floating)
		a = v->area->next;
	if(!(c->w.ewmh.type & TypeSplash))
		area_focus(a);
	area_attach(a, f);
}

void
view_restack(View *v) {
	static Vector_long wins;
	Divide *d;
	Frame *f;
	Area *a;
	bool fscrn;
	
	if(v != screen->sel)
		return;

	wins.n = 0;
	fscrn = view_fullscreen_p(v);

	if(!fscrn)
		vector_lpush(&wins, screen->barwin->w);

	for(f=v->area->stack; f; f=f->snext)
		vector_lpush(&wins, f->client->framewin->w);

	if(fscrn)
		vector_lpush(&wins, screen->barwin->w);

	for(d = divs; d && d->w->mapped; d = d->next)
		vector_lpush(&wins, d->w->w);

	for(a=v->area->next; a; a=a->next)
		if(a->frame) {
			vector_lpush(&wins, a->sel->client->framewin->w);
			for(f=a->frame; f; f=f->anext)
				if(f != a->sel)
					vector_lpush(&wins, f->client->framewin->w);
		}

	ewmh_updatestacking();
	if(wins.n) {
		XRaiseWindow(display, wins.ary[0]);
		XRestackWindows(display, (ulong*)wins.ary, wins.n);
	}
}

void
view_scale(View *v, int w) {
	uint xoff, numcol;
	uint minwidth;
	Area *a;
	float scale;
	int wdiff, dx;

	minwidth = Dx(screen->r)/NCOL;

	if(!v->area->next)
		return;

	numcol = 0;
	dx = 0;
	for(a=v->area->next; a; a=a->next) {
		numcol++;
		dx += Dx(a->r);
	}

	scale = (float)w / dx;
	xoff = 0;
	for(a=v->area->next; a; a=a->next) {
		a->r.max.x = xoff + Dx(a->r) * scale;
		a->r.min.x = xoff;
		if(!a->next)
			a->r.max.x = w;
		xoff = a->r.max.x;
	}

	/* minwidth can only be respected when there is enough space;
	 * the caller should guarantee this */
	if(numcol * minwidth > w)
		return;

	dx = numcol * minwidth;
	xoff = 0;
	for(a=v->area->next, numcol--; a; a=a->next, numcol--) {
		a->r.min.x = xoff;

		if(Dx(a->r) < minwidth)
			a->r.max.x = xoff + minwidth;
		else if((wdiff = xoff + Dx(a->r) - w + dx) > 0)
			a->r.max.x -= wdiff;
		if(!a->next)
			a->r.max.x = w;
		xoff = a->r.max.x;
	}
}

void
view_arrange(View *v) {
	uint xoff;
	Area *a;

	if(!v->area->next)
		return;

	view_scale(v, Dx(screen->r));
	xoff = 0;
	for(a=v->area->next; a; a=a->next) {
		a->r.min.x = xoff;
		a->r.min.y = 0;
		a->r.max.y = screen->brect.min.y;
		xoff = a->r.max.x;
		column_arrange(a, False);
	}
	if(v == screen->sel)
		div_update_all();
}

Rectangle*
view_rects(View *v, uint *num, Frame *ignore) {
	Rectangle *result;
	Frame *f;
	int i;

	i = 2;
	for(f=v->area->frame; f; f=f->anext)
		i++;

	result = emallocz(i * sizeof(Rectangle));

	i = 0;
	for(f=v->area->frame; f; f=f->anext)
		if(f != ignore)
			result[i++] = f->r;
	result[i++] = screen->r;
	result[i++] = screen->brect;

	*num = i;
	return result;
}

char*
view_index(View *v) {
	Rectangle *r;
	Frame *f;
	Area *a;
	int i;

	bufclear();
	for((a=v->area), (i=0); a; (a=a->next), i++) {
		if(a->floating)
			bufprint("# ~ %d %d\n", Dx(a->r), Dy(a->r));
		else
			bufprint("# %d %d %d\n", i, a->r.min.x, Dx(a->r));

		for(f=a->frame; f; f=f->anext) {
			r = &f->r;
			if(a->floating)
				bufprint("~ %C %d %d %d %d %s\n",
						f->client,
						r->min.x, r->min.y,
						Dx(*r), Dy(*r),
						f->client->props);
			else
				bufprint("%d %C %d %d %s\n",
						i, f->client,
						r->min.y, Dy(*r),
						f->client->props);
		}
	}
	return buffer;
}

char*
view_ctl(View *v) {
	Area *a;
	uint i;

	bufclear();
	bufprint("%s\n", v->name);

	/* select <area>[ <frame>] */
	bufprint("select %s", area_name(v->sel));
	if(v->sel->sel)
		bufprint(" %d", frame_idx(v->sel->sel));
	bufprint("\n");

	/* select client <client> */
	if(v->sel->sel)
		bufprint("select client %C\n", v->sel->sel->client);

	for(a = v->area->next, i = 1; a; a = a->next, i++)
		bufprint("colmode %d %s\n", i, colmode2str(a->mode));
	return buffer;
}

void
view_update_all(void) {
	View *n, *v, *old;
	int found;

	old = screen->sel;
	for(v=view; v; v=v->next)
		update_frame_selectors(v);

	found = 0;
	for(v=view; v; v=n) {
		n=v->next;
		if(v != old) {
			if(empty_p(v))
				view_destroy(v);
			else
				found++;
		}
	}

	if(found && !strcmp(old->name, "nil") && empty_p(old))
		view_destroy(old);
	view_focus(screen, screen->sel);
}

uint
view_newcolw(View *v, int num) {
	Rule *r;
	ulong n;

	for(r=def.colrules.rule; r; r=r->next)
		if(regexec(r->regex, v->name, nil, 0)) {
			char buf[sizeof r->value];
			char *toks[16];

			strcpy(buf, r->value);

			n = tokenize(toks, 16, buf, '+');
			if(num < n)
				if(getulong(toks[num], &n))
					return Dx(screen->r) * (n / 100.0);
			break;
		}
	return 0;
}

