/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <stdlib.h>

#include "wm.h"

Bar *free_bars = nil;

Bar *
create_bar(char *name)
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
	cext_strlcpy(b->colstr, def.normcolor, sizeof(b->colstr));
	b->color = def.norm;

	for(i=&bar; *i && (strcmp((*i)->name, name) < 0); i=&(*i)->next);
	b->next = *i;
	*i = b;
	
	return b;
}

void
destroy_bar(Bar *b)
{
	Bar **i;
	for(i=&bar; *i && *i != b; i=&(*i)->next);
	*i = b->next;

	b->next = free_bars;
	free_bars = b;
}

unsigned int
height_of_bar()
{
	enum { BAR_PADDING = 4 };
	return blitzfont.ascent + blitzfont.descent + BAR_PADDING;
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
	unsigned int i = 0, w = 0, size = 0;
	Bar *exp = nil;
	BlitzDraw d = { 0 };
	Bar *b = nil;

	d.gc = bargc;
	d.drawable = barpmap;
	d.rect = brect;
	d.rect.x = d.rect.y = 0;
	d.font = blitzfont;

	d.color = def.norm;
	blitz_drawlabel(dpy, &d);
	blitz_drawborder(dpy, &d);

	if(!bar)
		goto MapBar;

	for(b=bar; b && (w < brect.width); b=b->next, size++) {
		b->rect.x = 0;
		b->rect.y = 0;
		b->rect.width = brect.height;
		if(strlen(b->data))
			b->rect.width += blitz_textwidth(dpy, &blitzfont, b->data);
		b->rect.height = brect.height;
		w += b->rect.width;
	}

	if(b) { /* give all bars same width */
		for(; b; b=b->next, size++);
		w = brect.width / size;
		for(b=bar; b; b=b->next) {
			b->rect.x = i * w;
			b->rect.width = w;
		}
	}
	else { /* expand bar properly */
		for(exp = bar; exp && exp->next; exp=exp->next);
		if(exp)
			exp->rect.width += (brect.width - w);
		for(b=bar; b->next; b=b->next)
			b->next->rect.x = b->rect.x + b->rect.width;
	}

	for(b=bar; b; b=b->next) {
		d.color = b->color;
		d.rect = b->rect;
		d.data = b->data;
		if(b == exp)
			d.align = EAST;
		else
			d.align = CENTER;
		blitz_drawlabel(dpy, &d);
		blitz_drawborder(dpy, &d);
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
	for(b=bar; b && strncmp(b->name, name, sizeof(b->name)); b=b->next);
	return b;
}

Bar *
bar_of_id(unsigned short id)
{
	Bar *b;
	for(b=bar; b && b->id != id; b=b->next);
	return b;
}
