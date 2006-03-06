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
			resize_client(client[i], &f->rect, nil, False);
			if(f->area->tag == t)
				draw_client(client[i]);
		}
	snprintf(buf, sizeof(buf), "WS %s\n", t->name);
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
	unsigned int i;
	Tag *t;

	if(!has_ctag(name))
		return nil;
	for(i = 0; i < ntag; i++) {
		t = tag[i];
		if(!strncmp(t->name, name, strlen(t->name)))
			return t;
	}

	t = alloc_tag(name);
	for(i = 0; i < nclient; i++)
		if(!clientoftag(t, client[i]) && strstr(client[i]->tags, name))
			attach_totag(t, client[i]);
	return t;
}

void
select_tag(char *arg)
{
	Client *c;
	Tag *t = get_tag(arg);
	if(!t)
		return;
    focus_tag(t);
	if((c = sel_client_of_tag(t)))
		focus_client(c);
}

Bool
has_ctag(char *tag)
{
	unsigned int i;
	for(i = 0; i < nctag; i++)
		if(!strncmp(ctag[i], tag, strlen(ctag[i])))
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
update_ctags()
{
	unsigned int i, j, k;
	char buf[256];
	char *tags[128];

	fprintf(stderr, "%s", "update_ctags\n");
	for(i = 0; i < nctag; i++) {
		Bool exists = False;
		for(j = 0; j < nclient; j++)
			if(strstr(client[j]->tags, ctag[i]))
				exists = True;
		if(!exists) {
			for(j = 0; j < ntag; j++)
				if(!strncmp(tag[j]->name, ctag[i], strlen(tag[j]->name))) {
					destroy_tag(tag[j]);
					j--;
				}
		}
		free(ctag[i]);
		ctag[i] = nil;
	}
	nctag = 0;

	for(i = 0; i < nclient; i++) {
		cext_strlcpy(buf, client[i]->tags, sizeof(buf));
		j = cext_tokenize(tags, 128, buf, ' ');
		for(k = 0; k < j; k++) {
			if(!has_ctag(tags[k])) {
				ctag = (char **)cext_array_attach((void **)ctag, strdup(tags[k]),
						sizeof(char *), &ctagsz);
				nctag++;
			}
		}
	}

	for(i = 0; i < nclient; i++)
		for(j = 0; j < ntag; j++) {
			if(strstr(client[i]->tags, tag[j]->name)) {
				if(!clientoftag(tag[j], client[i]))
					attach_totag(tag[j], client[i]);
			}
			else {
				if(clientoftag(tag[j], client[i]))
					detach_fromtag(tag[j], client[i], False);
			}
		}
}
 
void
detach_fromtag(Tag *t, Client *c, Bool unmap)
{
	int i;
	for(i = 0; i < t->narea; i++)
		if(clientofarea(t->area[i], c))
			detach_fromarea(t->area[i], c);
}

void
attach_totag(Tag *t, Client *c)
{
	Area *a = t->area[t->sel];

	if(c->tags[0] == 0)
		cext_strlcat(c->tags, t->name, sizeof(c->tags));
    reparent_client(c, c->framewin, c->rect.x, c->rect.y);
	attach_toarea(a, c);
    map_client(c);
	XMapWindow(dpy, c->framewin);
}
