/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

static Vector *
view2vector(ViewVector *vv)
{
	return (Vector *) vv;
}

View *
alloc_view(char *name)
{
	static unsigned short id = 1;
	View *v = cext_emallocz(sizeof(View));

	v->id = id++;
	v->ntag = str2tags(v->tag, name);
	alloc_area(v);
	alloc_area(v);
	sel = view.size;
	cext_vattach(view2vector(&view), v);
	return v;
}

void
destroy_view(View *v)
{
	while(v->area.size)
		destroy_area(v->area.data[0]);

	cext_vdetach(view2vector(&view), v);
	if(sel >= view.size)
		sel = 0;

	free(v);
}

int
view2index(View *v)
{
	int i;
	for(i = 0; i < view.size; i++)
		if(v == view.data[i])
			return i;
	return -1;
}

static void
update_frame_selectors(View *v)
{
	unsigned int i, j;

	/* select correct frames of clients */
	for(i = 0; i < client.size; i++)
		for(j = 0; j < client.data[i]->frame.size; j++)
			if(client.data[i]->frame.data[j]->area->view == v)
				client.data[i]->sel = j;
}

void
focus_view(View *v)
{
	Client *c;
	unsigned int i;
	char vname[256];

	/* cleanup other empty views */
	for(i = 0; i < view.size; i++)
		if(!hasclient(view.data[i])) {
			destroy_view(view.data[i]);
			i--;
		}

	XGrabServer(dpy);
	sel = view2index(v);

	tags2str(vname, sizeof(vname), v->tag, v->ntag);
	cext_strlcpy(def.tag, vname, sizeof(def.tag));

	update_frame_selectors(v);

	/* gives all(!) clients proper geometry (for use of different tags) */
	for(i = 0; i < client.size; i++)
		if(client.data[i]->frame.size) {
			Frame *f = client.data[i]->frame.data[client.data[i]->sel];
			if(f->area->view == v) {
				XMoveWindow(dpy, client.data[i]->framewin, f->rect.x, f->rect.y);
				if(client.data[i]->frame.size > 1)
					resize_client(client.data[i], &f->rect, False);
				draw_client(client.data[i]);
			}
			else
				XMoveWindow(dpy, client.data[i]->framewin,
						2 * rect.width + f->rect.x, f->rect.y);
		}
	if((c = sel_client_of_view(v)))
		focus_client(c);
	update_bar_tags();
	XSync(dpy, False);
	XUngrabServer(dpy);
}

XRectangle *
rectangles(View *v, Bool isfloat, unsigned int *num)
{
	XRectangle *result = nil;
	unsigned int i;

	*num = 0;
	if(isfloat)
		*num = v->area.data[0]->frame.size;
	else {
		for(i = 1; i < v->area.size; i++)
			*num += v->area.data[i]->frame.size;
	}

	if(*num) {
		result = cext_emallocz(*num * sizeof(XRectangle));
		if(isfloat) {
			for(i = 0; i < *num; i++)
				result[i] = v->area.data[0]->frame.data[0]->rect;
		}
		else {
			unsigned int j, n = 0;
			for(i = 1; i < v->area.size; i++) {
				for(j = 0; j < v->area.data[i]->frame.size; j++)
					result[n++] = v->area.data[i]->frame.data[j]->rect;
			}
		}
	}
	return result;
}

int
vid2index(unsigned short id)
{
	int i;
	for(i = 0; i < view.size; i++)
		if(view.data[i]->id == id)
			return i;
	return -1;
}

View *
name2view(char *name)
{
	View *v = nil;
	char vname[256];
	unsigned int i;

	for(i = 0; i < view.size; i++) {
		v = view.data[i];
		tags2str(vname, sizeof(vname), v->tag, v->ntag);
		if(!strncmp(vname, name, strlen(name)))
			return v;
	}

	return nil;
}

View *
get_view(char *name)
{
	unsigned int i, j, ntags;
	View *v = name2view(name);
	char tags[MAX_TAGS][MAX_TAGLEN];

	if(v)
		return v;

	ntags = str2tags(tags, name);
	for(i = 0; i < client.size; i++)
		for(j = 0; j < ntags; j++)
			if(clienthastag(client.data[i], tags[j]))
				goto Createview;
	return nil;

Createview:
	v = alloc_view(name);
	for(i = 0; i < client.size; i++)
		for(j = 0; j < ntags; j++)
			if(clienthastag(client.data[i], tags[j]) && !clientofview(v, client.data[i]))
				attach_toview(v, client.data[i]);
	return v;
}

Bool
hasclient(View *v)
{
	unsigned int i;
	for(i = 0; i < v->area.size; i++)
		if(v->area.data[i]->frame.size)
			return True;
	return False;
}

void
select_view(char *arg)
{
	View *v = get_view(arg);
	if(!v)
		return;
	ensure_tag(arg);
	focus_view(v);
}

Bool
clientofview(View *v, Client *c)
{
	unsigned int i;
	for(i = 0; i < v->area.size; i++)
		if(clientofarea(v->area.data[i], c))
			return True;
	return False;
}

void
detach_fromview(View *v, Client *c)
{
	int i;
	Client *cl;
	for(i = 0; i < v->area.size; i++) {
		if(clientofarea(v->area.data[i], c)) {
			detach_fromarea(v->area.data[i], c);
			XMoveWindow(dpy, c->framewin, 2 * rect.width, 0);
		}
	}
	if((cl = sel_client_of_view(v)))
		focus_client(cl);
}

void
attach_toview(View *v, Client *c)
{
	Area *a;

	if(c->trans || c->floating)
		a = v->area.data[0];
	else
		a = v->area.data[v->sel];

	attach_toarea(a, c);
	map_client(c);
	XMapWindow(dpy, c->framewin);
	if(v == view.data[sel])
		focus_client(c);
}

Client *
sel_client_of_view(View *v)
{
	if(v) {
		Area *a = v->area.size ? v->area.data[v->sel] : nil;
		return (a && a->frame.size) ? a->frame.data[a->sel]->client : nil;
	}
	return nil;
}

void
restack_view(View *v)
{
	unsigned int i, j, n = 0;
	static Window *wins = nil;
	static unsigned int winssz = 0;

	if(client.size > winssz) {
		winssz = 2 * client.size;
		free(wins);
		wins = cext_emallocz(sizeof(Window) * winssz);
	}

	for(i = 0; i < v->area.size; i++) {
		Area *a = v->area.data[i];
		if(a->frame.size) {
			wins[n++] = a->frame.data[a->sel]->client->framewin;
			for(j = 0; j < a->frame.size; j++) {
				if(j == a->sel)
					continue;
				wins[n++] = a->frame.data[j]->client->framewin;
			}
		}
	}

	if(n)
		XRestackWindows(dpy, wins, n);
}

void
arrange_view(View *v, Bool updategeometry)
{
	unsigned int i;
	unsigned int width;

	if(v->area.size == 1)
		return;

	width = rect.width / (v->area.size - 1);
	for(i = 1; i < v->area.size; i++) {
		Area *a = v->area.data[i];
		if(updategeometry) {
			a->rect.height = rect.height - brect.height;
			a->rect.x = (i - 1) * width;
			a->rect.width = width;
		}
		arrange_column(a);
	}
}
