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
	}
	else
		b = cext_emallocz(sizeof(Bar));

	b->id = id++;
	cext_strlcpy(b->name, name, sizeof(b->name));
	b->brush = bbrush;
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

unsigned int
height_of_bar()
{
	enum { BAR_PADDING = 4 };
	return def.font.ascent + def.font.descent + BAR_PADDING;
}

void
resize_bar()
{
	View *v;
	Area *a;
	Frame *f;

	brect = rect;
	brect.height = height_of_bar();
	brect.y = rect.height - brect.height;
	XMoveResizeWindow(blz.display, barwin, brect.x, brect.y, brect.width, brect.height);
	XSync(blz.display, False);
	draw_bar();

	for(v=view; v; v=v->next) {
		for(a = v->area; a; a=a->next) {
			a->rect.height = rect.height - brect.height;
			arrange_column(a, False);
		}
		for(f=v->area->frame; f; f=f->anext) {
			resize_client(f->client, &f->rect, False);
		}
	}
}

void
draw_bar()
{
	unsigned int width, tw, nb, size;
	float shrink;
	Bar *b, *tb, *largest, **pb;

	blitz_draw_tile(&bbrush);

	if(!lbar && !rbar)
		goto MapBar;

	largest = b = tb = nil;
	tw = width = nb = size = 0;

	for(b=lbar, nb=2 ;nb; --nb && (b = rbar))
		for(; b; b=b->next) {
			b->brush.rect.x = b->brush.rect.y = 0;
			b->brush.rect.width = brect.height;
			if(b->text && strlen(b->text))
				b->brush.rect.width += blitz_textwidth(b->brush.font, b->text);
			b->brush.rect.height = brect.height;
			width += b->brush.rect.width;
		}

	/* Not enough room. Shrink bars until they all fit */
	if(width > brect.width) {
		for(b=lbar, nb=2 ;nb; --nb && (b = rbar))
			for(; b; b=b->next) {
				for(pb=&largest; *pb; pb=&(*pb)->smaller)
					if((*pb)->brush.rect.width < b->brush.rect.width) break; 
				b->smaller = *pb;
				*pb = b;
			}
		for(tb=largest; tb; tb=tb->smaller) {
			width -= tb->brush.rect.width;
			tw += tb->brush.rect.width;
			shrink = (brect.width - width) / (float)tw;
			if(tb->smaller)
			if(tb->brush.rect.width * shrink >= tb->smaller->brush.rect.width)
				break;
		}
		for(b=largest; b != tb->smaller; b=b->smaller)
			b->brush.rect.width = floor(b->brush.rect.width * shrink);
		width += tw * shrink;
		tb=nil;
	}

	for(b=lbar, nb=2 ;nb; b=rbar, nb--)
		for(; b; tb = b, b=b->next) {
			if(b == rbar) {
				b->brush.align = EAST;
				rbar->brush.rect.width += (brect.width - width);
			}else
				b->brush.align = CENTER;
			if(tb)
				b->brush.rect.x = tb->brush.rect.x + tb->brush.rect.width;
			blitz_draw_label(&b->brush, b->text);
		}
MapBar:
	XCopyArea(blz.display, bbrush.drawable, barwin, bbrush.gc, 0, 0,
			brect.width, brect.height, 0, 0);
	XSync(blz.display, False);
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
