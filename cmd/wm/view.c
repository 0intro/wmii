/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

Vector *
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
	cext_strlcpy(v->name, name, sizeof(v->name));
	alloc_area(v);
	alloc_area(v);
	sel = view.size;
	cext_vattach(view2vector(&view), v);
	return v;
}

void
destroy_view(View *v)
{
	fprintf(stderr, "destroy_view: %s\n", v->name);
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

	XGrabServer(dpy);
	sel = view2index(v);

	update_frame_selectors(v);

	/* gives all(!) clients proper geometry (for use of different tags) */
	if((c = sel_client_of_view(v)))
		focus_client(c);
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
	unsigned int i;

	for(i = 0; i < view.size; i++)
		if(!strncmp(view.data[i]->name, name, strlen(name)))
			return view.data[i];
	return nil;
}

View *
get_view(char *name)
{
	View *v = name2view(name);
	return v ? v : alloc_view(name);
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
	View *v = name2view(arg);
	if(!v)
		return;
	focus_view(v);
}

Bool
clientofview(View *v, Client *c)
{
	unsigned int i;
	for(i = 0; i < c->view.size; i++)
		if(v == c->view.data[i])
			return True;
	return False;
}

void
detach_fromview(View *v, Client *c)
{
	unsigned int i;

	fprintf(stderr, "detach_fromview: %s\n", c->name);
	cext_vdetach(view2vector(&c->view), v);
	for(i = 0; i < v->area.size; i++) {
		if(clientofarea(v->area.data[i], c)) {
			detach_fromarea(v->area.data[i], c);
			XMoveWindow(dpy, c->framewin, 2 * rect.width, 0);
		}
	}
}

void
attach_toview(View *v, Client *c)
{
	Area *a;

	fprintf(stderr, "attach_toview: %s\n", c->name);

	if(c->trans || c->floating)
		a = v->area.data[0];
	else
		a = v->area.data[v->sel];

	attach_toarea(a, c);
	map_client(c);
	XMapWindow(dpy, c->framewin);
	cext_vattach(view2vector(&c->view), v);
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
	unsigned int i, n = 0;
	int j;
	static Window *wins = nil;
	static unsigned int winssz = 0;

	if(client.size > winssz) {
		winssz = 2 * client.size;
		wins = realloc(wins, sizeof(Window) * winssz);
	}

	for(i = 0; i < v->area.size; i++) {
		Area *a = v->area.data[i];
		if(a->frame.size) {
			wins[n++] = a->frame.data[a->sel]->client->framewin;
			for(j = a->frame.size - 1; j >= 0; j--) {
				Client *c = a->frame.data[j]->client;
				if((v->sel == i) && (a->sel == j)) {
					ungrab_mouse(c->framewin, AnyModifier, AnyButton);
					grab_mouse(c->framewin, Mod1Mask, Button1);
					grab_mouse(c->framewin, Mod1Mask, Button3);
				}
				else
					grab_mouse(c->framewin, AnyModifier, Button1);
				if(j == a->sel)
					continue;
				wins[n++] = c->framewin;
			}
		}
	}

	if(n)
		XRestackWindows(dpy, wins, n);
}

void
arrange_view(View *v, Bool dirty)
{
	unsigned int i, xoff = 0;
	unsigned int dx = 0;
	float scale = 1.0;

	if(v->area.size == 1)
		return;

	if(dirty) {
		for(i = 1; i < v->area.size; i++)
			dx += v->area.data[i]->rect.width;
		scale = (float)rect.width / (float)dx;
	}
	for(i = 1; i < v->area.size; i++) {
		Area *a = v->area.data[i];
		if(dirty) {
			a->rect.x = xoff;
			a->rect.y = 0;
			a->rect.height = rect.height - brect.height;
			a->rect.width *= scale;
			xoff += a->rect.width;
		}
		arrange_column(a, False);
	}
}
