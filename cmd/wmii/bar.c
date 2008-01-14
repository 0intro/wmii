/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

static Handlers handlers;
static Bar *free_bars;

void
bar_init(WMScreen *s) {
	WinAttr wa;

	s->brect = s->r;
	s->brect.min.y = s->brect.max.y - labelh(def.font);

	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask =
		  ExposureMask
		| ButtonPressMask
		| ButtonReleaseMask
		| FocusChangeMask
		| SubstructureRedirectMask
		| SubstructureNotifyMask;

	s->barwin = createwindow(&scr.root, s->brect, scr.depth, InputOutput, &wa,
			  CWOverrideRedirect
			| CWBackPixmap
			| CWEventMask);
	sethandler(s->barwin, &handlers);
	mapwin(s->barwin);
}

Bar*
bar_create(Bar **bp, const char *name) {
	static uint id = 1;
	Bar *b;

	b = bar_find(*bp, name);;
	if(b)
		return b;

	if(free_bars) {
		b = free_bars;
		free_bars = b->next;
		memset(b, 0, sizeof(*b));
	}
	else
		b = emallocz(sizeof(Bar));

	b->id = id++;
	utflcpy(b->name, name, sizeof(b->name));
	b->col = def.normcolor;

	for(; *bp; bp = &bp[0]->next)
		if(strcmp(bp[0]->name, name) >= 0)
			break;
	b->next = *bp;
	*bp = b;

	return b;
}

void
bar_destroy(Bar **bp, Bar *b) {
	Bar **p;

	for(p = bp; *p; p = &p[0]->next)
		if(*p == b) break;
	*p = b->next;

	b->next = free_bars;
	free_bars = b;
}

void
bar_resize(WMScreen *s) {
	View *v;

	s->brect = s->r;
	s->brect.min.y = s->brect.max.y - labelh(def.font);

	reshapewin(s->barwin, s->brect);

	sync();
	bar_draw(s);
	for(v = view; v; v = v->next)
		view_arrange(v);
}

void
bar_draw(WMScreen *s) {
	Bar *b, *tb, *largest, **pb;
	Rectangle r;
	Align align;
	uint width, tw, nb;
	float shrink;

	largest = nil;
	tw = width = 0;
	for(nb = 0; nb < nelem(s->bar); nb++)
		for(b = s->bar[nb]; b; b=b->next) {
			b->r.min = ZP;
			b->r.max.y = Dy(s->brect);
			b->r.max.x = def.font->height & ~1;
			if(b->text && strlen(b->text))
				b->r.max.x += textwidth(def.font, b->text);

			width += Dx(b->r);
		}

	if(width > Dx(s->brect)) { /* Not enough room. Shrink bars until they all fit. */
		for(nb = 0; nb < nelem(s->bar); nb++)
			for(b = s->bar[nb]; b; b=b->next) {
				for(pb = &largest; *pb; pb = &pb[0]->smaller)
					if(Dx(pb[0]->r) < Dx(b->r))
						break; 
				b->smaller = *pb;
				*pb = b;
			}
		for(tb = largest; tb; tb = tb->smaller) {
			width -= Dx(tb->r);
			tw += Dx(tb->r);
			shrink = (Dx(s->brect) - width) / (float)tw;
			if(tb->smaller)
				if(Dx(tb->r) * shrink >= Dx(tb->smaller->r))
					break;
		}
		SET(shrink);
		if(tb)
			for(b = largest; b != tb->smaller; b = b->smaller)
				b->r.max.x *= shrink;
		width += tw * shrink;
	}

	tb = nil;
	for(nb = 0; nb < nelem(s->bar); nb++)
		for(b = s->bar[nb]; b; tb=b, b=b->next) {
			if(b == s->bar[BarRight])
				b->r.max.x += Dx(s->brect) - width;

			if(tb)
				b->r = rectaddpt(b->r, Pt(tb->r.max.x, 0));
		}

	r = rectsubpt(s->brect, s->brect.min);
	fill(screen->ibuf, r, def.normcolor.bg);
	for(nb = 0; nb < nelem(s->bar); nb++)
		for(b = s->bar[nb]; b; b=b->next) {
			align = CENTER;
			if(b == s->bar[BarRight])
				align = EAST;
			fill(screen->ibuf, b->r, b->col.bg);
			drawstring(screen->ibuf, def.font, b->r, align, b->text, b->col.fg);
			border(screen->ibuf, b->r, 1, b->col.border);
		}
	copyimage(s->barwin, r, screen->ibuf, ZP);
	sync();
}

Bar*
bar_find(Bar *bp, const char *name) {
	Bar *b;

	for(b = bp; b; b = b->next)
		if(!strncmp(b->name, name, sizeof(b->name)))
			break;
	return b;
}

static void
bdown_event(Window *w, XButtonPressedEvent *e) {
	Bar *b;
	
	USED(w);

	/* Ungrab so a menu can receive events before the button is released */
	XUngrabPointer(display, e->time);
	sync();

	for(b=screen->bar[BarLeft]; b; b=b->next)
		if(rect_haspoint_p(Pt(e->x, e->y), b->r)) {
			event("LeftBarMouseDown %d %s\n", e->button, b->name);
			return;
		}
	for(b=screen->bar[BarRight]; b; b=b->next)
		if(rect_haspoint_p(Pt(e->x, e->y), b->r)) {
			event("RightBarMouseDown %d %s\n", e->button, b->name);
			return;
		}
}

static void
bup_event(Window *w, XButtonPressedEvent *e) {
	Bar *b;
	
	USED(w, e);

	for(b=screen->bar[BarLeft]; b; b=b->next)
		if(rect_haspoint_p(Pt(e->x, e->y), b->r)) {
			event("LeftBarClick %d %s\n", e->button, b->name);
			return;
		}
	for(b=screen->bar[BarRight]; b; b=b->next)
		if(rect_haspoint_p(Pt(e->x, e->y), b->r)) {
			event("RightBarClick %d %s\n", e->button, b->name);
			return;
		}
}

static void
expose_event(Window *w, XExposeEvent *e) {
	USED(w, e);
	bar_draw(screen);
}

static Handlers handlers = {
	.bdown = bdown_event,
	.bup = bup_event,
	.expose = expose_event,
};
