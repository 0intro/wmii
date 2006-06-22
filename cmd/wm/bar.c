/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <stdlib.h>

#include "wm.h"

Bar *free_bars = nil;

Bar *
create_bar(Bar **b_link, char *name)
{
	static unsigned int id = 1;
	Bar **i, *b = bar_of_name(name);;
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
	b->widget = blitz_create_input(barpmap, bargc, &def.font);
	b->widget->color = def.normcolor;
	b->widget->align = CENTER;

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
	if(b->widget->text)
		free(b->widget->text);
	blitz_destroy_input(b->widget);
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
	XMoveResizeWindow(dpy, barwin, brect.x, brect.y, brect.width, brect.height);
	XSync(dpy, False);
	XFreePixmap(dpy, barpmap);
	barpmap = XCreatePixmap(dpy, barwin, brect.width, brect.height,
			DefaultDepth(dpy, screen));
	XSync(dpy, False);
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
	unsigned int i = 0, w = 0, nb, size = 0;
	Bar *b = nil, *prev = nil;
	blitz_draw_tile(bartile);

	if(!lbar && !rbar)
		goto MapBar;

	for(b=lbar, nb=2 ;nb; --nb && (b = rbar))
		for(; b && (w < brect.width); b=b->next, size++) {
			b->widget->rect.x = 0;
			b->widget->rect.y = 0;
			b->widget->rect.width = brect.height;
			if(b->widget->text && strlen(b->widget->text))
				b->widget->rect.width += blitz_textwidth(b->widget->font, b->widget->text);
			b->widget->rect.height = brect.height;
			w += b->widget->rect.width;
		}

	if(b) { /* give all bars same width */
		/* finish counting */
		for( ;nb; b = rbar, nb--)
			for(; b; b=b->next, size++);

		w = brect.width / size;
		for(b = lbar, nb=2 ;nb; b = rbar, nb--) {
			for(; b; b=b->next) {
				b->widget->rect.x = i * w;
				b->widget->rect.width = w;
			}
		}
	}
	else { /* expand rbar properly */
		if(rbar)
			rbar->widget->rect.width += (brect.width - w);
		for(b=lbar, nb=2 ;nb--; b = rbar)
			for(; b; prev = b, b=b->next)
				if(prev) b->widget->rect.x = prev->widget->rect.x + prev->widget->rect.width;
	}

	for(b=lbar, nb=2 ;nb; b=rbar, nb--)
		for(; b; b=b->next) {
			if(b == rbar)
				b->widget->align = EAST;
			blitz_draw_input(b->widget);
		}
MapBar:
	XCopyArea(dpy, barpmap, barwin, bargc, 0, 0, brect.width, brect.height, 0, 0);
	XSync(dpy, False);
}

Bar *
bar_of_name(const char *name)
{
	static char buf[256];
	Bar *b;

 	cext_strlcpy(buf, name, sizeof(buf));
	for(b=lbar; b && strncmp(b->name, name, sizeof(b->name)); b=b->next);
	return b;
}

Bar *
bar_of_id(unsigned short id)
{
	Bar *b;
	for(b=lbar; b && b->id != id; b=b->next);
	return b;
}
