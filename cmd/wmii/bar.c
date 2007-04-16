/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <stdio.h>
#include <string.h>
#include <util.h>
#include "dat.h"
#include "fns.h"

Bar *free_bars = nil;

Bar *
create_bar(Bar **bp, char *name) {
	static uint id = 1;
	Bar *b;

	b = bar_of_name(*bp, name);;
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
	strncpy(b->name, name, sizeof(b->name));
	b->col = def.normcolor;

	for(; *bp; bp = &(*bp)->next)
		if(strcmp((*bp)->name, name) >= 0)
			break;
	b->next = *bp;
	*bp = b;

	return b;
}

void
destroy_bar(Bar **bp, Bar *b) {
	Bar **p;

	for(p = bp; *p; p = &(*p)->next)
		if(*p == b) break;
	*p = b->next;

	b->next = free_bars;
	free_bars = b;
}

void
resize_bar(WMScreen *s) {
	View *v;

	s->brect = s->rect;
	s->brect.min.y = s->brect.max.y - labelh(def.font);

	reshapewin(s->barwin, s->brect);

	XSync(display, False);
	draw_bar(s);
	for(v = view; v; v = v->next)
		arrange_view(v);
}

void
draw_bar(WMScreen *s) {
	Bar *b, *tb, *largest, **pb;
	Rectangle r;
	Align align;
	uint width, tw, nb, size;
	float shrink;

	fill(screen->ibuf, s->brect, def.normcolor.bg);
	if(!s->bar[BarLeft] && !s->bar[BarRight])
		goto MapBar;

	largest = b = tb = nil;
	tw = width = nb = size = 0;
	for(nb = 0; nb < nelem(s->bar); nb++)
		for(b = s->bar[nb]; b; b=b->next) {
			b->r.min = ZP;
			b->r.max.y = Dy(s->brect);
			b->r.max.x = def.font->height & ~1;
			if(b->text && strlen(b->text))
				b->r.max.x += textwidth(def.font, b->text);

			width += Dx(b->r);
		}

	/* Not enough room. Shrink bars until they all fit */
	if(width > Dx(s->brect)) {
		for(nb = 0; nb < nelem(s->bar); nb++)
			for(b = s->bar[nb]; b; b=b->next) {
				for(pb = &largest; *pb; pb = &(*pb)->smaller)
					if(Dx((*pb)->r) < Dx(b->r))
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
		if(tb)
			for(b = largest; b != tb->smaller; b = b->smaller)
				b->r.max.x *= shrink;
		width += tw * shrink;
		tb = nil;
	}

	for(nb = 0; nb < nelem(s->bar); nb++)
		for(b = s->bar[nb]; b; tb=b, b=b->next) {
			if(b == s->bar[BarRight]) {
				align = EAST;
				b->r.max.x += Dx(s->brect) - width;
			}else
				align = CENTER;

			if(tb)
				b->r = rectaddpt(b->r, Pt( tb->r.max.x, 0));

			fill(screen->ibuf, b->r, b->col.bg);
			drawstring(screen->ibuf, def.font, b->r, align, b->text, b->col.fg);
			border(screen->ibuf, b->r, 1, b->col.border);
		}
MapBar:
	r = rectsubpt(s->brect, s->brect.min);
	copyimage(s->barwin, r, screen->ibuf, ZP);
	XSync(display, False);
}

Bar *
bar_of_name(Bar *bp, const char *name) {
	Bar *b;

	for(b = bp; b; b = b->next)
		if(!strncmp(b->name, name, sizeof(b->name)))
			break;
	return b;
}
