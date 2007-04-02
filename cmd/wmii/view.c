/* Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <util.h>
#include "dat.h"
#include "fns.h"

static Bool
is_empty(View *v) {
	Area *a;
	for(a=v->area; a; a=a->next)
		if(a->frame)
			return False;
	return True;
}

Frame *
clientframe_of_view(View *v, Client *c) {              
	Frame *f;
	for(f=c->frame; f; f=f->cnext)
		if(f->area->view == v)
			break;
	return f;
} 

static void
assign_sel_view(View *v) {
	if(screen->sel != v) {
		if(screen->sel)
			write_event("UnfocusTag %s\n",screen->sel->name);
		screen->sel = v;
		write_event("FocusTag %s\n", screen->sel->name);
	}
}

Client *
view_selclient(View *v) {
	return v->sel && v->sel->sel ? v->sel->sel->client : nil;
}

View *
get_view(const char *name) {
	View *v;
	int cmp;

	for(v = view; v; v=v->next)
		if((cmp=strcmp(name, v->name)) >= 0)
			break;
	if(!v || cmp != 0)
		v = create_view(name);
	return v;
}

View *
create_view(const char *name) {
	static ushort id = 1;
	View **i, *v;

	v = emallocz(sizeof(View));
	v->id = id++;
	strncpy(v->name, name, sizeof(v->name));

	write_event("CreateTag %s\n", v->name);
	create_area(v, nil, 0);
	create_area(v, v->area, 0);

	for(i=&view; *i; i=&(*i)->next)
		if(strcmp((*i)->name, name) < 0) break;
	v->next = *i;
	*i = v;

	if(!screen->sel)
		assign_sel_view(v);
	return v;
}

void
destroy_view(View *v) {
	Area *a;
	View **i, *tv;

	while((a = v->area)) {
		v->area = a->next;
		destroy_area(a);
	};
	for(i=&view; *i; i=&(*i)->next)
		if(*i == v) break;
	*i = v->next;
	write_event("DestroyTag %s\n", v->name);
	if(v == screen->sel) {
		for(tv=view; tv && tv->next; tv=tv->next)
			if(tv->next == *i) break;
		if(tv)
			focus_view(screen, tv);
	}
	free(v);
}

static void
update_frame_selectors(View *v) {
	Area *a;
	Frame *f;

	for(a=v->area; a; a=a->next)
		for(f=a->frame; f; f=f->anext)
			f->client->sel = f;
}

void
focus_view(WMScreen *s, View *v) {
	View *old;
	Frame *f;
	Client *c;

	old = screen->sel;
	XGrabServer(blz.dpy);
	assign_sel_view(v);
	update_frame_selectors(v);
	for(c=client; c; c=c->next)
		if((f = c->sel)) {
			if(f->view == v) {
				resize_client(c, &f->rect);
				update_client_grab(c);
			} else {
				unmap_frame(c);
				unmap_client(c, IconicState);
			}
		}
	restack_view(v);
	focus_area(v->sel);
	draw_frames();
	XSync(blz.dpy, False);
	XUngrabServer(blz.dpy);
	flushevents(EnterWindowMask, False);
}

void
select_view(const char *arg) {
	char buf[256];

	strncpy(buf, arg, sizeof(buf));
	trim(buf, " \t+/");
	if(!strlen(buf))
		return;
	if(!strncmp(buf, ".", 2) || !strncmp(buf, "..", 3))
		return;
	assign_sel_view(get_view(buf));
	update_views(); /* performs focus_view */
}

void
attach_to_view(View *v, Frame *f) {
	Client *c = f->client;

	c->revert = nil;
	if(c->trans || c->floating || c->fixedsize || c->fullscreen)
		v->sel = v->area;
	else if(starting && v->sel->floating)
		v->sel = v->area->next;
	attach_to_area(v->sel, f, False);
}

void
restack_view(View *v) {
	Area *a;
	Frame *f;
	Client *c;
	uint n, i;
	static Window *wins = nil;
	static uint winssz = 0;
	
	if(v != screen->sel)
		return;

	i = 0;
	n = 0;

	for(c=client; c; c=c->next)
		i++;
	if(i == 0)
		return;
	if(i >= winssz) {
		winssz = 2 * i;
		wins = erealloc(wins, sizeof(Window) * winssz);
	}

	for(f=v->area->stack; f; f=f->snext)
		if(f->client->fullscreen)
			wins[n++] = f->client->framewin;
	wins[n++] = screen->barwin;
	for(f=v->area->stack; f; f=f->snext)
		if(!f->client->fullscreen)
			wins[n++] = f->client->framewin;
	for(a=v->area->next; a; a=a->next) {
		if(a->frame) {
			wins[n++] = a->sel->client->framewin;
			for(f=a->frame; f; f=f->anext)
				if(f != a->sel)
					wins[n++] = f->client->framewin;
		}
	}
	if(n)
		XRestackWindows(blz.dpy, wins, n);
}

void
scale_view(View *v, float w) {
	uint xoff, num_col;
	uint min_width;
	Area *a;
	float scale, dx = 0;
	int wdiff = 0;

	min_width = screen->rect.width/NCOL;

	if(!v->area->next)
		return;

	num_col = 0;
	for(a=v->area->next; a; a=a->next)
		num_col++, dx += a->rect.width;

	scale = w / dx;
	xoff = 0;
	for(a=v->area->next; a; a=a->next) {
		a->rect.width *= scale;
		if(!a->next)
			a->rect.width = w - xoff;
		xoff += a->rect.width;
	}
	/* min_width can only be respected when there is enough space;
	 * the caller should guarantee this */
	if(num_col * min_width > w)
		return;
	xoff = 0;
	for(a=v->area->next, num_col--; a; a=a->next, num_col--) {
		if(a->rect.width < min_width)
			a->rect.width = min_width;
		else if((wdiff = xoff + a->rect.width - w + num_col * min_width) > 0)
			a->rect.width -= wdiff;
		if(!a->next)
			a->rect.width = w - xoff;
		xoff += a->rect.width;
	}
}

void
arrange_view(View *v) {
	uint xoff = 0;
	Area *a;

	if(!v->area->next)
		return;
	scale_view(v, screen->rect.width);
	for(a=v->area->next; a; a=a->next) {
		a->rect.x = xoff;
		a->rect.y = 0;
		a->rect.height = screen->rect.height - screen->brect.height;
		xoff += a->rect.width;
		arrange_column(a, False);
	}
}

XRectangle *
rects_of_view(View *v, uint *num, Frame *ignore) {
	XRectangle *result;
	Frame *f;
	int i;

	i = 2;
	for(f=v->area->frame; f; f=f->anext)
		i++;
	result = emallocz(i * sizeof(XRectangle));
	i = 0;
	for(f=v->area->frame; f; f=f->anext)
		if(f != ignore)
			result[i++] = f->rect;
	result[i++] = screen->rect;
	result[i++] = screen->brect;
	*num = i;
	return result;
}

/* XXX: This will need cleanup */
uchar *
view_index(View *v) {
	uint a_i, buf_i, n;
	int len;
	Frame *f;
	Area *a;

	len = BUFFER_SIZE;
	buf_i = 0;
	for((a = v->area), (a_i = 0); a && len > 0; (a=a->next), (a_i++)) {
		if(a->floating)
			n = snprintf(&buffer[buf_i], len, "# ~ %d %d\n",
					a->rect.width, a->rect.height);
		else
			n = snprintf(&buffer[buf_i], len, "# %d %d %d\n",
					a_i, a->rect.x, a->rect.width);
		buf_i += n;
		len -= n;
		for(f=a->frame; f && len > 0; f=f->anext) {
			XRectangle *r = &f->rect;
			if(a->floating)
				n = snprintf(&buffer[buf_i], len, "~ 0x%x %d %d %d %d %s\n",
						(uint)f->client->win,
						r->x, r->y, r->width, r->height,
						f->client->props);
			else
				n = snprintf(&buffer[buf_i], len, "%d 0x%x %d %d %s\n",
						a_i, (uint)f->client->win, r->y,
						r->height, f->client->props);
			if(len - n < 0)
				return (uchar*)buffer;
			buf_i += n;
			len -= n;
		}
	}
	return (uchar *)buffer;
}

Client *
client_of_message(View *v, char *message, uint *next) {              
	ulong id = 0;
	Client *c;

	if(!strncmp(message, "sel ", 4)) {
		*next = 4;
		return view_selclient(v);
	}
	sscanf(message, "0x%lx %n", &id, next);
	if(!id)
		sscanf(message, "%lu %n", &id, next);
	if(!id)
		return nil;
    for(c=client; c && c->win!=id; c=c->next);
	return c;
}

Area *
area_of_message(View *v, char *message, uint *next) {
	uint i;
	Area *a;

	if(!strncmp(message, "sel ", 4)) {
		*next = 4;
		return v->sel;
	}
	if(!strncmp(message, "~ ", 2)) {
		*next = 2;
		return v->area;
	}
	if(1 != sscanf(message, "%u %n", &i, next) || i == 0)
		return nil;
	for(a=v->area; i && a; a=a->next)
		i--;
	return a;
}

char *
message_view(View *v, char *message) {
	uint n, i;
	Client *c;
	Frame *f;
	Area *a;
	Bool swap;
	static char Ebadvalue[] = "bad value";

	if(!strncmp(message, "send ", 5)) {
		message += 5;
		swap = False;
		goto send;
	}
	if(!strncmp(message, "swap ", 5)) {
		message += 5;
		swap = True;
		goto send;
	}
	if(!strncmp(message, "select ", 7)) {
		message += 7;
		return select_area(v->sel, message);
	}
	if(!strncmp(message, "colmode ", 8)) {
		message += 8;
		if(!(a = area_of_message(v, message, &n)) || a == v->area)
			return Ebadvalue;
		if((i = column_mode_of_str(&message[n])) == -1)
			return Ebadvalue;
		a->mode = i;
		arrange_column(a, True);
		restack_view(v);
		if(v == screen->sel)
			focus_view(screen, v);
		draw_frames();
		return nil;
	}
	return Ebadvalue;

send:
	if(!(c = client_of_message(v, message, &n)))
		return Ebadvalue;
	if(!(f = clientframe_of_view(v, c)))
		return Ebadvalue;
	return send_client(f, &message[n], swap);
}

void
update_views() {
	View *n, *v, *old;
	Bool found;

	old = screen->sel;
	for(v=view; v; v=v->next)
		update_frame_selectors(v);

	found = False;
	for(v=view; v; v=n) {
		n=v->next;
		if(v != old) {
			found = True;
			if(is_empty(v))
				destroy_view(v);
		}
	}

	if(found && !strcmp(old->name, "nil") && is_empty(old))
		destroy_view(old);
	focus_view(screen, screen->sel);
}

uint
newcolw_of_view(View *v, int num) {
	regmatch_t regm;
	Rule *r;
	uint n;

	for(r=def.colrules.rule; r; r=r->next)
		if(!regexec(&r->regex, v->name, 1, &regm, 0)) {
			char buf[sizeof r->value];
			char *toks[16];

			strncpy(buf, r->value, sizeof(buf));
			n = tokenize(toks, 16, buf, '+');
			if(n > num)
				if(sscanf(toks[num], "%u", &n) == 1)
					return screen->rect.width * ((double)n / 100);
			break;
		}
	return 0;
}
