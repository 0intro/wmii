/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <string.h>

#include "wm.h"

static Vector *
vector_of_views(ViewVector *vv)
{
	return (Vector *) vv;
}

static int
comp_view_name(const void *v1, const void *v2)
{
	View *vv1 = *(View **)v1;
	View *vv2 = *(View **)v2;
	return strcmp(vv1->name, vv2->name);
}

View *
create_view(const char *name)
{
	static unsigned short id = 1;
	static char buf[256];
	View *v = cext_emallocz(sizeof(View));

	v->id = id++;
	cext_strlcpy(v->name, name, sizeof(v->name));
	create_area(v, v->area.size, 0);
	create_area(v, v->area.size, 0);
	cext_vattach(vector_of_views(&view), v);
	qsort(view.data, view.size, sizeof(View *), comp_view_name);
	snprintf(buf, sizeof(buf), "CreateTag %s\n", name);
	write_event(buf);
	return v;
}

void
destroy_view(View *v)
{
	static char buf[256];
	while(v->area.size)
		destroy_area(v->area.data[0]);

	cext_vdetach(vector_of_views(&view), v);
	if(sel >= view.size)
		sel = 0;
	snprintf(buf, sizeof(buf), "DestroyTag %s\n", v->name);
	write_event(buf);

	free(v);
}

int
idx_of_view(View *v)
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
	for(i = 0; i < client.size; i++) {
		Client *c = client.data[i];
		for(j = 0; j < c->frame.size; j++)
			if(c->frame.data[j]->area->view == v) {
				c->sel = j;
				break;
			}
	}
}

void
focus_view(View *v)
{
	Client *c;
	unsigned int i;
	static char buf[256];

	XGrabServer(dpy);

	if(sel < view.size) {
		snprintf(buf, sizeof(buf), "UnfocusTag %s\n", view.data[sel]->name);
		write_event(buf);
	}
	sel = idx_of_view(v);

	update_frame_selectors(v);

	/* gives all(!) clients proper geometry (for use of different tags) */
	for(i = 0; i < client.size; i++)
		if(client.data[i]->frame.size) {
			Frame *f = client.data[i]->frame.data[client.data[i]->sel];
			if(f->area->view == v) {
				XMoveWindow(dpy, client.data[i]->framewin, f->rect.x, f->rect.y);
				resize_client(client.data[i], &f->rect, False);
			}
			else
				XMoveWindow(dpy, client.data[i]->framewin,
							2 * rect.width + f->rect.x, f->rect.y);
		}
	if((c = sel_client_of_view(v)))
		focus_client(c, True);
	draw_clients();
	XSync(dpy, False);
	XUngrabServer(dpy);
	flush_masked_events(EnterWindowMask);
	snprintf(buf, sizeof(buf), "FocusTag %s\n", v->name);
	write_event(buf);
}

XRectangle *
rects_of_view(View *v, unsigned int *num)
{
	XRectangle *result = nil;
	unsigned int i;

	*num = v->area.data[0]->frame.size + 2;

	if(*num) {
		result = cext_emallocz(*num * sizeof(XRectangle));
		for(i = 0; i < v->area.data[0]->frame.size; i++)
			result[i] = v->area.data[0]->frame.data[i]->rect;
		result[*num - 1] = rect;
		result[*num - 2] = brect;
	}
	return result;
}

int
idx_of_view_id(unsigned short id)
{
	int i;
	for(i = 0; i < view.size; i++)
		if(view.data[i]->id == id)
			return i;
	return -1;
}

View *
view_of_name(const char *name)
{
	unsigned int i;

	for(i = 0; i < view.size; i++)
		if(!strncmp(view.data[i]->name, name, strlen(name))
			&& !strncmp(view.data[i]->name, name, strlen(view.data[i]->name)))
			return view.data[i];
	return nil;
}

static View *
get_view(const char *name)
{
	View *v = view_of_name(name);
	return v ? v : create_view(name);
}

void
select_view(const char *arg)
{
	char buf[256];
	cext_strlcpy(buf, arg, sizeof(buf));
	cext_trim(buf, " \t+");
	if(!strlen(buf))
		return;
	focus_view(get_view(arg));
	update_views(); /* performs focus_view */
}

static Bool
is_of_view(View *v, Client *c)
{
	unsigned int i;
	for(i = 0; i < v->area.size; i++)
		if(is_of_area(v->area.data[i], c))
			return True;
	return False;
}

void
detach_from_view(View *v, Client *c)
{
	unsigned int i;

	for(i = 0; i < v->area.size; i++) {
		if(is_of_area(v->area.data[i], c)) {
			detach_from_area(v->area.data[i], c);
			XMoveWindow(dpy, c->framewin, 2 * rect.width, 0);
		}
	}
}

void
attach_to_view(View *v, Client *c)
{
	Area *a;

	c->revert = nil;

	if(c->trans || c->floating || c->fixedsize
		|| (c->rect.width == rect.width && c->rect.height == rect.height))
		a = v->area.data[0];
	else
		a = v->area.data[v->sel];
	attach_to_area(a, c, False);
	v->sel = idx_of_area(a);
}

Client *
sel_client_of_view(View *v)
{
	if(v) {
		Area *a = v->area.size ? v->area.data[v->sel] : nil;
		return sel_client_of_area(a);
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
				update_client_grab(c, (v->sel == i) && (a->sel == j));
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
scale_view(View *v, float w)
{
	unsigned int i, xoff;
	float scale, dx = 0;
	int wdiff = 0;

	if(v->area.size == 1)
		return;

	for(i = 1; i < v->area.size; i++)
		dx += v->area.data[i]->rect.width;
	scale = w / dx;
	xoff = 0;
	for(i = 1; i < v->area.size; i++) {
		Area *a = v->area.data[i];
		a->rect.width *= scale;
		if(i == v->area.size - 1)
			a->rect.width = w - xoff;
		xoff += a->rect.width;
	}

	/* MIN_COLWIDTH can only be respected when there is enough space; the caller should guarantee this */
	if((v->area.size - 1) * MIN_COLWIDTH > w)
		return;
	xoff = 0;
	for(i = 1; i < v->area.size; i++) {
		Area *a = v->area.data[i];
		if(a->rect.width < MIN_COLWIDTH)
			a->rect.width = MIN_COLWIDTH;
		else if((wdiff = xoff + a->rect.width - w + (v->area.size - 1 - i) * MIN_COLWIDTH) > 0)
			a->rect.width -= wdiff;
		if(i == v->area.size - 1)
			a->rect.width = w - xoff;
		xoff += a->rect.width;
	}
}

void
arrange_view(View *v)
{
	unsigned int i, xoff = 0;

	if(v->area.size == 1)
		return;

	scale_view(v, rect.width);
	for(i = 1; i < v->area.size; i++) {
		Area *a = v->area.data[i];
		a->rect.x = xoff;
		a->rect.y = 0;
		a->rect.height = rect.height - brect.height;
		xoff += a->rect.width;
		arrange_column(a, False);
	}
}

static void
update_client_views(Client *c)
{
	char buf[256];
	char *toks[16];
	unsigned int i, n;

	cext_strlcpy(buf, c->tags, sizeof(buf));
	n = cext_tokenize(toks, 16, buf, '+');

	while(c->view.size)
		cext_vdetach(vector_of_views(&c->view), c->view.data[0]);

	for(i = 0; i < n; i++)
		cext_vattach(vector_of_views(&c->view), get_view(toks[i]));
}

static Bool
is_view_of(Client *c, View *v)
{
	unsigned int i;
	for(i = 0; i < c->view.size; i++)
		if(c->view.data[i] == v)
			return True;
	return False;
}

static Bool
is_empty(View *v)
{
	unsigned int i;
	for(i = 0; i < v->area.size; i++)
		if(v->area.data[i]->frame.size)
			return False;
	return True;
}

static View *
next_empty_view(View *ignore)
{
	unsigned int i;
	for(i = 0; i < view.size; i++)
		if((view.data[i] != ignore) && is_empty(view.data[i]))
			return view.data[i];
	return nil;
}

void
update_views()
{
	unsigned int i, j;
	View *v, *old = view.size ? view.data[sel] : nil;

	for(i = 0; i < client.size; i++)
		update_client_views(client.data[i]);

	for(i = 0; i < client.size; i++) {
		Client *c = client.data[i];
		for(j = 0; j < view.size; j++) {
			View *vw = view.data[j];
			update_frame_selectors(vw);
			if(is_view_of(c, vw)) {
				if(!is_of_view(vw, c))
					attach_to_view(vw, c);
			}
			else {
				if(is_of_view(vw, c))
					detach_from_view(vw, c);
			}
		}
	}

	if(old && !strncmp(old->name, "nil", 4))
		old = nil;
	while((v = next_empty_view(old)))
		destroy_view(v);

	if(old)
		focus_view(old);
	else if(view.size)
		focus_view(view.data[sel]);
}

unsigned int
newcolw_of_view(View *v)
{
	unsigned int i, n;
	regmatch_t tmpregm;

	for(i = 0; i < vrule.size; i++) {
		Rule *r = vrule.data[i];
		if(!regexec(&r->regex, v->name, 1, &tmpregm, 0)) {
			char buf[256];
			char *toks[16];
			cext_strlcpy(buf, r->value, sizeof(buf));
			n = cext_tokenize(toks, 16, buf, '+');
			if(n && n > v->area.size - 1) {
				if(sscanf(toks[v->area.size - 1], "%u", &n) == 1)
					return (rect.width * n) / 100;
			}
			break;
		}
	}
	return 0;
}
