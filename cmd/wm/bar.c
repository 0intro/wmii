/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <stdlib.h>

#include "wm.h"

static int
comp_bar(const void *b1, const void *b2)
{
	Bar *bb1 = *(Bar **)b1;
	Bar *bb2 = *(Bar **)b2;
	return strcmp(bb1->name, bb2->name);
}

static Vector *
vector_of_bars(BarVector *bv)
{
	return (Vector *) bv;
}

Bar *
create_bar(char *name, Bool intern)
{
	static unsigned int id = 1;
	Bar *b = bar_of_name(name);

	if(b)
		return b;
	b = cext_emallocz(sizeof(Bar));
	b->id = id++;
	cext_strlcpy(b->name, name, sizeof(b->name));
	cext_strlcpy(b->colstr, def.selcolor, sizeof(b->colstr));
	b->color = def.norm;
	cext_vattach(vector_of_bars(&bar), b);
	qsort(bar.data, bar.size, sizeof(Bar *), comp_bar);
	return b;
}

void
destroy_bar(Bar *b)
{
	cext_vdetach(vector_of_bars(&bar), b);
	free(b);
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
	unsigned int i, j;
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
	for(i = 0; i < view.size; i++) {
		for(j = 1; j < view.data[i]->area.size; j++) {
			Area *a = view.data[i]->area.data[j];
			a->rect.height = rect.height - brect.height;
			arrange_column(a, False);
		}
		for(j = 0; j < view.data[i]->area.data[0]->frame.size; j++) {
			Frame *f = view.data[i]->area.data[0]->frame.data[j];
			resize_client(f->client, &f->rect, False);
		}
	}
}

void
draw_bar()
{
	unsigned int i = 0, w = 0;
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

	if(!bar.size)
		goto MapBar;

	for(i = 0; (i < bar.size) && (w < brect.width); i++) {
		b = bar.data[i];
		b->rect.x = 0;
		b->rect.y = 0;
		b->rect.width = brect.height;
		if(strlen(b->data))
			b->rect.width += blitz_textwidth(dpy, &blitzfont, b->data);
		b->rect.height = brect.height;
		w += b->rect.width;
	}

	if(i != bar.size) { /* give all bars same width */
		w = brect.width / bar.size;
		for(i = 0; i < bar.size; i++) {
			b = bar.data[i];
			b->rect.x = i * w;
			b->rect.width = w;
		}
	}
	else { /* expand bar properly */
		bar.data[bar.size - 1]->rect.width += (brect.width - w);
		for(i = 1; i < bar.size; i++)
			bar.data[i]->rect.x = bar.data[i - 1]->rect.x + bar.data[i - 1]->rect.width;
	}

	for(i = 0; i < bar.size; i++) {
		b = bar.data[i];
		d.color = b->color;
		d.rect = b->rect;
		d.data = b->data;
		if(i == bar.size - 1)
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

int
idx_of_bar(Bar *b)
{
	int i;
	for(i = 0; i < bar.size; i++)
		if(bar.data[i] == b)
			return i;
	return -1;
}

int
idx_of_bar_id(unsigned short id)
{
	int i;
	for(i = 0; i < bar.size; i++)
		if(bar.data[i]->id == id)
			return i;
	return -1;
}

Bar *
bar_of_name(const char *name)
{
	static char buf[256];
	unsigned int i;

 	cext_strlcpy(buf, name, sizeof(buf));
	for(i = 0; i < bar.size; i++)
		if(!strncmp(bar.data[i]->name, name, sizeof(bar.data[i]->name)))
			return bar.data[i];
	return nil;
}
