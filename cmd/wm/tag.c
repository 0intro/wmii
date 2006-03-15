/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

static Bool
istag(char **tags, unsigned int ntags, char *tag)
{
	unsigned int i;
	for(i = 0; i < ntags; i++)
		if(!strncmp(tags[i], tag, strlen(tag)))
			return True;
	return False;
}

Tag *
alloc_tag(char *name)
{
	static unsigned short id = 1;
    Tag *t = cext_emallocz(sizeof(Tag));

	t->id = id++;
	cext_strlcpy(t->name, name, sizeof(t->name));
	alloc_area(t);
	alloc_area(t);
	tag = (Tag **)cext_array_attach((void **)tag, t, sizeof(Tag *), &tagsz);
	ntag++;
	focus_tag(t);
    return t;
}

char *
destroy_tag(Tag *t)
{
	unsigned int i;

	for(i = 0; i < t->narea; i++)
		if(t->area[i]->nframe)
			return "tag not empty";

	while(t->narea)
		destroy_area(t->area[0]);

	cext_array_detach((void **)tag, t, &tagsz);
	ntag--;
	if(sel >= ntag)
		sel = 0;

    free(t); 
	return nil;
}

int
tag2index(Tag *t)
{
	int i;
	for(i = 0; i < ntag; i++)
		if(t == tag[i])
			return i;
	return -1;
}

void
focus_tag(Tag *t)
{
	char buf[16];
	int i, j;

	if(!ntag)
		return;

	XGrabServer(dpy);
	sel = tag2index(t);

	/* select correct frames of clients */
	for(i = 0; i < nclient; i++)
		for(j = 0; j < client[i]->nframe; j++)
			if(client[i]->frame[j]->area->tag == t)
				client[i]->sel = j;

	/* gives all(!) clients proper geometry (for use of different tags) */
	for(i = 0; i < nclient; i++)
		if(client[i]->nframe) {
			Frame *f = client[i]->frame[client[i]->sel];
			if(f->area->tag == t) {
				XMoveWindow(dpy, client[i]->framewin, f->rect.x, f->rect.y);
				if(client[i]->nframe > 1)
					resize_client(client[i], &f->rect, nil, False);
				draw_client(client[i]);
			}
			else
				XMoveWindow(dpy, client[i]->framewin, 2 * rect.width + f->rect.x, f->rect.y);
		}
	snprintf(buf, sizeof(buf), "FocusTag %s\n", t->name);
	write_event(buf);
	XSync(dpy, False);
	XUngrabServer(dpy);
}

XRectangle *
rectangles(Tag *t, Bool isfloat, unsigned int *num)
{
    XRectangle *result = nil;
	unsigned int i;
	
	*num = 0;
	if(isfloat)
		*num = t->area[0]->nframe;
	else {
		for(i = 1; i < t->narea; i++)
			*num += t->area[i]->nframe;
	}

	if(*num) {
        result = cext_emallocz(*num * sizeof(XRectangle));
		if(isfloat) {
			for(i = 0; i < *num; i++)
				result[i] = t->area[0]->frame[0]->rect;
		}
		else {
			unsigned int j, n = 0;
			for(i = 1; i < t->narea; i++) {
				for(j = 0; j < t->area[i]->nframe; j++)
					result[n++] = t->area[i]->frame[j]->rect;
			}
		}
	}
    return result;
}

int
tid2index(unsigned short id)
{
	int i;
	for(i = 0; i < ntag; i++)
		if(tag[i]->id == id)
			return i;
	return -1;
}

Tag *
get_tag(char *name)
{
	unsigned int i, n = 0, j, nt;
	Tag *t = nil;
	char buf[256];
	char *tags[8];

	for(i = 0; i < ntag; i++) {
		t = tag[i];
		if(!strncmp(t->name, name, strlen(name)))
			return t;
	}

	cext_strlcpy(buf, name, sizeof(buf));
	nt = cext_tokenize(tags, 8, buf, ' ');
	for(i = 0; i < nclient; i++)
		for(j = 0; j < nt; j++)
			if(clienthastag(client[i], tags[j])) {
				n++;
				break;
			}
	if(!n)
		return nil;

	t = alloc_tag(name);
	for(i = 0; i < nclient; i++)
		for(j = 0; j < nt; j++)
			if(clienthastag(client[i], tags[j]) && !clientoftag(t, client[i]))
				attach_totag(t, client[i]);
	return t;
}

void
select_tag(char *arg)
{
	int i, j, n;
	Client *c;
	Tag *t = get_tag(arg);

	if(!t)
		return;
    focus_tag(t);
	cext_strlcpy(def.tag, arg, sizeof(def.tag));
	if(!istag(ctag, nctag, arg)) {
		char buf[256];
		ctag = (char **)cext_array_attach((void **)ctag, strdup(arg),
				sizeof(char *), &ctagsz);
		nctag++;
		snprintf(buf, sizeof(buf), "NewTag %s\n", arg);
		write_event(buf);
	}

	for(i = 0; i < ntag; i++) {
		n = 0;
		for(j = 0; j < tag[i]->narea; j++)
			n += tag[i]->area[j]->nframe;
		if(!n) {
			destroy_tag(tag[i]);
			i--;
		}
	}

	if((c = sel_client_of_tag(t)))
		focus_client(c);
}

Bool
clientoftag(Tag *t, Client *c)
{
	unsigned int i;
	for(i = 0; i < t->narea; i++)
		if(clientofarea(t->area[i], c))
			return True;
	return False;
}

void
update_tags()
{
	unsigned int i, j;
	char buf[256];

	char **newctag = nil;
	unsigned int newctagsz = 0;
	unsigned int nnewctag = 0;

	for(i = 0; i < nclient; i++) {
		for(j = 0; j < client[i]->ntag; j++) {
			if(!strncmp(client[i]->tag[j], "~", 2)) /* magic floating tag */
				continue;
			if(!istag(newctag, nnewctag, client[i]->tag[j])) {
				newctag = (char **)cext_array_attach((void **)newctag, strdup(client[i]->tag[j]),
							sizeof(char *), &newctagsz);
				nnewctag++;
			}
		}
	}

	/* propagate tagging events */
	for(i = 0; i < nnewctag; i++)
		if(!istag(ctag, nctag, newctag[i])) {
			snprintf(buf, sizeof(buf), "NewTag %s\n", newctag[i]);
			write_event(buf);
		}
	for(i = 0; i < nctag; i++) {
		if(!istag(newctag, nnewctag, ctag[i])) {
			snprintf(buf, sizeof(buf), "RemoveTag %s\n", ctag[i]);
			write_event(buf);
		}
		free(ctag[i]);
	}

	free(ctag);
	ctag = newctag;
	nctag = nnewctag;
	ctagsz = newctagsz;

	for(i = 0; i < nclient; i++)
		for(j = 0; j < ntag; j++) {
			if(!clienthastag(client[i], tag[j]->name)) {
				if(clientoftag(tag[j], client[i]))
					detach_fromtag(tag[j], client[i]);
			}
			else {
				if(!clientoftag(tag[j], client[i]))
					attach_totag(tag[j], client[i]);
			}
		}

	if(!ntag && nctag)
		select_tag(ctag[0]);
}
 
void
detach_fromtag(Tag *t, Client *c)
{
	int i;
	Client *cl;
	for(i = 0; i < t->narea; i++) {
		if(clientofarea(t->area[i], c)) {
			detach_fromarea(t->area[i], c);
			XMoveWindow(dpy, c->framewin, 2 * rect.width, 0);
		}
	}
	if((cl = sel_client_of_tag(t)))
		focus_client(cl);
}

void
attach_totag(Tag *t, Client *c)
{
	Area *a;

	if(c->trans || clienthastag(c, "~"))
		a = t->area[0];
	else
   		a = t->area[t->sel];

	attach_toarea(a, c);
    map_client(c);
	XMapWindow(dpy, c->framewin);
	if(t == tag[sel])
		focus_client(c);
}

Client *
sel_client_of_tag(Tag *t)
{
	if(t) {
		Area *a = t->narea ? t->area[t->sel] : nil;
		return (a && a->nframe) ? a->frame[a->sel]->client : nil;
	}
	return nil;
}

void
restack_tag(Tag *t)
{
	unsigned int i, j, n = 0;
	static Window *wins = nil;
   	static unsigned int winssz = 0;

	if(nclient > winssz) {
		winssz = 2 * nclient;
		free(wins);
		wins = cext_emallocz(sizeof(Window) * winssz);
	}

	for(i = 0; i < t->narea; i++) {
		Area *a = t->area[i];
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

unsigned int
str2tags(const char *stags, char tags[MAX_TAGS][MAX_TAGLEN])
{
	unsigned int i, n;
	char buf[256];
	char *toks[MAX_TAGS];

	cext_strlcpy(buf, stags, sizeof(buf));
	n = cext_tokenize(toks, MAX_TAGS, buf, ' ');
	for(i = 0; i < n; i++)
		cext_strlcpy(tags[i], toks[i], MAX_TAGLEN);
	return n;
}
