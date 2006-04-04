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
	Label *ll1 = *(Label **)l1;
	Label *ll2 = *(Label **)l2;
	if(ll1->intern && !ll2->intern)
		return -1;
	if(!ll1->intern && ll2->intern)
		return 1;
	return 0;
}

static int
comp_label_name(const void *l1, const void *l2)
{
	Label *ll1 = *(Label **)l1;
	Label *ll2 = *(Label **)l2;
	return strcmp(ll1->name, ll2->name);
}

static Vector *
label2vector(LabelVector *lv)
{
	return (Vector *) lv;
}

Label *
get_label(char *name, Bool intern)
{
	static unsigned int id = 1;
	Label *l = name2label(name);

	if(l)
		return l;
	l = cext_emallocz(sizeof(Label));
	l->id = id++;
	l->intern = intern;
	cext_strlcpy(l->name, name, sizeof(l->name));
	cext_strlcpy(l->colstr, def.selcolor, sizeof(l->colstr));
	l->color = def.sel;
	cext_vattach(label2vector(&label), l);
	qsort(label.data, label.size, sizeof(Label *), comp_label_name);
	qsort(label.data, label.size, sizeof(Label *), comp_label_intern);

	return l;
}

void
destroy_label(Label *l)
{
	cext_vdetach(label2vector(&label), l);
}

unsigned int
bar_height()
{
	return xfont->ascent + xfont->descent + 4;
}

void
update_bar_geometry()
{
	unsigned int i, j;
	brect = rect;
	brect.height = bar_height();
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
			arrange_column(a);
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
	Label *l = nil;

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
			if(view.size && !strncmp(l->name, def.tag, sizeof(l->name)))
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
label2index(Label *l)
{
	int i;
	for(i = 0; i < label.size; i++)
		if(label.data[i] == l)
			return i;
	return -1;
}

int
lid2index(unsigned short id)
{
	int i;
	for(i = 0; i < label.size; i++)
		if(label.data[i]->id == id)
			return i;
	return -1;
}

Label *
name2label(const char *name)
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
update_bar_tags()
{
	unsigned int i;
	Label *l = nil;

	for(i = 0; (i < label.size) && label.data[i]->intern; i++) {
		l = label.data[i];
		if(!istag(l->name) && !name2view(l->name)) {
			destroy_label(l);
			i--;
		}
	}
	for(i = 0; i < tag.size; i++) {
		l = get_label(tag.data[i], True);
		cext_strlcpy(l->data, tag.data[i], sizeof(l->data));
	}

	draw_bar();
}
