/* Copyright ©2006-2009 Kris Maglione <maglione.k at Gmail>
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

	if(s->barwin) {
		bar_resize(s);
		return;
	}

	s->brect = s->r;
	s->brect.min.y = s->brect.max.y - labelh(def.font);

	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask
		      | ButtonPressMask
		      | ButtonReleaseMask
		      | FocusChangeMask;
	s->barwin = createwindow(&scr.root, s->brect, scr.depth, InputOutput,
			&wa, CWOverrideRedirect
			   | CWBackPixmap
			   | CWEventMask);
	s->barwin->aux = s;
	xdnd_initwindow(s->barwin);
	sethandler(s->barwin, &handlers);
	mapwin(s->barwin);
}

void
bar_resize(WMScreen *s) {

	s->brect = s->r;
	s->brect.min.y = s->r.max.y - labelh(def.font);
	reshapewin(s->barwin, s->brect);
	/* FIXME: view_arrange. */
}

void
bar_setbounds(WMScreen *s, int left, int right) {
	Rectangle *r;

	r = &s->brect;
	r->min.x = left;
	r->max.x = right;
	reshapewin(s->barwin, *r);
}

void
bar_sety(WMScreen *s, int y) {
	Rectangle *r;
	int dy;

	r = &s->brect;

	dy = Dy(*r);
	r->min.y = y;
	r->max.y = y + dy;
	reshapewin(s->barwin, *r);
}

Bar*
bar_create(Bar **bp, const char *name) {
	static uint id = 1;
	WMScreen *s, **sp;
	Bar *b;
	uint i;

	b = bar_find(*bp, name);;
	if(b)
		return b;

	b = emallocz(sizeof *b);
	b->id = id++;
	utflcpy(b->name, name, sizeof b->name);
	b->col = def.normcolor;

	strlcat(b->buf, b->col.colstr, sizeof(b->buf));
	strlcat(b->buf, " ", sizeof(b->buf));
	strlcat(b->buf, b->text, sizeof(b->buf));
	
	for(sp=screens; (s = *sp); sp++) {
		i = bp - s->bar;
		if(i < nelem(s->bar))
			break;
	}
	b->bar = i;
	b->screen = s;

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
	width = 0;
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
		tw = 0;
		for(tb=largest; tb; tb=tb->smaller) {
			width -= Dx(tb->r);
			tw += Dx(tb->r);
			shrink = (Dx(s->brect) - width) / (float)tw;
			if(tb->smaller && Dx(tb->r) * shrink < Dx(tb->smaller->r))
				continue;
			if(width + (int)(tw * shrink) <= Dx(s->brect))
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
	fill(disp.ibuf, r, def.normcolor.bg);
	border(disp.ibuf, r, 1, def.normcolor.border);
	foreach_bar(s, b) {
		align = Center;
		if(b == s->bar[BRight])
			align = East;
		fill(disp.ibuf, b->r, b->col.bg);
		drawstring(disp.ibuf, def.font, b->r, align, b->text, b->col.fg);
		border(disp.ibuf, b->r, 1, b->col.border);
	}
	copyimage(s->barwin, r, disp.ibuf, ZP);
}

Bar*
bar_find(Bar *bp, const char *name) {
	Bar *b;

	for(b=bp; b; b=b->next)
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
	bar_draw(w->aux);
}

static Handlers handlers = {
	.bdown = bdown_event,
	.bup = bup_event,
	.dndmotion = dndmotion_event,
	.expose = expose_event,
};

