/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <stdlib.h>

#include "wm.h"

static int
comp_label_intern(const void *l1, const void *l2)
{
	Bar *ll1 = *(Bar **)l1;
	Bar *ll2 = *(Bar **)l2;
	if(ll1->intern && !ll2->intern)
		return -1;
	if(!ll1->intern && ll2->intern)
		return 1;
	return 0;
}

static int
comp_label_name(const void *l1, const void *l2)
{
	Bar *ll1 = *(Bar **)l1;
	Bar *ll2 = *(Bar **)l2;
	return strcmp(ll1->name, ll2->name);
}

static Vector *
vector_of_bars(BarVector *lv)
{
	return (Vector *) lv;
}

Bar *
create_bar(char *name, Bool intern)
{
	static unsigned int id = 1;
	Bar *l = bar_of_name(name);

	if(l)
		return l;
	l = cext_emallocz(sizeof(Bar));
	l->id = id++;
	l->intern = intern;
	cext_strlcpy(l->name, name, sizeof(l->name));
	cext_strlcpy(l->colstr, def.selcolor, sizeof(l->colstr));
	l->color = def.sel;
	cext_vattach(vector_of_bars(&label), l);
	qsort(label.data, label.size, sizeof(Bar *), comp_label_name);
	qsort(label.data, label.size, sizeof(Bar *), comp_label_intern);

	return l;
}

void
destroy_bar(Bar *l)
{
	cext_vdetach(vector_of_bars(&label), l);
}

unsigned int
height_of_bar()
{
	enum { BAR_PADDING = 4 };
	return xfont->ascent + xfont->descent + BAR_PADDING;
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
	int exp = -1;
	Draw d = { 0 };
	Bar *l = nil;

	d.gc = bargc;
	d.drawable = barpmap;
	d.rect = brect;
	d.rect.x = d.rect.y = 0;
	d.font = xfont;

	d.color = def.norm;
	blitz_drawlabel(dpy, &d);
	blitz_drawborder(dpy, &d);

	if(!label.size)
		return;

	for(i = 0; (i < label.size) && (w < brect.width); i++) {
		l = label.data[i];
		if(l->intern) {
			if(view.size && !strncmp(l->name, view.data[sel]->name, sizeof(l->name)))
				l->color = def.sel;
			else
				l->color = def.norm;
		}
		l->rect.x = 0;
		l->rect.y = 0;
		l->rect.width = brect.height;
		if(strlen(l->data))
			l->rect.width += XTextWidth(xfont, l->data, strlen(l->data));
		l->rect.height = brect.height;
		w += l->rect.width;
	}

	if(i != label.size) { /* give all labels same width */
		w = brect.width / label.size;
		for(i = 0; i < label.size; i++) {
			l = label.data[i];
			l->rect.x = i * w;
			l->rect.width = w;
		}
	}
	else { /* expand label properly */
		for(exp = 0; (exp < label.size) && (label.data[exp]->intern); exp++);
		if(exp == label.size)
			exp = -1;
		else
			label.data[exp]->rect.width += (brect.width - w);
		for(i = 1; i < label.size; i++)
			label.data[i]->rect.x = label.data[i - 1]->rect.x + label.data[i - 1]->rect.width;
	}

	for(i = 0; i < label.size; i++) {
		l = label.data[i];
		d.color = l->color;
		d.rect = l->rect;
		d.data = l->data;
		if(i == exp)
			d.align = EAST;
		else
			d.align = CENTER;
		blitz_drawlabel(dpy, &d);
		blitz_drawborder(dpy, &d);
	}
	XCopyArea(dpy, barpmap, barwin, bargc, 0, 0, brect.width, brect.height, 0, 0);
	XSync(dpy, False);
}

int
idx_of_bar(Bar *l)
{
	int i;
	for(i = 0; i < label.size; i++)
		if(label.data[i] == l)
			return i;
	return -1;
}

int
idx_of_bar_id(unsigned short id)
{
	int i;
	for(i = 0; i < label.size; i++)
		if(label.data[i]->id == id)
			return i;
	return -1;
}

Bar *
bar_of_name(const char *name)
{
	static char buf[256];
	unsigned int i;

 	cext_strlcpy(buf, name, sizeof(buf));
	for(i = 0; i < label.size; i++)
		if(!strncmp(label.data[i]->name, name, sizeof(label.data[i]->name)))
			return label.data[i];
	return nil;
}

void
update_view_bars()
{
	unsigned int i;
	Bar *l = nil;

	for(i = 0; (i < label.size) && label.data[i]->intern; i++) {
		l = label.data[i];
		if(!view_of_name(l->name)) {
			destroy_bar(l);
			i--;
		}
	}
	for(i = 0; i < view.size; i++) {
		l = create_bar(view.data[i]->name, True);
		cext_strlcpy(l->data, view.data[i]->name, sizeof(l->data));
	}

	draw_bar();
}
