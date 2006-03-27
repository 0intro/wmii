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
	qsort(label, nlabel, sizeof(Label *), comp_label_name);
	qsort(label, nlabel, sizeof(Label *), comp_label_intern);

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

	if(!nlabel)
		return;

	for(i = 0; (i < nlabel) && (w < brect.width); i++) {
		l = label[i];
		if(l->intern) {
			if(nview && !strncmp(l->name, def.tag, sizeof(l->name)))
				l->color = def.sel;
			else
				l->color = def.norm;
		}
		l->rect.x = 0;
		l->rect.y = 0;
		l->rect.width = brect.height;
		if(strlen(l->name))
			l->rect.width += XTextWidth(xfont, l->data, strlen(l->data));
		l->rect.height = brect.height;
		w += l->rect.width;
	}

	if(i != nlabel) { /* give all labels same width */
		w = brect.width / nlabel;
		for(i = 0; i < nlabel; i++) {
			l = label[i];
			l->rect.x = i * w;
			l->rect.width = w;
		}
	}
	else { /* expand label properly */
		for(exp = 0; (exp < nlabel) && (label[exp]->intern); exp++);
		if(exp == nlabel)
			exp = -1;
		else
			label[exp]->rect.width += (brect.width - w);
		for(i = 1; i < nlabel; i++)
			label[i]->rect.x = label[i - 1]->rect.x + label[i - 1]->rect.width;
	}

	for(i = 0; i < nlabel; i++) {
		l = label[i];
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
