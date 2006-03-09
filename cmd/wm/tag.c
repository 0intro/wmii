/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>

#include "wm.h"

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
	int i, j, pi = tag2index(t);
	int px;

	if(!ntag || (pi == -1))
		return;

	sel = pi;
	px = sel * rect.width;

	/* select correct frames of clients */
	for(i = 0; i < nclient; i++)
		for(j = 0; j < client[i]->nframe; j++)
			if(client[i]->frame[j]->area->tag == t)
				client[i]->sel = j;

	/* gives all(!) clients proper geometry (for use of different tags) */
	for(i = 0; i < nclient; i++)
		if(client[i]->nframe) {
			Frame *f = client[i]->frame[client[i]->sel];
			pi = tag2index(f->area->tag);
			XMoveWindow(dpy, client[i]->framewin, px - (pi * rect.width) + f->rect.x, f->rect.y);
			if(f->area->tag == t) {
				if(client[i]->nframe > 1)
					resize_client(client[i], &f->rect, nil, False);
				draw_client(client[i]);
			}
		}
	snprintf(buf, sizeof(buf), "TF %s\n", t->name);
	write_event(buf);
	XSync(dpy, False);
}

XRectangle *
rectangles(unsigned int *num)
{
    XRectangle *result = 0;
    int i, j = 0;
    Window d1, d2;
    Window *wins;
    XWindowAttributes wa;
    XRectangle r;

    if(XQueryTree(dpy, root, &d1, &d2, &wins, num)) {
        result = cext_emallocz(*num * sizeof(XRectangle));
        for(i = 0; i < *num; i++) {
            if(!XGetWindowAttributes(dpy, wins[i], &wa))
                continue;
            if(wa.override_redirect && (wa.map_state == IsViewable)) {
                r.x = wa.x;
                r.y = wa.y;
                r.width = wa.width;
                r.height = wa.height;
                result[j++] = r;
            }
        }
    }
    if(wins)
        XFree(wins);
    *num = j;
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
	unsigned int i, n = 0;
	Tag *t;

	if(!has_tag(ctag, name, nctag))
		return nil;
	for(i = 0; i < ntag; i++) {
		t = tag[i];
		if(!strncmp(t->name, name, strlen(t->name)))
			return t;
	}

	for(i = 0; i < nclient; i++)
		if(strstr(client[i]->tags, name))
			n++;
	if(!n)
		return nil;

	t = alloc_tag(name);
	for(i = 0; i < nclient; i++)
		if(strstr(client[i]->tags, name))
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
has_tag(char **tags, char *tag, unsigned int ntags)
{
	unsigned int i;
	for(i = 0; i < ntags; i++)
		if(!strncmp(tags[i], tag, strlen(tags[i])))
			return True;
	return False;
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
	unsigned int i, j, k;
	char buf[256];
	char *tags[128];
	char *t;

	char **newctag = nil;
	unsigned int newctagsz = 0;
	unsigned int nnewctag = 0;

	for(i = 0; i < nclient; i++) {
		cext_strlcpy(buf, client[i]->tags, sizeof(buf));
		j = cext_tokenize(tags, 128, buf, ' ');
		for(k = 0; k < j; k++) {
			t = tags[k];
			if(*t == '~')
				t++;
			if(!*t) /* should not happen, but some user might try */
				continue;
			if(!has_tag(newctag, t, nnewctag)) {
				newctag = (char **)cext_array_attach((void **)newctag, strdup(t),
							sizeof(char *), &newctagsz);
				nnewctag++;
			}
		}
	}

	/* propagate tagging events */
	for(i = 0; i < nnewctag; i++)
		if(!has_tag(ctag, newctag[i], nctag)) {
			snprintf(buf, sizeof(buf), "NT %s\n", newctag[i]);
			write_event(buf);
		}
	for(i = 0; i < nctag; i++) {
		if(!has_tag(newctag, ctag[i], nnewctag)) {
			snprintf(buf, sizeof(buf), "RT %s\n", ctag[i]);
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
			if(strstr(client[i]->tags, tag[j]->name)) {
				if(!clientoftag(tag[j], client[i]))
					attach_totag(tag[j], client[i]);
			}
			else {
				if(clientoftag(tag[j], client[i]))
					detach_fromtag(tag[j], client[i]);
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

	if(strchr(c->tags, '~'))
		a = t->area[0];
	else
   		a = t->area[t->sel];
    reparent_client(c, c->framewin, c->rect.x, c->rect.y);
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
