/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2008 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

#define foreach_area(v, s, a) \
	Area *__anext; /* Getting ugly... */ \
	for(s=0; s <= nscreens; s++) \
		for((a)=(s < nscreens ? (v)->areas[s] : v->floating), __anext=(a)->next; (a); (void)(((a)=__anext) && (__anext=(a)->next)))

#define foreach_frame(v, s, a, f) \
	Frame *__fnext; \
	foreach_area(v, s, a) \
		for((void)(((f)=(a)->frame) && (__fnext=(f)->anext)); (f); (void)(((f)=__fnext) && (__fnext=(f)->anext)))

static bool
empty_p(View *v) {
	Frame *f;
	Area *a;
	char **p;
	int cmp;
	int s;

	foreach_frame(v, s, a, f) {
		cmp = 1;
		for(p=f->client->retags; *p; p++) {
			cmp = strcmp(*p, v->name);
			if(cmp >= 0)
				break;
		}
		if(cmp)
			return false;
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
	if(v->sel && v->sel->sel)
		return v->sel->sel->client;
	return nil;
}

bool
view_fullscreen_p(View *v) {
	Frame *f;

	for(f=v->floating->frame; f; f=f->anext)
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
	int i;

	for(vp=&view; *vp; vp=&(*vp)->next) {
		i = strcmp((*vp)->name, name);
		if(i == 0)
			return *vp;
		if(i > 0)
			break;
	}

	v = emallocz(sizeof *v);
	v->id = id++;
	v->r = screen->r;

	utflcpy(v->name, name, sizeof v->name);

	event("CreateTag %s\n", v->name);
	area_create(v, nil, screen->idx, 0);

	v->areas = emallocz(nscreens * sizeof *v->areas);
	for(i=0; i < nscreens; i++)
		view_init(v, i);
	
	area_focus(v->firstarea);

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
view_init(View *v, int iscreen) {
	column_new(v, nil, iscreen, 0);
}

void
view_destroy(View *v) {
	View **vp;
	Frame *f, *fn;
	View *tv;
	Area *a, *an;
	int s;

	if(v->dead)
		return;
	v->dead = true;

	for(vp=&view; *vp; vp=&(*vp)->next)
		if(*vp == v) break;
	*vp = v->next;
	assert(v != v->next);

	/* FIXME: Can do better */
	/* Detach frames held here by regex tags. */
	for(a=v->floating; a; a=an) {
		an = a->next;
		for(f=a->frame; f; f=fn) {
			fn = f->anext;
			apply_tags(f->client, f->client->tags);
		}
	}

	foreach_area(v, s, a)
		area_destroy(a);

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

	for(a=v->firstarea; a && --idx > 0; a=a->next)
		if(create && a->next == nil)
			return area_create(v, a, screen->idx, 0);
	return a;
}

static void
frames_update_sel(View *v) {
	Frame *f;
	Area *a;
	int s;

	foreach_frame(v, s, a, f)
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
	for(f=v->floating->frame; f; f=f->anext) {
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
	v->floating->r = r;
	v->r = r;

	brect = screen->brect;
	brect.min.x = screen->r.min.x;
	brect.max.x = screen->r.max.x;
	for(f=v->floating->frame; f; f=f->anext) {
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
	Frame *f;
	Area *a;
	bool fscrn;
	int s;

	if(v != screen->sel)
		return;
	if(starting)
		return;

	frames_update_sel(v);
	view_arrange(v);

	foreach_frame(v, s, a, f)
		if(f->client->fullscreen) {
			f->collapsed = false;
			fscrn = true;
			if(!f->area->floating) {
				f->oldarea = area_idx(f->area);
				area_moveto(v->floating, f);
				area_setsel(v->floating, f);
			}else if(f->oldarea == -1)
				f->oldarea = 0;
		}

	for(c=client; c; c=c->next) {
		f = c->sel;
		if((f && f->view == v)
		&& (f->area == v->sel || !(f->area && f->area->max && f->area->floating))) {
			if(f->area)
				client_resize(c, f->r);
		}else {
			unmap_frame(c);
			client_unmap(c, IconicState);
		}
		ewmh_updatestate(c);
		ewmh_updateclient(c);
	}

	view_restack(v);
	if(fscrn)
		area_focus(v->floating);
	else
		area_focus(v->sel);
	frame_draw_all();
}

void
view_focus(WMScreen *s, View *v) {
	
	USED(s);

	_view_select(v);
	view_update(v);
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
	Area *a, *oldsel;
	
	c = f->client;

	oldsel = v->oldsel;
	a = v->sel;
	if(client_floats_p(c)) {
		if(v->sel != v->floating)
			oldsel = v->sel;
		a = v->floating;
	}
	else if((ff = client_groupframe(c, v)))
		a = ff->area;
	else if(v->sel->floating) {
		if(v->oldsel)
			a = v->oldsel;
		/* Don't float a frame when starting or when its
		 * last focused frame didn't float. Important when
		 * tagging with +foo.
		 */
		else if(starting || c->sel && c->sel->area && !c->sel->area->floating)
			a = v->firstarea;
	}

	area_attach(a, f);
	/* TODO: Decide whether to focus this frame */
	bool newgroup = !c->group
		     || c->group->ref == 1
		     || view_selclient(v) && (view_selclient(v)->group == c->group)
		     || group_leader(c->group) && !client_viewframe(group_leader(c->group),
								    c->sel->view);
	if(!(c->w.ewmh.type & (TypeSplash|TypeDock))) {
		if(newgroup)
			frame_focus(f);
		else if(c->group && f->area->sel->client->group == c->group)
			/* XXX: Stack. */
			area_setsel(f->area, f);
	}

	if(oldsel)
		v->oldsel = oldsel;

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
	for(f=v->floating->stack; f; f=f->snext)
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

	for(a=v->firstarea; a; a=a->next)
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

	if(!v->firstarea)
		return;

	numcol = 0;
	dx = 0;
	for(a=v->firstarea; a; a=a->next) {
		numcol++;
		dx += Dx(a->r);
	}

	scale = (float)w / dx;
	xoff = v->r.min.x;
	for(a=v->firstarea; a; a=a->next) {
		a->r.max.x = xoff + Dx(a->r) * scale;
		a->r.min.x = xoff;
		if(!a->next)
			a->r.max.x = v->r.min.x + w;
		xoff = a->r.max.x;
	}

	if(numcol * minwidth > w)
		return;

	xoff = v->r.min.x;
	for(a=v->firstarea; a; a=a->next) {
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

	if(!v->firstarea)
		return;

	view_update_rect(v);
	view_scale(v, Dx(v->r));
	for(a=v->firstarea; a; a=a->next) {
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
	for(f=v->floating->frame; f; f=f->anext)
		i++;
	result = emallocz(i * sizeof *result);

	i = 0;
	for(f=v->floating->frame; f; f=f->anext)
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
	char *toks[16];
	char buf[sizeof r->value];
	ulong n;

	for(r=def.colrules.rule; r; r=r->next)
		if(regexec(r->regex, v->name, nil, 0)) {
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
	int i, s;

	bufclear();
	foreach_area(v, s, a) {
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

