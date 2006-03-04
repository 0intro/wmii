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
alloc_tag()
{
	static unsigned short id = 1;
    Tag *t = cext_emallocz(sizeof(Tag));

	t->id = id++;
	alloc_area(t);
	alloc_area(t);
	tag = (Tag **)cext_array_attach((void **)tag, t, sizeof(Tag *), &tagsz);
	ntag++;
	focus_tag(t);
    XChangeProperty(dpy, root, net_atom[NetNumWS], XA_CARDINAL,
			        32, PropModeReplace, (unsigned char *) &ntag, 1);
    return t;
}

char *
destroy_tag(Tag *t)
{
	unsigned int i;

	for(i = 0; i < t->narea; i++)
		if(t->area[i]->nclient)
			return "tag not empty";

	while(t->narea)
		destroy_area(t->area[0]);

	cext_array_detach((void **)tag, t, &tagsz);
	ntag--;

	for(i = 0; i < ntag; i++) 
		XChangeProperty(dpy, root, net_atom[NetNumWS], XA_CARDINAL,
				32, PropModeReplace, (unsigned char *) &i, 1);

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
	Client *c;
	int i, pi = tag2index(t);
	int px;

	if(!ntag || (pi == -1))
		return;

	sel = pi;
	px = sel * rect.width;
	/* gives all(!) clients proper geometry (for use of different tagrs) */
	for(i = 0; i < nclient; i++) {
		c = client[i];
		if(c->area) {
			pi = tag2index(c->area->tag);
			XMoveWindow(dpy, c->framewin, px - (pi * rect.width) + c->frect.x, c->frect.y);
			if(c->area->tag == t)
				draw_client(c);
		}
	}
	snprintf(buf, sizeof(buf), "PN %d\n", sel);
	write_event(buf);
    XChangeProperty(dpy, root, net_atom[NetSelWS], XA_CARDINAL,
			        32, PropModeReplace, (unsigned char *) &sel, 1);
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
pid2index(unsigned short id)
{
	int i;
	for(i = 0; i < ntag; i++)
		if(tag[i]->id == id)
			return i;
	return -1;
}

void
select_tag(char *arg)
{
	unsigned int new = sel;
	const char *err;
	Client *c;

    if(!ntag)
        return;
    if(!strncmp(arg, "prev", 5)) {
		if(new <= 1)
			new = ntag;
		new--;
    } else if(!strncmp(arg, "next", 5)) {
		if(new < ntag - 1)
			new++;
		else
			new = 1;
    } else {
		int idx = cext_strtonum(arg, 0, ntag - 1, &err);
		if(idx < ntag)
			new = idx;
	}
    focus_tag(tag[new]);
	if((c = sel_client_of_tag(tag[new])))
		focus_client(c);
}
