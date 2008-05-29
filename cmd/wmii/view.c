/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

#define foreach_frame(v, a, f) \
	for((a)=(v)->area; (a); (a)=(a)->next) \
		for((f)=(a)->frame; (f); (f)=(f)->anext)

static bool
empty_p(View *v) {
	Frame *f;
	Area *a;
	char **p;
	int cmp;

	foreach_frame(v, a, f) {
		for(p=f->client->retags; *p; p++) {
			cmp = strcmp(*p, v->name);
			if(cmp == 0)
				goto nextframe;
			if(cmp > 0)
				return false;
		}
		return false;
	nextframe:
		continue;
	}
	return true;
}

static void
_view_select(View *v) {
	if(screen->sel != v) {
		if(screen->sel)
			event("UnfocusTag %s\n",screen->sel->name);
		screen->sel = v;
		event("FocusTag %s\n", v->name);
		event("AreaFocus %a\n", v->sel);
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
	View **vp;
	Client *c;
	View *v;

	for(v=view; v; v=v->next)
		if(!strcmp(name, v->name))
			return v;

	v = emallocz(sizeof *v);
	v->id = id++;
	v->r = screen->r;
	v->r.max.y = screen->barwin->r.min.y;

	utflcpy(v->name, name, sizeof v->name);

	event("CreateTag %s\n", v->name);
	area_create(v, nil, 0);
	column_new(v, v->area, 0);
	
	area_focus(v->area->next);

	for(vp=&view; *vp; vp=&(*vp)->next)
		if(strcmp((*vp)->name, name) >= 0)
			break;
	v->next = *vp;
	*vp = v;

	/* FIXME: Belongs elsewhere */
	/* FIXME: Can do better. */
	for(c=client; c; c=c->next)
		if(c != kludge)
			apply_tags(c, c->tags);

	view_arrange(v);
	if(!screen->sel)
		_view_select(v);
	ewmh_updateviews();
	return v;
}

void
view_destroy(View *v) {
	View **vp;
	Frame *f, *fn;
	View *tv;
	Area *a, *an;

	for(vp=&view; *vp; vp=&(*vp)->next)
		if(*vp == v) break;
	*vp = v->next;

	/* FIXME: Can do better */
	for(a=v->area; a; a=an) {
		an = a->next;
		for(f=a->frame; f; f=fn) {
			fn = f->anext;
			apply_tags(f->client, f->client->tags);
		}
	}

	while((a = v->area->next))
		area_destroy(a);
	area_destroy(v->area);

	event("DestroyTag %s\n", v->name);

	if(v == screen->sel) {
		for(tv=view; tv; tv=tv->next)
			if(tv->next == *vp) break;
		if(tv == nil)
			tv = view;
		if(tv)
			view_focus(screen, tv);
	}
	free(v);
	ewmh_updateviews();
}

Area*
view_findarea(View *v, int idx, bool create) {
	Area *a;

	for(a=v->area->next; a && --idx > 0; a=a->next)
		if(create && a->next == nil)
			return area_create(v, a, 0);
	return a;
}

static void
frames_update_sel(View *v) {
	Area *a;
	Frame *f;

	foreach_frame(v, a, f)
		f->client->sel = f;
}

void
view_update_rect(View *v) {
	Rectangle r, sr, brect;
	Strut *strut;
	Frame *f;
	int left, right, top, bottom;

	top = 0;
	left = 0;
	right = 0;
	bottom = 0;
	for(f=v->area->frame; f; f=f->anext) {
		strut = f->client->strut;
		if(!strut)
			continue;
		/* Can do better in the future. */
		top = max(top, strut->top.max.y);
		left = max(left, strut->left.max.x);
		right = min(right, strut->right.min.x);
		bottom = min(bottom, strut->bottom.min.y);
	}
	r = screen->r;
	r.min.y += min(top, .3 * Dy(screen->r));
	r.min.x += min(left, .3 * Dx(screen->r));
	r.max.x += max(right, -.3 * Dx(screen->r));
	r.max.y += max(bottom, -.3 * Dy(screen->r));
	if(screen->barpos == BTop) {
		bar_sety(r.min.y);
		r.min.y += Dy(screen->brect);
	}else {
		r.max.y -= Dy(screen->brect);
		bar_sety(r.max.y);
	}
	v->r = r;

	brect = screen->brect;
	brect.min.x = screen->r.min.x;
	brect.max.x = screen->r.max.x;
	for(f=v->area->frame; f; f=f->anext) {
		/* This is not pretty. :( */
		strut = f->client->strut;
		if(!strut)
			continue;
		sr = strut->left;
		if(rect_intersect_p(brect, sr))
			brect.min.x = sr.max.x;
		sr = rectaddpt(strut->right, Pt(screen->r.max.x, 0));
		if(rect_intersect_p(brect, sr))
			brect.max.x = sr.min.x;
	}
	bar_setbounds(brect.min.x, brect.max.x);
}

void
view_update(View *v) {
	Client *c;
	Frame *f, *fnext;
	Area *a, *an;
	bool fscrn;

	if(v != screen->sel)
		return;
	if(starting)
		return;

	frames_update_sel(v);
	view_arrange(v);

	fscrn = false;
	for(a=v->area; a; a=an) {
		an = a->next;
		for(f=a->frame; f; f=fnext) {
			fnext = f->anext;
			if(f->client->fullscreen) {
				f->collapsed = false;
				fscrn = true;
				if(!f->area->floating) {
					f->oldarea = area_idx(f->area);
					area_moveto(v->area, f);
					area_setsel(v->area, f);
				}else if(f->oldarea == -1)
					f->oldarea = 0;
			}
		}
	}

	for(c=client; c; c=c->next) {
		f = c->sel;
		if(f && f->view == v)
			client_resize(c, f->r);
		else {
			unmap_frame(c);
			client_unmap(c, IconicState);
		}
		ewmh_updatestate(c);
		ewmh_updateclient(c);
	}

	view_restack(v);
	if(fscrn)
		area_focus(v->area);
	else
		area_focus(v->sel);
	frame_draw_all();
}

void
view_focus(WMScreen *s, View *v) {
	
	USED(s);

	XGrabServer(display);

	_view_select(v);
	view_update(v);

	XUngrabServer(display);
}

void
view_select(const char *arg) {
	char buf[256];

	utflcpy(buf, arg, sizeof buf);
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
	Frame *ff;
	Area *a;
	
	c = f->client;

	a = v->sel;
	if(client_floats_p(c))
		a = v->area;
	else if((ff = client_groupframe(c, v)))
		a = ff->area;
	else if(starting && v->sel->floating)
		a = v->area->next;

	area_attach(a, f);
	/* TODO: Decide whether to focus this frame */
	frame_focus(f);

	if(c->sel == nil)
		c->sel = f;
	view_update(v);
}

void
view_detach(Frame *f) {
	Client *c;
	View *v;

	v = f->view;
	c = f->client;

	area_detach(f);
	if(c->sel == f)
		c->sel = f->cnext;

	if(v == screen->sel)
		view_update(v);
	else if(empty_p(v))
		view_destroy(v);
}

char**
view_names(void) {
	Vector_ptr vec;
	View *v;

	vector_pinit(&vec);
	for(v=view; v; v=v->next)
		vector_ppush(&vec, v->name);
	vector_ppush(&vec, nil);
	return erealloc(vec.ary, vec.n * sizeof *vec.ary);
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

	/* *sigh */
	for(f=v->area->stack; f; f=f->snext)
		if(f->client->w.ewmh.type & TypeDock)
			vector_lpush(&wins, f->client->framewin->w);
		else
			break;

	if(!fscrn)
		vector_lpush(&wins, screen->barwin->w);

	for(; f; f=f->snext)
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
	if(wins.n)
		XRestackWindows(display, (ulong*)wins.ary, wins.n);
}

void
view_scale(View *v, int w) {
	uint xoff, numcol;
	uint minwidth;
	Area *a;
	float scale;
	int dx;

	minwidth = Dx(v->r)/NCOL;

	if(!v->area->next)
		return;

	numcol = 0;
	dx = 0;
	for(a=v->area->next; a; a=a->next) {
		numcol++;
		dx += Dx(a->r);
	}

	scale = (float)w / dx;
	xoff = v->r.min.x;
	for(a=v->area->next; a; a=a->next) {
		a->r.max.x = xoff + Dx(a->r) * scale;
		a->r.min.x = xoff;
		if(!a->next)
			a->r.max.x = v->r.min.x + w;
		xoff = a->r.max.x;
	}

	/* minwidth can only be respected when there is enough space;
	 * the caller should guarantee this */
	if(numcol * minwidth > w)
		return;

	xoff = v->r.min.x;
	for(a=v->area->next; a; a=a->next) {
		a->r.min.x = xoff;

		if(Dx(a->r) < minwidth)
			a->r.max.x = xoff + minwidth;
		if(!a->next)
			a->r.max.x = v->r.min.x + w;
		xoff = a->r.max.x;
	}
}

void
view_arrange(View *v) {
	Area *a;

	if(!v->area->next)
		return;
	/*
	for(a=v->area->next; a; a=anext) {
		anext = a->next;
		if(!a->frame && v->area->next->next)
			area_destroy(a);
	}
	*/
	view_update_rect(v);
	view_scale(v, Dx(v->r));
	for(a=v->area->next; a; a=a->next) {
		/* This is wrong... */
		a->r.min.y = v->r.min.y;
		a->r.max.y = v->r.max.y;
		column_arrange(a, false);
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

	result = emallocz(i * sizeof *result);

	i = 0;
	for(f=v->area->frame; f; f=f->anext)
		if(f != ignore)
			result[i++] = f->r;
	result[i++] = screen->r;
	result[i++] = screen->brect;

	*num = i;
	return result;
}

void
view_update_all(void) {
	View *n, *v, *old;

	old = screen->sel;
	for(v=view; v; v=v->next)
		frames_update_sel(v);

	for(v=view; v; v=n) {
		n=v->next;
		if(v != old && empty_p(v))
			view_destroy(v);
	}

	view_update(screen->sel);
}

uint
view_newcolw(View *v, int num) {
	Rule *r;
	ulong n;

	for(r=def.colrules.rule; r; r=r->next)
		if(regexec(r->regex, v->name, nil, 0)) {
			char buf[sizeof r->value];
			char *toks[16];

			utflcpy(buf, r->value, sizeof buf);

			n = tokenize(toks, 16, buf, '+');
			if(num < n)
				if(getulong(toks[num], &n))
					return Dx(v->r) * (n / 100.0);
			break;
		}
	return 0;
}

char*
view_index(View *v) {
	Rectangle *r;
	Frame *f;
	Area *a;
	int i;

	bufclear();
	for(a=v->area, i=0; a; a=a->next, i++) {
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

