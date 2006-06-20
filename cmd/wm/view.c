/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "wm.h"

static char buf[256];

static void
assign_sel_view(View *v)
{
	if(sel && sel != v) {
		snprintf(buf, sizeof(buf), "UnfocusTag %s\n", sel->name);
		write_event(buf);
	}
	sel = v;
	snprintf(buf, sizeof(buf), "FocusTag %s\n", sel->name);
	write_event(buf);
}

View *
create_view(const char *name)
{
	static unsigned short id = 1;
	View **i, *v = cext_emallocz(sizeof(View));

	v->id = id++;
	cext_strlcpy(v->name, name, sizeof(v->name));
	create_area(v, nil, 0);
	create_area(v, v->area, 0);

	for(i=&view; *i && (strcmp((*i)->name, name) < 0); i=&(*i)->next);
	v->next = *i;
	*i = v;

	snprintf(buf, sizeof(buf), "CreateTag %s\n", v->name);
	write_event(buf);
	if(!sel)
		assign_sel_view(v);

	return v;
}

void
destroy_view(View *v)
{
	Area *a;
	View **i;

	while((a = v->area)) {
		v->area = a->next;
		destroy_area(a);
	};

	for(i=&view; *i; i=&(*i)->next)
		if(*i == v) break;
	*i = v->next;

	if(sel == v)
		for(sel=view; sel; sel=sel->next)
			if(sel->next == *i) break;

	snprintf(buf, sizeof(buf), "DestroyTag %s\n", v->name);
	write_event(buf);
	free(v);
}

static void
update_frame_selectors(View *v)
{
	Client *c;
	Frame *f;

	/* select correct frames of clients */
	for(c=client; c; c=c->next)
		for(f=c->frame; f; f=f->cnext)
			if(f->area->view == v) {
				c->sel = f;
				break;
			}
}

void
focus_view(View *v)
{
	Client *c;

	cext_assert(v);

	XGrabServer(dpy);
	assign_sel_view(v);

	update_frame_selectors(v);

	/* gives all(!) clients proper geometry (for use of different tags) */
	for(c=client; c; c=c->next)
		if(c->sel) {
			Frame *f = c->sel;
			if(f && f->area->view == v) {
				XMoveWindow(dpy, c->framewin, f->rect.x, f->rect.y);
				resize_client(c, &f->rect, False);
			}
			else
				XMoveWindow(dpy, c->framewin, 2 * rect.width + f->rect.x, f->rect.y);
		}

	if((c = sel_client()))
		focus_client(c, True);

	draw_frames();
	XSync(dpy, False);
	XUngrabServer(dpy);
	flush_masked_events(EnterWindowMask);
}

XRectangle *
rects_of_view(View *v, unsigned int *num)
{
	XRectangle *result = nil;
	Frame *f;

	*num = 2;
	for(f=v->area->frame; f; f=f->anext, (*num)++);

	result = cext_emallocz(*num * sizeof(XRectangle));
	for(f=v->area->frame; f; f=f->anext)
		*(result++) = f->rect;
	*(result++) = rect;
	*(result++) = brect;
	return (result - *num);
}

View *
view_of_id(unsigned short id) {
	View *v;
	for(v = view; v && v->id != id; v=v->next);
	return v;
}

View *
view_of_name(const char *name)
{
	View *v;
	for(v = view; v && strcmp(v->name, name); v=v->next);
	return v;
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
	assign_sel_view(get_view(arg));
	update_views(); /* performs focus_view */
}

static Bool
is_of_view(View *v, Client *c)
{
	Area *a;
	for(a=v->area; a; a=a->next)
		if(is_of_area(a, c))
			return True;
	return False;
}

void
detach_from_view(View *v, Client *c)
{
	Area *a, *next;

	for(a=v->area; a; a=next) {
		next=a->next;
		if(is_of_area(a, c)) {
			detach_from_area(a, c);
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
		a = v->area;
	else
		a = v->sel;
	attach_to_area(a, c, False);
	v->sel = a;
}

void
restack_view(View *v)
{
	Area *a;
	Frame *f;
	Client *c;
	unsigned int n=0, i=0;
	static Window *wins = nil;
	static unsigned int winssz = 0;

	for(c=client; c; c=c->next, i++);
	if(i > winssz) {
		winssz = 2 * i;
		wins = realloc(wins, sizeof(Window) * winssz);
	}

	for(a=v->area; a; a=a->next) {
		if(a->frame) {
			wins[n++] = a->sel->client->framewin;
			for(f=a->frame; f; f=f->anext)
				if(f != a->sel) n++;
			i=n;
			for(f=a->frame; f; f=f->anext) {
				Client *c = f->client;
				update_client_grab(c, (v->sel == a) && (a->sel == f));
				if(f != a->sel)
					wins[--i] = c->framewin;
			}
		}
	}

	if(n)
		XRestackWindows(dpy, wins, n);
}

void
scale_view(View *v, float w)
{
	unsigned int xoff, i=0;
	Area *a;
	float scale, dx = 0;
	int wdiff = 0;

	if(!v->area->next)
		return;

	for(a=v->area->next; a; a=a->next, i++)
		dx += a->rect.width;
	scale = w / dx;
	xoff = 0;
	for(a=v->area->next; a; a=a->next) {
		a->rect.width *= scale;
		if(!a->next)
			a->rect.width = w - xoff;
		xoff += a->rect.width;
	}

	/* MIN_COLWIDTH can only be respected when there is enough space; the caller should guarantee this */
	if(i * MIN_COLWIDTH > w)
		return;
	xoff = 0;
	for(a=v->area->next; a; a=a->next, i--) {
		if(a->rect.width < MIN_COLWIDTH)
			a->rect.width = MIN_COLWIDTH;
		else if((wdiff = xoff + a->rect.width - w + i * MIN_COLWIDTH) > 0)
			a->rect.width -= wdiff;
		if(!a->next)
			a->rect.width = w - xoff;
		xoff += a->rect.width;
	}
}

void
arrange_view(View *v)
{
	unsigned int xoff = 0;
	Area *a;

	if(!v->area->next)
		return;

	scale_view(v, rect.width);
	for(a=v->area->next; a; a=a->next) {
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
	static ViewLink *free_view_links = nil;
	ViewLink *v;
	char buf[256];
	char *toks[16];
	unsigned int i, n;

	cext_strlcpy(buf, c->tags, sizeof(buf));
	n = cext_tokenize(toks, 16, buf, '+');

	while((v = c->views)) {
		c->views = v->next;
		v->next = free_view_links;
		free_view_links = v;
	}

	for(i = 0; i < n; i++) {
		if(free_view_links) {
			v = free_view_links;
			free_view_links = v->next;
		}
		else
			v = cext_emallocz(sizeof(ViewLink));
		
		v->next = c->views;
		c->views = v;
		v->view = get_view(toks[i]);
	}
}

static Bool
is_view_of(Client *c, View *v)
{
	ViewLink *l;
	for(l=c->views; l; l=l->next)
		if(l->view == v)
			return True;
	return False;
}

/* XXX: This will need cleanup */
unsigned char *
view_index(View *v) {
	enum { BUF_MAX = 8092 };
	static unsigned char buf[BUF_MAX];
	unsigned int a_i, buf_i, n;
	int len;
	Frame *f;
	Area *a;

	len = BUF_MAX;
	buf_i = 0;
	for((a = v->area), (a_i = 0); a; (a=a->next), (a_i++)) {
		for(f=a->frame; f && len > 0; f=f->anext) {
			XRectangle *r = &f->rect;
			if(a_i == 0)
				n = snprintf((char *)&buf[buf_i], len, "~ %d %d %d %d %d %s\n",
						idx_of_client(f->client),
						r->x, r->y, r->width, r->height,
						f->client->props);
			else
				n = snprintf((char *)&buf[buf_i], len, "%d %d %d %s\n",
						a_i, idx_of_client(f->client),
						r->width, f->client->props);
			buf_i += n;
			len -= n;
		}
	}
	return buf;
}

Client *
client_of_message(char *message, unsigned int *next)
{              
	unsigned int i;
	Client *c;

	if(!strncmp(message, "sel ", 4)) {
		*next = 4;
		return sel_client();
	}
	if((1 != sscanf(message, "%d %n", &i, next)))
		return nil;
	for(c=client; i && c; c=c->next, i--);
	return c;
}

Frame *
clientframe_of_view(View *v, Client *c)
{              
	Frame *f;
	for(f=c->frame; f; f=f->cnext)
		if(f->area->view == v)
			break;
	return f;
} 

/* XXX: This will need cleanup too */
char *
message_view(View *v, char *message) {
	unsigned int n;
	Frame *f;
	Client *c;
	static char Ebadvalue[] = "bad value";

	if(!strncmp(message, "send ", 5)) {
		message += 5;
		if(!(c = client_of_message(message, &n)))
			return Ebadvalue;
		if(!(f = clientframe_of_view(v, c)))
			return Ebadvalue;
		return send_client(f, &message[n]);
	}
	if(!strncmp(message, "select ", 7)) {
		message += 7;
		return select_area(v->sel, message);
	}
	return Ebadvalue;
}

static Bool
is_empty(View *v)
{
	Area *a;
	for(a=v->area; a; a=a->next)
		if(a->frame)
			return False;
	return True;
}

void
update_views()
{
	View **i, *v, *old = sel;
	Client *c;

	for(c=client; c; c=c->next)
		update_client_views(c);

	for(c=client; c; c=c->next) {
		for(v=view; v; v=v->next) {
			update_frame_selectors(v);
			if(is_view_of(c, v)) {
				if(!is_of_view(v, c))
					attach_to_view(v, c);
			}else
				if(is_of_view(v, c))
					detach_from_view(v, c);
		}
	}

	if(old && !strncmp(old->name, "nil", 4))
		old = nil;

	for(i=&view; *i; *i && (i=&(*i)->next))
		if((*i != old) && is_empty(*i))
			destroy_view(*i);

	if(old)
		focus_view(old);
	else if(sel)
		focus_view(sel);
}

unsigned int
newcolw_of_view(View *v)
{
	Rule *r;
	Area *a;
	unsigned int i, n;
	regmatch_t tmpregm;

	for(r=def.colrules.rule; r; r=r->next) {
		if(!regexec(&r->regex, v->name, 1, &tmpregm, 0)) {
			char buf[256];
			char *toks[16];
			cext_strlcpy(buf, r->value, sizeof(buf));
			n = cext_tokenize(toks, 16, buf, '+');
			for(a=v->area, i=0; a; a=a->next, i++);
			if(n && n >= i) {
				if(sscanf(toks[i - 1], "%u", &n) == 1)
					return (rect.width * n) / 100;
			}
			break;
		}
	}
	return 0;
}
