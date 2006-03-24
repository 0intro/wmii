/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <string.h>
#include <stdlib.h>

#include "wm.h"

static int
comp_label(const void *l1, const void *l2)
{
	Label *ll1 = *(Label **)l1;
	Label *ll2 = *(Label **)l2;
	return strcmp(ll1->name, ll2->name);
}

Label *
get_label(char *name)
{
	static unsigned int id = 1;
	Label *l = name2label(name);

	if(l)
		return l;
	l = cext_emallocz(sizeof(Label));
	l->id = id++;
	cext_strlcpy(l->name, name, sizeof(l->name));
	cext_strlcpy(l->colstr, def.selcolor, sizeof(l->colstr));
	l->color = def.sel;
	label = (Label **)cext_array_attach((void **)label, l,
			sizeof(Label *), &labelsz);
	nlabel++;
	qsort(label, nlabel, sizeof(Label *), comp_label);

	return l;
}

void
destroy_label(Label *l)
{
	cext_array_detach((void **)label, l, &labelsz);
	nlabel--;
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
	for(i = 0; i < nview; i++) {
		for(j = 1; j < view[i]->narea; j++) {
			Area *a = view[i]->area[j];
			a->rect.height = rect.height - brect.height;
			arrange_area(a);
		}
		for(j = 0; j < view[i]->area[0]->nframe; j++) {
			Frame *f = view[i]->area[0]->frame[j];
			resize_client(f->client, &f->rect, False);
		}
	}
}

void
draw_bar()
{
	unsigned int i = 0, n = 0, x = 0, w = 0;
	Draw d = { 0 };
	Label *l = nil;

	d.align = WEST;
	d.gc = bargc;
	d.drawable = barpmap;
	d.rect = brect;
	d.rect.x = d.rect.y = 0;
	d.font = xfont;

	d.color = def.norm;
	blitz_drawlabel(dpy, &d);
	blitz_drawborder(dpy, &d);

	if(!nlabel)
		return;
	for(i = 0; (i < nlabel) && label[i]->intern; i++) {
		l = label[i];
		if(nview && !strncmp(l->data, def.tag, sizeof(l->data)))
			l->color = def.sel;
		else
			l->color = def.norm;
		l->rect.y = 0;
		l->rect.x = x;
		l->rect.width = l->rect.height = brect.height;
		if(strlen(l->name))
			l->rect.width += XTextWidth(xfont, l->name, strlen(l->name));
		x += l->rect.width;
	}

	if(i)
		n = i;
	for(; i < nlabel; i++) {
		l = label[i];
		l->rect.x = x;
		l->rect.y = 0;
		l->rect.height = brect.height;
		l->rect.width = brect.height;
		if(strlen(l->data))
			l->rect.width += XTextWidth(xfont, l->data, strlen(l->data));
		w += l->rect.width;
	}

	if(w >= brect.width - x) {
		/* failsafe mode, give all labels same width */
		w = (brect.width - x) / (nlabel - n);
		for(i = n; i < nlabel; i++) {
			l = label[i];
			l->rect.x = x + i * w;
			l->rect.width = w;
		}
		i--;
		label[i]->rect.width = brect.width - label[i]->rect.x;
	}
	else {
		label[n]->rect.x = brect.width - w;
		for(i = n + 1; i < nlabel; i++)
			label[i]->rect.x = label[i - 1]->rect.x + label[i - 1]->rect.width;
	}

	for(i = 0; i < nlabel; i++) {
		l = label[i];
		d.color = l->color;
		d.rect = l->rect;
		d.data = l->data;
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
	for(i = 0; i < nlabel; i++)
		if(label[i] == l)
			return i;
	return -1;
}

int
lid2index(unsigned short id)
{
	int i;
	for(i = 0; i < nlabel; i++)
		if(label[i]->id == id)
			return i;
	return -1;
}

Label *
name2label(const char *name)
{
	char buf[256];
	unsigned int i;

 	cext_strlcpy(buf, name, sizeof(buf));
	for(i = 0; i < nlabel; i++)
		if(!strncmp(label[i]->name, name, sizeof(label[i]->name)))
			return label[i];
	return nil;
}

void
update_bar_tags()
{
	unsigned int i;
	char vname[256];
	Label *l = nil;

	for(i = 0; (i < nlabel) && label[i]->intern; i++) {
		l = label[i];
		if(!istag(l->name) && !name2view(l->name)) {
			destroy_label(l);
			i--;
		}
	}
	for(i = 0; i < ntag; i++) {
		l = get_label(tag[i]);
		l->intern = True;
		cext_strlcpy(l->data, tag[i], sizeof(l->data));
	}
	for(i = 0; i < nview; i++) {
		View *v = view[i];
		tags2str(vname, sizeof(vname), v->tag, v->ntag);
		l = get_label(vname);
		l->intern = True;
		cext_strlcpy(l->data, vname, sizeof(l->data));
	}
	draw_bar();
}
