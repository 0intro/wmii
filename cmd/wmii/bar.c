/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
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

	if(s->barwin && (s->barwin->depth == 32) == s->barwin_rgba)
		return;

	s->brect = s->r;
	s->brect.min.y = s->brect.max.y - labelh(def.font);

	wa.override_redirect = 1;
	wa.event_mask = ExposureMask
		      | ButtonPressMask
		      | ButtonReleaseMask
		      | FocusChangeMask;
	if(s->barwin_rgba)
		s->barwin = createwindow_rgba(&scr.root, s->brect,
				&wa, CWOverrideRedirect
				   | CWEventMask);
	else
		s->barwin = createwindow(&scr.root, s->brect, scr.depth, InputOutput,
				&wa, CWOverrideRedirect
				   | CWEventMask);
	s->barwin->aux = s;
	xdnd_initwindow(s->barwin);
	sethandler(s->barwin, &handlers);
	if(s == screens[0])
		mapwin(s->barwin);
}

void
bar_resize(WMScreen *s) {

	s->brect = s->r;
	s->brect.min.y = s->r.max.y - labelh(def.font);
	if(s == screens[0])
		reshapewin(s->barwin, s->brect);
	else
		s->brect.min.y = s->r.max.y;
	/* FIXME: view_arrange. */
}

void
bar_setbounds(WMScreen *s, int left, int right) {
	Rectangle *r;

	if(s != screens[0])
		return;

	r = &s->brect;
	r->min.x = left;
	r->max.x = right;
	if(Dy(*r))
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
	if(Dy(*r))
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
	b->colors = def.normcolor;

	strlcat(b->buf, b->colors.colstr, sizeof b->buf);
	strlcat(b->buf, " ", sizeof b->buf);
	strlcat(b->buf, b->text, sizeof b->buf);

	SET(i);
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
	Image *ibuf;
	Rectangle r;
	Align align;
	uint width, tw;
	float shrink;

	/* To do: Generalize this. */

	largest = nil;
	width = 0;
	s->barwin_rgba = false;
	foreach_bar(s, b) {
		b->r.min = ZP;
		b->r.max.y = Dy(s->brect);
		b->r.max.x = (def.font->height & ~1) + def.font->pad.min.x + def.font->pad.max.x;
		if(b->text && strlen(b->text))
			b->r.max.x += textwidth(def.font, b->text);
		width += Dx(b->r);
		s->barwin_rgba += RGBA_P(b->colors);
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

	if(s->bar[BRight])
		s->bar[BRight]->r.max.x += Dx(s->brect) - width;
	tb = nil;
	foreach_bar(s, b) {
		if(tb)
			b->r = rectaddpt(b->r, Pt(tb->r.max.x, 0));
		tb = b;
	}

	ibuf = s->barwin_rgba ? disp.ibuf32 : disp.ibuf;

	r = rectsubpt(s->brect, s->brect.min);
	fill(ibuf, r, &def.normcolor.bg);
	border(ibuf, r, 1, &def.normcolor.border);
	foreach_bar(s, b) {
		align = Center;
		if(b == s->bar[BRight])
			align = East;
		fill(ibuf, b->r, &b->colors.bg);
		drawstring(ibuf, def.font, b->r, align, b->text, &b->colors.fg);
		border(ibuf, b->r, 1, &b->colors.border);
	}

	if(s->barwin_rgba != (s->barwin->depth == 32))
		bar_init(s);
	copyimage(s->barwin, r, ibuf, ZP);
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
		if(rect_haspoint_p(b->r, p))
			return b;
	return nil;
}

static bool
bdown_event(Window *w, void *aux, XButtonPressedEvent *e) {
	WMScreen *s;
	Bar *b;

	/* Ungrab so a menu can receive events before the button is released */
	XUngrabPointer(display, e->time);
	sync();

	s = aux;
	b = findbar(s, Pt(e->x, e->y));
	if(b)
		event("%sBarMouseDown %d %s\n", barside[b->bar], e->button, b->name);
	return false;
}

static bool
bup_event(Window *w, void *aux, XButtonPressedEvent *e) {
	WMScreen *s;
	Bar *b;

	s = aux;
	b = findbar(s, Pt(e->x, e->y));
	if(b)
		event("%sBarClick %d %s\n", barside[b->bar], e->button, b->name);
	return false;
}

static Rectangle
dndmotion_event(Window *w, void *aux, Point p) {
	WMScreen *s;
	Bar *b;

	s = aux;
	b = findbar(s, p);
	if(b) {
		event("%sBarDND 1 %s\n", barside[b->bar], b->name);
		return b->r;
	}
	return ZR;
}

static bool
expose_event(Window *w, void *aux, XExposeEvent *e) {
	USED(w, e);
	bar_draw(aux);
	return false;
}

static Handlers handlers = {
	.bdown = bdown_event,
	.bup = bup_event,
	.dndmotion = dndmotion_event,
	.expose = expose_event,
};

