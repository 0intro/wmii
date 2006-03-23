/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

View *
alloc_tag(char *name)
{
	static unsigned short id = 1;
    View *v = cext_emallocz(sizeof(View));

	v->id = id++;
	v->ntag = str2tags(v->tag, name);
	alloc_area(v);
	alloc_area(v);
	view = (View **)cext_array_attach((void **)view, v, sizeof(View *), &viewsz);
	nview++;
	focus_tag(v);
    return v;
}

static void
destroy_tag(View *v)
{
	while(v->narea)
		destroy_area(v->area[0]);

	cext_array_detach((void **)view, v, &viewsz);
	nview--;
	if(sel >= nview)
		sel = 0;

    free(v); 
}

int
tag2index(View *v)
{
	int i;
	for(i = 0; i < nview; i++)
		if(v == view[i])
			return i;
	return -1;
}

static void
update_frame_selectors(View *v)
{
	unsigned int i, j;

	/* select correct frames of clients */
	for(i = 0; i < nclient; i++)
		for(j = 0; j < client[i]->nframe; j++)
			if(client[i]->frame[j]->area->view == v)
				client[i]->sel = j;
}

void
focus_tag(View *v)
{
	char buf[256];
	char name[256];
	unsigned int i;

	if(!nview)
		return;

	XGrabServer(dpy);
	sel = tag2index(v);

	update_frame_selectors(v);

	/* gives all(!) clients proper geometry (for use of different tags) */
	for(i = 0; i < nclient; i++)
		if(client[i]->nframe) {
			Frame *f = client[i]->frame[client[i]->sel];
			if(f->area->view == v) {
				XMoveWindow(dpy, client[i]->framewin, f->rect.x, f->rect.y);
				if(client[i]->nframe > 1)
					resize_client(client[i], &f->rect, False);
				draw_client(client[i]);
			}
			else
				XMoveWindow(dpy, client[i]->framewin, 2 * rect.width + f->rect.x, f->rect.y);
		}
	tags2str(name, sizeof(name), v->tag, v->ntag);
	snprintf(buf, sizeof(buf), "FocusTag %s\n", name);
	write_event(buf, True);
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
		*num = v->area[0]->nframe;
	else {
		for(i = 1; i < v->narea; i++)
			*num += v->area[i]->nframe;
	}

	if(*num) {
        result = cext_emallocz(*num * sizeof(XRectangle));
		if(isfloat) {
			for(i = 0; i < *num; i++)
				result[i] = v->area[0]->frame[0]->rect;
		}
		else {
			unsigned int j, n = 0;
			for(i = 1; i < v->narea; i++) {
				for(j = 0; j < v->area[i]->nframe; j++)
					result[n++] = v->area[i]->frame[j]->rect;
			}
		}
	}
    return result;
}

int
tid2index(unsigned short id)
{
	int i;
	for(i = 0; i < nview; i++)
		if(view[i]->id == id)
			return i;
	return -1;
}

View *
get_tag(char *name)
{
	unsigned int i, j, ntags;
	View *v = nil;
	char vname[256];
	char tags[MAX_TAGS][MAX_TAGLEN];

	for(i = 0; i < nview; i++) {
		v = view[i];
		tags2str(vname, sizeof(vname), v->tag, v->ntag);
		if(!strncmp(vname, name, strlen(name)))
			return v;
	}

	ntags = str2tags(tags, name);
	for(i = 0; i < nclient; i++)
		for(j = 0; j < ntags; j++)
			if(clienthastag(client[i], tags[j]))
				goto Createtag;
	return nil;

Createtag:
	v = alloc_tag(name);
	for(i = 0; i < nclient; i++)
		for(j = 0; j < ntags; j++)
			if(clienthastag(client[i], tags[j]) && !clientoftag(v, client[i]))
				attach_totag(v, client[i]);
	return v;
}

Bool
hasclient(View *v)
{
	unsigned int i;
	for(i = 0; i < v->narea; i++)
		if(v->area[i]->nframe)
			return True;
	return False;
}

void
select_tag(char *arg)
{
	int i;
	Client *c;
	View *v = get_tag(arg);

	if(!v)
		return;
	cext_strlcpy(def.tag, arg, sizeof(def.tag));
	if(!istag(ctag, nctag, arg)) {
		char buf[256];
		ctag = (char **)cext_array_attach((void **)ctag, strdup(arg),
				sizeof(char *), &ctagsz);
		nctag++;
		snprintf(buf, sizeof(buf), "NewTag %s\n", arg);
		write_event(buf, True);
	}
    focus_tag(v);

	/* cleanup on select */
	for(i = 0; i < nview; i++)
		if(!hasclient(view[i])) {
			destroy_tag(view[i]);
			i--;
		}

	if((c = sel_client_of_tag(v)))
		focus_client(c);
}

Bool
clientoftag(View *v, Client *c)
{
	unsigned int i;
	for(i = 0; i < v->narea; i++)
		if(clientofarea(v->area[i], c))
			return True;
	return False;
}

void
detach_fromtag(View *v, Client *c)
{
	int i;
	Client *cl;
	for(i = 0; i < v->narea; i++) {
		if(clientofarea(v->area[i], c)) {
			detach_fromarea(v->area[i], c);
			XMoveWindow(dpy, c->framewin, 2 * rect.width, 0);
		}
	}
	if((cl = sel_client_of_tag(v)))
		focus_client(cl);
}

void
attach_totag(View *v, Client *c)
{
	Area *a;

	if(c->trans || clienthastag(c, "~"))
		a = v->area[0];
	else
   		a = v->area[v->sel];

	attach_toarea(a, c);
    map_client(c);
	XMapWindow(dpy, c->framewin);
	if(v == view[sel])
		focus_client(c);
}

Client *
sel_client_of_tag(View *v)
{
	if(v) {
		Area *a = v->narea ? v->area[v->sel] : nil;
		return (a && a->nframe) ? a->frame[a->sel]->client : nil;
	}
	return nil;
}

void
restack_tag(View *v)
{
	unsigned int i, j, n = 0;
	static Window *wins = nil;
   	static unsigned int winssz = 0;

	if(nclient > winssz) {
		winssz = 2 * nclient;
		free(wins);
		wins = cext_emallocz(sizeof(Window) * winssz);
	}

	for(i = 0; i < v->narea; i++) {
		Area *a = v->area[i];
		if(a->nframe) {
			wins[n++] = a->frame[a->sel]->client->framewin;
			for(j = 0; j < a->nframe; j++) {
				if(j == a->sel)
					continue;
				wins[n++] = a->frame[j]->client->framewin;
			}
		}
	}

	if(n)
		XRestackWindows(dpy, wins, n);
}
