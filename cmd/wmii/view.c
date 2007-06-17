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
view_clientframe(View *v, Client *c) {              
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
	new_column(v, v->area, 0);
	
	focus_area(v->area->next);

	for(i=&view; *i; i=&(*i)->next)
		if(strcmp((*i)->name, name) < 0)
			break;
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

	while((a = v->area->next))
		destroy_area(a);
	destroy_area(v->area);

	for(i=&view; *i; i=&(*i)->next)
		if(*i == v) break;
	*i = v->next;

	write_event("DestroyTag %s\n", v->name);

	if(v == screen->sel) {
		for(tv=view; tv; tv=tv->next)
			if(tv->next == *i) break;
		if(tv == nil)
			tv = view;
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

	XGrabServer(display);

	assign_sel_view(v);
	update_frame_selectors(v);
	update_divs();
	for(c=client; c; c=c->next)
		if((f = c->sel)) {
			if(f->view == v)
				resize_client(c, &f->r);
			else {
				unmap_frame(c);
				unmap_client(c, IconicState);
			}
		}

	restack_view(v);
	focus_area(v->sel);
	draw_frames();

	XSync(display, False);
	XUngrabServer(display);
	flushevents(EnterWindowMask, False);
}

void
select_view(const char *arg) {
	char buf[256];

	strncpy(buf, arg, sizeof(buf));
	trim(buf, " \t+/");

	if(strlen(buf) == 0)
		return;
	if(!strcmp(buf, ".") || !strcmp(buf, ".."))
		return;

	assign_sel_view(get_view(buf));
	update_views(); /* performs focus_view */
}

void
attach_to_view(View *v, Frame *f) {
	Client *c;
	
	c = f->client;
	c->revert = nil;
	if(c->trans || c->floating || c->fixedsize || c->fullscreen)
		focus_area(v->area);
	else if(starting && v->sel->floating)
		focus_area(v->area->next);
	attach_to_area(v->sel, f, False);
}

void
restack_view(View *v) {
	static XWindow *wins;
	static uint winssz;
	Divide *d;
	Frame *f;
	Client *c;
	Area *a;
	uint n, i;
	
	if(v != screen->sel)
		return;

	i = 0;
	for(c = client; c; c = c->next)
		i++;
	if(i == 0)
		return;

	for(a = v->area; a; a = a->next)
		i++;

	if(i >= winssz) {
		winssz = 2 * i;
		wins = erealloc(wins, sizeof(Window) * winssz);
	}

	n = 0;
	wins[n++] = screen->barwin->w;
	for(f = v->area->frame; f; f = f->anext)
		if(f->client->fullscreen) {
			n--;
			break;
		}

	for(f=v->area->stack; f; f=f->snext)
		wins[n++] = f->client->framewin->w;

	for(d = divs; d && d->w->mapped; d = d->next)
		wins[n++] = d->w->w;

	for(a=v->area->next; a; a=a->next)
		if(a->frame) {
			wins[n++] = a->sel->client->framewin->w;
			for(f=a->frame; f; f=f->anext)
				if(f != a->sel)
					wins[n++] = f->client->framewin->w;
		}
	if(n) {
		XRaiseWindow(display, wins[0]);
		XRestackWindows(display, wins, n);
	}
}

void
scale_view(View *v, int w) {
	uint xoff, numcol;
	uint minwidth;
	Area *a;
	float scale;
	int wdiff, dx;

	minwidth = Dx(screen->r)/NCOL;

	if(!v->area->next)
		return;

	numcol = 0;
	dx = 0;
	for(a=v->area->next; a; a=a->next) {
		numcol++;
		dx += Dx(a->r);
	}

	scale = (float)w / dx;
	xoff = 0;
	for(a=v->area->next; a; a=a->next) {
		a->r.max.x = xoff + Dx(a->r) * scale;
		a->r.min.x = xoff;
		if(!a->next)
			a->r.max.x = w;
		xoff = a->r.max.x;
	}

	/* minwidth can only be respected when there is enough space;
	 * the caller should guarantee this */
	if(numcol * minwidth > w)
		return;

	dx = numcol * minwidth;
	xoff = 0;
	for(a=v->area->next, numcol--; a; a=a->next, numcol--) {
		a->r.min.x = xoff;

		if(Dx(a->r) < minwidth)
			a->r.max.x = xoff + minwidth;
		else if((wdiff = xoff + Dx(a->r) - w + dx) > 0)
			a->r.max.x -= wdiff;
		if(!a->next)
			a->r.max.x = w;
		xoff = a->r.max.x;
	}
}

void
arrange_view(View *v) {
	uint xoff;
	Area *a;

	if(!v->area->next)
		return;

	scale_view(v, Dx(screen->r));
	xoff = 0;
	for(a=v->area->next; a; a=a->next) {
		a->r.min.x = xoff;
		a->r.min.y = 0;
		a->r.max.y = screen->brect.min.y;
		xoff = a->r.max.x;
		arrange_column(a, False);
	}
	if(v == screen->sel)
		update_divs();
}

Rectangle *
rects_of_view(View *v, uint *num, Frame *ignore) {
	Rectangle *result;
	Frame *f;
	int i;

	i = 2;
	for(f=v->area->frame; f; f=f->anext)
		i++;

	result = emallocz(i * sizeof(Rectangle));

	i = 0;
	for(f=v->area->frame; f; f=f->anext)
		if(f != ignore)
			result[i++] = f->r;
	result[i++] = screen->r;
	result[i++] = screen->brect;

	*num = i;
	return result;
}

/* XXX: This will need cleanup */
uchar *
view_index(View *v) {
	Rectangle *r;
	Frame *f;
	Area *a;
	char *buf, *end;
	uint i;

	buf = buffer;
	end = buffer+sizeof(buffer);
	for((a=v->area), (i=0); a && buf < end-1; (a=a->next), i++) {
		if(a->floating)
			buf += snprintf(buf, end-buf, "# ~ %d %d\n",
					Dx(a->r), Dy(a->r));
		else
			buf += snprintf(buf, end-buf, "# %d %d %d\n",
					i, a->r.min.x, Dx(a->r));

		for(f=a->frame; f && buf < end-1; f=f->anext) {
			r = &f->r;
			if(a->floating)
				buf += snprintf(buf, end-buf, "~ 0x%x %d %d %d %d %s\n",
						(uint)f->client->w.w,
						r->min.x, r->min.y,
						Dx(*r), Dy(*r),
						f->client->props);
			else
				buf += snprintf(buf, end-buf, "%d 0x%x %d %d %s\n",
						i, (uint)f->client->w.w,
						r->min.y, Dy(*r),
						f->client->props);
		}
	}
	return (uchar*)buffer;
}

uchar *
view_ctl(View *v) {
	Area *a;
	char *buf, *end;
	uint i;

	buf = buffer;
	end = buffer+sizeof(buffer);

	buf += snprintf(buf, end-buf, "%s\n", v->name);

	/* select <area>[ <frame>] */
	buf += snprintf(buf, end-buf, "select %s", area_name(v->sel));
	if(v->sel->sel)
		buf  += snprintf(buf, end-buf, " %d", frame_idx(v->sel->sel));
	buf  += snprintf(buf, end-buf, "\n");

	/* select client <client> */
	if(v->sel->sel)
		buf  += snprintf(buf, end-buf, "select client 0x%x\n", clientwin(v->sel->sel->client));

	for(a = v->area->next, i = 1; a && buf < end-1; a = a->next, i++) {
		buf += snprintf(buf, end-buf, "colmode %d %s\n",
			i, colmode2str(a->mode));
	}
	return (uchar*)buffer;
}

void
update_views(void) {
	View *n, *v, *old;
	Bool found;

	old = screen->sel;
	for(v=view; v; v=v->next)
		update_frame_selectors(v);

	found = False;
	for(v=view; v; v=n) {
		n=v->next;
		if(v != old) {
			if(is_empty(v))
				destroy_view(v);
			else
				found = True;
		}
	}

	if(found && !strcmp(old->name, "nil") && is_empty(old))
		destroy_view(old);
	focus_view(screen, screen->sel);
}

uint
newcolw(View *v, int num) {
	regmatch_t regm;
	Rule *r;
	uint n;

	for(r=def.colrules.rule; r; r=r->next)
		if(!regexec(&r->regex, v->name, 1, &regm, 0)) {
			char buf[sizeof r->value];
			char *toks[16];

			strcpy(buf, r->value);

			n = tokenize(toks, 16, buf, '+');
			if(num < n)
				if(sscanf(toks[num], "%u", &n) == 1)
					return Dx(screen->r) * (n / 100.0);
			break;
		}
	return 0;
}
