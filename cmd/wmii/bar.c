/* Copyright ©2006-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include "fns.h"

static Handlers handlers;

#define foreach_bar(s, b) \
	for(int __bar_n=0; __bar_n < nelem((s)->bar); __bar_n++) \
		for((b)=(s)->bar[__bar_n]; (b); (b)=(b)->next)

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
		| FocusChangeMask;
	s->barwin = createwindow(&scr.root, s->brect, scr.depth, InputOutput, &wa,
			  CWOverrideRedirect
			| CWBackPixmap
			| CWEventMask);
	s->barwin->aux = s;
	xdnd_initwindow(s->barwin);
	sethandler(s->barwin, &handlers);
	mapwin(s->barwin);
}

void
bar_resize(WMScreen *s) {
	View *v;

	s->brect = s->r;
	s->brect.min.y = s->brect.max.y - labelh(def.font);

	reshapewin(s->barwin, s->brect);

	bar_draw(s);
	for(v=view; v; v=v->next)
		view_arrange(v);
}

void
bar_setbounds(int left, int right) {
	Rectangle *r;

	r = &screen->brect;
	r->min.x = left;
	r->max.x = right;
	reshapewin(screen->barwin, *r);
}

void
bar_sety(int y) {
	Rectangle *r;
	int dy;

	r = &screen->brect;

	dy = y - r->min.y;
	r->min.y += dy;
	r->max.y += dy;
	reshapewin(screen->barwin, *r);
}

Bar*
bar_create(Bar **bp, const char *name) {
	static uint id = 1;
	WMScreen *s;
	Bar *b;
	uint i;

	b = bar_find(*bp, name);;
	if(b)
		return b;

	b = emallocz(sizeof *b);
	b->id = id++;
	utflcpy(b->name, name, sizeof b->name);
	b->col = def.normcolor;
	
	/* FIXME: Kludge. */
	for(s=screens; s < screens+num_screens; s++) {
		i = bp - s->bar;
		if(i < nelem(s->bar))
			b->bar = i;
	}

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
	free(b);
}

void
bar_draw(WMScreen *s) {
	Bar *b, *tb, *largest, **pb;
	Rectangle r;
	Align align;
	uint width, tw;
	float shrink;

	largest = nil;
	tw = width = 0;
	foreach_bar(s, b) {
		b->r.min = ZP;
		b->r.max.y = Dy(s->brect);
		b->r.max.x = def.font->height & ~1;
		if(b->text && strlen(b->text))
			b->r.max.x += textwidth(def.font, b->text);
		width += Dx(b->r);
	}

	if(width > Dx(s->brect)) { /* Not enough room. Shrink bars until they all fit. */
		foreach_bar(s, b) {
			for(pb=&largest; *pb; pb=&pb[0]->smaller)
				if(Dx(pb[0]->r) < Dx(b->r))
					break; 
			b->smaller = *pb;
			*pb = b;
		}
		SET(shrink);
		for(tb=largest; tb; tb=tb->smaller) {
			width -= Dx(tb->r);
			tw += Dx(tb->r);
			shrink = (Dx(s->brect) - width) / (float)tw;
			if(tb->smaller)
				if(Dx(tb->r) * shrink >= Dx(tb->smaller->r))
					break;
		}
		if(tb)
			for(b=largest; b != tb->smaller; b=b->smaller)
				b->r.max.x *= shrink;
		width += tw * shrink;
	}

	tb = nil;
	foreach_bar(s, b) {
		if(tb)
			b->r = rectaddpt(b->r, Pt(tb->r.max.x, 0));
		if(b == s->bar[BRight])
			b->r.max.x += Dx(s->brect) - width;
		tb = b;
	}

	r = rectsubpt(s->brect, s->brect.min);
	fill(screen->ibuf, r, def.normcolor.bg);
	foreach_bar(s, b) {
		align = Center;
		if(b == s->bar[BRight])
			align = East;
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
		if(!strcmp(b->name, name))
			break;
	return b;
}

static char *barside[] = {
	[BLeft]  = "Left",
	[BRight] = "Right",
};

static Bar*
findbar(WMScreen *s, Point p) {
	Bar *b;

	foreach_bar(s, b)
		if(rect_haspoint_p(p, b->r))
			return b;
	return nil;
}

static void
bdown_event(Window *w, XButtonPressedEvent *e) {
	WMScreen *s;
	Bar *b;

	/* Ungrab so a menu can receive events before the button is released */
	XUngrabPointer(display, e->time);
	sync();

	s = w->aux;
	b = findbar(s, Pt(e->x, e->y));
	if(b)
		event("%sBarMouseDown %d %s\n", barside[b->bar], e->button, b->name);
}

static void
bup_event(Window *w, XButtonPressedEvent *e) {
	WMScreen *s;
	Bar *b;
	
	s = w->aux;
	b = findbar(s, Pt(e->x, e->y));
	if(b)
		event("%sBarClick %d %s\n", barside[b->bar], e->button, b->name);
}

static Rectangle
dndmotion_event(Window *w, Point p) {
	WMScreen *s;
	Bar *b;

	s = w->aux;
	b = findbar(s, p);
	if(b) {
		event("%sBarDND 1 %s\n", barside[b->bar], b->name);
		return b->r;
	}
	return ZR;
}

static void
expose_event(Window *w, XExposeEvent *e) {
	USED(w, e);
	bar_draw(screen);
}

static Handlers handlers = {
	.bdown = bdown_event,
	.bup = bup_event,
	.dndmotion = dndmotion_event,
	.expose = expose_event,
};

