/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "wm.h"

Bar *free_bars = nil;

Bar *
create_bar(Bar **b_link, char *name)
{
	static unsigned int id = 1;
	Bar **i, *b = bar_of_name(*b_link, name);;
	if(b)
		return b;

	if(free_bars) {
		b = free_bars;
		free_bars = b->next;
		memset(b, 0, sizeof(*b));
	}
	else
		b = cext_emallocz(sizeof(Bar));

	b->id = id++;
	cext_strlcpy(b->name, name, sizeof(b->name));
	b->brush = screen->bbrush;
	b->brush.color = def.normcolor;

	for(i=b_link; *i; i=&(*i)->next)
		if(strcmp((*i)->name, name) >= 0)
			break;
	b->next = *i;
	*i = b;
	
	return b;
}

void
destroy_bar(Bar **b_link, Bar *b)
{
	Bar **p;
	for(p=b_link; *p && *p != b; p=&(*p)->next);
	*p = b->next;

	b->next = free_bars;
	free_bars = b;
}

void
resize_bar(WMScreen *s)
{
	View *v;

	s->brect = s->rect;
	s->brect.height = blitz_labelh(&def.font);
	s->brect.y = s->rect.height - s->brect.height;
	XMoveResizeWindow(blz.dpy, s->barwin, s->brect.x, s->brect.y, s->brect.width, s->brect.height);
	XSync(blz.dpy, False);
	draw_bar(s);

	for(v=view; v; v=v->next)
		arrange_view(v);
}

void
draw_bar(WMScreen *s)
{
	unsigned int width, tw, nb, size;
	float shrink;
	Bar *b, *tb, *largest, **pb;

	blitz_draw_tile(&s->bbrush);

	if(!s->lbar && !s->rbar)
		goto MapBar;

	largest = b = tb = nil;
	tw = width = nb = size = 0;

	for(b=s->lbar, nb=2 ;nb; --nb && (b = s->rbar))
		for(; b; b=b->next) {
			b->brush.rect.x = b->brush.rect.y = 0;
			b->brush.rect.width = def.font.height;
			if(b->text && strlen(b->text))
				b->brush.rect.width += blitz_textwidth(b->brush.font, b->text);
			b->brush.rect.height = s->brect.height;
			width += b->brush.rect.width;
		}

	/* Not enough room. Shrink bars until they all fit */
	if(width > s->brect.width) {
		for(b=s->lbar, nb=2 ;nb; --nb && (b = s->rbar))
			for(; b; b=b->next) {
				for(pb=&largest; *pb; pb=&(*pb)->smaller)
					if((*pb)->brush.rect.width < b->brush.rect.width) break; 
				b->smaller = *pb;
				*pb = b;
			}
		for(tb=largest; tb; tb=tb->smaller) {
			width -= tb->brush.rect.width;
			tw += tb->brush.rect.width;
			shrink = (s->brect.width - width) / (float)tw;
			if(tb->smaller)
			if(tb->brush.rect.width * shrink >= tb->smaller->brush.rect.width)
				break;
		}
		if(tb)
		for(b=largest; b != tb->smaller; b=b->smaller)
			b->brush.rect.width = floor(b->brush.rect.width * shrink);
		width += tw * shrink;
		tb=nil;
	}

	for(b=s->lbar, nb=2 ;nb; b=s->rbar, nb--)
		for(; b; tb = b, b=b->next) {
			if(b == s->rbar) {
				b->brush.align = EAST;
				s->rbar->brush.rect.width += (s->brect.width - width);
			}else
				b->brush.align = CENTER;
			if(tb)
				b->brush.rect.x = tb->brush.rect.x + tb->brush.rect.width;
			blitz_draw_label(&b->brush, b->text);
		}
MapBar:
	XCopyArea(blz.dpy, s->bbrush.drawable, s->barwin, s->bbrush.gc, 0, 0,
			s->brect.width, s->brect.height, 0, 0);
	XSync(blz.dpy, False);
}

Bar *
bar_of_name(Bar *b_link, const char *name)
{
	static char buf[256];
	Bar *b;

 	cext_strlcpy(buf, name, sizeof(buf));
	for(b=b_link; b; b=b->next)
		if(!strncmp(b->name, name, sizeof(b->name))) break;
	return b;
}
