/* Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fns.h"

static char
	Ebadcmd[] = "bad command",
	Ebadvalue[] = "bad value",
	Ebadusage[] = "bad usage";

/* Edit |sort   Edit s/"([^"]+)"/L\1/g   Edit |tr 'a-z' 'A-Z' */
enum {
	LFULLSCREEN,
	LURGENT,
	LBORDER,
	LCLIENT,
	LCOLMODE,
	LDOWN,
	LEXEC,
	LFOCUSCOLORS,
	LFONT,
	LGRABMOD,
	LKILL,
	LLEFT,
	LNORMCOLORS,
	LOFF,
	LON,
	LQUIT,
	LRIGHT,
	LSELCOLORS,
	LSELECT,
	LSEND,
	LSWAP,
	LTOGGLE,
	LUP,
	LVIEW,
	LTILDE,
};
char *symtab[] = {
	"Fullscreen",
	"Urgent",
	"border",
	"client",
	"colmode",
	"down",
	"exec",
	"focuscolors",
	"font",
	"grabmod",
	"kill",
	"left",
	"normcolors",
	"off",
	"on",
	"quit",
	"right",
	"selcolors",
	"select",
	"send",
	"swap",
	"toggle",
	"up",
	"view",
	"~",
};

/* Edit ,y/^[a-zA-Z].*\n.* {\n/d
 * Edit s/^([a-zA-Z].*)\n(.*) {\n/\1 \2;\n/
 * Edit ,x/^static.*\n/d
 */

static int
getsym(char *s) {
	int i, n, m, cmp;

	if(s == nil)
		return -1;

	n = nelem(symtab);
	i = 0;
	while(n) {
		m = n/2;
		cmp = strcmp(s, symtab[i+m]);
		if(cmp == 0)
			return i+m;
		if(cmp < 0 || m == 0)
			n = m;
		else {
			i += m;
			n = n-m;
		}
	}
	return -1;
}

static int
gettoggle(IxpMsg *m) {
	switch(getsym(getword(m))) {
	case LON:
		return On;
	case LOFF:
		return Off;
	case LTOGGLE:
		return Toggle;
	default:
		return -1;
	}
}

static void
eatrunes(IxpMsg *m, int (*p)(Rune), int val) {
	Rune r;
	int n;

	while(m->pos < m->end) {
		n = chartorune(&r, (char*)m->pos);
		if(p(r) != val)
			break;
		m->pos += n;
	}
	if(m->pos > m->end)
		m->pos = m->end;
}

char *
getword(IxpMsg *m) {
	char *ret;
	Rune r;
	int n;

	eatrunes(m, isspacerune, 1);
	ret = (char*)m->pos;
	eatrunes(m, isspacerune, 0);
	n = chartorune(&r, (char*)m->pos);
	*m->pos = '\0';
	m->pos += n;
	eatrunes(m, isspacerune, 1);

	if(ret == (char*)m->end)
		return nil;
	return ret;
}

#define strbcmp(str, const) (strncmp((str), (const), sizeof(const)-1))	
static int
getbase(char **s) {
	char *p;

	p = *s;
	if(!strbcmp(p, "0x")) {
		*s += 2;
		return 16;
	}
	if(isdigit(p[0]) && p[1] == 'r') {
		*s += 2;
		return p[0] - '0';
	}
	if(isdigit(p[0]) && isdigit(p[1]) && p[2] == 'r') {
		*s += 3;
		return 10*(p[0]-'0') + (p[1]-'0');
	}
	if(!strbcmp(p, "0")) {
		*s += 1;
		return 8;
	}
	return 10;
}

static int
getlong(char *s, long *ret) {
	char *end, *rend;
	int base;

	end = s+strlen(s);
	base = getbase(&s);

	*ret = strtol(s, &rend, base);
	return (end == rend);
}

static int
getulong(char *s, ulong *ret) {
	char *end, *rend;
	int base;

	end = s+strlen(s);
	base = getbase(&s);

	*ret = strtoul(s, &rend, base);
	return (end == rend);
}

static Client *
strclient(View *v, char *s) {
	ulong id;

	if(!strcmp(s, "sel"))
		return view_selclient(v);
	if(getulong(s, &id))
		return win2client(id);

	return nil;
}

Area *
strarea(View *v, char *s) {
	Area *a;
	long i;

	if(!strcmp(s, "sel"))
		return v->sel;
	if(!strcmp(s, "~"))
		return v->area;
	if(!getlong(s, &i) || i == 0)
		return nil;

	if(i > 0)
		for(a = v->area; a; a = a->next) {
			if(i-- == 0) break;
		}
	else {
		for(a = v->area; a->next; a = a->next)
			;
		for(; a != v->area; a = a->prev)
			if(++i == 0) break;
		if(a == v->area)
			a = nil;
	}
	return a;
}

char *
message_view(View *v, IxpMsg *m) {
	Area *a;
	char *s;
	int i;

	s = getword(m);

	switch(getsym(s)) {
	case LSEND:
		return send_client(v, m, 0);
	case LSWAP:
		return send_client(v, m, 1);
	case LSELECT:
		return select_area(v->sel, m);
	case LCOLMODE:
		s = getword(m);
		if((a = strarea(v, s)) == nil || a->floating)
			return Ebadvalue;

		s = getword(m);
		if((i = str2colmode(s)) == -1)
			return Ebadvalue;

		a->mode = i;
		arrange_column(a, True);
		restack_view(v);

		if(v == screen->sel)
			focus_view(screen, v);
		draw_frames();
		return nil;
	default:
		return Ebadcmd;
	}
	/* not reached */
}

char *
parse_colors(IxpMsg *m, CTuple *col) {
	static char Ebad[] = "bad color string";
	Rune r;
	char c, *p;
	int i, j;

	/* '#%6x #%6x #%6x' */
	p = (char*)m->pos;
	for(i = 0; i < 3 && p < (char*)m->end; i++) {
		if(*p++ != '#')
			return Ebad;
		for(j = 0; j < 6 && p < (char*)m->end; j++)
			if(!isxdigit(*p++))
				return Ebad;
		chartorune(&r, p);
		if(i < 2 && r != ' ' || !(isspacerune(r) || *p == '\0'))
			return Ebad;
		if(i < 2)
			p++;
	}

	c = *p;
	*p = '\0';
	loadcolor(col, (char*)m->pos);
	*p = c;

	m->pos = (uchar*)p;
	eatrunes(m, isspacerune, 1);
	return nil;
}

char *
message_root(void *p, IxpMsg *m) {
	Font *fn;
	char *s, *ret;
	ulong n;

	USED(p);
	ret = nil;
	s = getword(m);

	switch(getsym(s)) {
	case LQUIT:
		srv.running = 0;
		break;
	case LEXEC:
		execstr = smprint("exec %s", (char*)m->pos);
		srv.running = 0;
		break;
	case LVIEW:
		select_view((char*)m->pos);
		break;
	case LSELCOLORS:
		fprint(2, "%s: warning: selcolors have been removed\n", argv0);
		return Ebadcmd;
	case LFOCUSCOLORS:
		ret = parse_colors(m, &def.focuscolor);
		focus_view(screen, screen->sel);
		break;
	case LNORMCOLORS:
		ret = parse_colors(m, &def.normcolor);
		focus_view(screen, screen->sel);
		break;
	case LFONT:
		fn = loadfont((char*)m->pos);
		if(fn) {
			freefont(def.font);
			def.font = fn;
			resize_bar(screen);
		}else
			ret = "can't load font";
		break;
	case LBORDER:
		if(!getulong(getword(m), &n))
			return Ebadvalue;
		def.border = n;
		/* XXX: Apply the change */
		break;
	case LGRABMOD:
		s = getword(m);
		n = str2modmask(s);

		if((n & (Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask)) == 0)
			return Ebadvalue;

		utflcpy(def.grabmod, s, sizeof(def.grabmod));
		def.mod = n;
		break;
	default:
		return Ebadcmd;
	}
	return ret;
}

char *
read_root_ctl(void) {
	char *b, *e;

	b = buffer;
	e = b + sizeof(buffer) - 1;
	b = seprint(b, e, "view %s\n", screen->sel->name);
	b = seprint(b, e, "focuscolors %s\n", def.focuscolor.colstr);
	b = seprint(b, e, "normcolors %s\n", def.normcolor.colstr);
	b = seprint(b, e, "font %s\n", def.font->name);
	b = seprint(b, e, "grabmod %s\n", def.grabmod);
	b = seprint(b, e, "border %d\n", def.border);
	USED(b);
	return buffer;
}

char *
message_client(Client *c, IxpMsg *m) {
	char *s;
	int i;

	s = getword(m);

	switch(getsym(s)) {
	case LKILL:
		kill_client(c);
		break;
	case LURGENT:
		i = gettoggle(m);
		if(i == -1)
			return Ebadusage;
		set_urgent(c, i, True);
		break;
	case LFULLSCREEN:
		i = gettoggle(m);
		if(i == -1)
			return Ebadusage;
		fullscreen(c, i);
		break;
	default:
		return Ebadcmd;
	}
	return nil;
}

static char*
send_frame(Frame *f, int sym, Bool swap) {
	Frame *fp;

	SET(fp);
	switch(sym) {
	case LUP:
		fp = f->aprev;
		if(!fp)
			return Ebadvalue;
		fp = fp->aprev;
		break;
	case LDOWN:
		fp = f->anext;
		if(!fp)
			return Ebadvalue;
		break;
	default:
		assert(!"can't get here");
	}

	if(swap) {
		if(!fp)
			return Ebadvalue;
		swap_frames(f, fp);
	}else {
		remove_frame(f);
		insert_frame(fp, f);
	}

	arrange_view(f->view);

	flushevents(EnterWindowMask, False);
	focus_frame(f, True);
	update_views();
	return nil;
}

char *
send_client(View *v, IxpMsg *m, Bool swap) {
	Area *to, *a;
	Frame *f;
	Client *c;
	char *s;
	ulong i;
	int sym;

	s = getword(m);

	c = strclient(v, s);
	if(c == nil)
		return Ebadvalue;

	f = view_clientframe(v, c);
	if(f == nil)
		return Ebadvalue;

	a = f->area;
	to = nil;

	s = getword(m);
	sym = getsym(s);

	switch(sym) {
	case LUP:
	case LDOWN:
		return send_frame(f, sym, swap);
	case LLEFT:
		if(a->floating)
			return Ebadvalue;

		if(a->prev != v->area)
			to = a->prev;
		a = v->area;
		break;
	case LRIGHT:
		if(a->floating)
			return Ebadvalue;

		to = a->next;
		break;
	case LTOGGLE:
		if(!a->floating)
			to = v->area;
		else if(c->revert && !c->revert->floating)
			to = c->revert;
		else
			to = v->area->next;
		break;
	default:
		if(!getulong(s, &i) || i == 0)
			return Ebadvalue;

		for(to=v->area; to; to=to->next)
			if(!i--) break;
		break;
	}

	if(!to && !swap && (f->anext || f != f->area->frame))
		to = new_column(v, a, 0);

	if(!to)
		return Ebadvalue;

	if(!swap)
		send_to_area(to, f);
	else if(to->sel)
		swap_frames(f, to->sel);
	else
		return Ebadvalue;

	flushevents(EnterWindowMask, False);
	focus_frame(f, True);
	arrange_view(v);
	update_views();
	return nil;
}

static char*
select_frame(Frame *f, IxpMsg *m, int sym) {
	Frame *fp;
	Client *c;
	Area *a;
	char *s;
	ulong i;

	if(!f)
		return Ebadvalue;
	a = f->area;

	SET(fp);
	switch(sym) {
	case LUP:
		for(fp = a->frame; fp->anext; fp = fp->anext)
			if(fp->anext == f) break;
		break;
	case LDOWN:
		fp = f->anext;
		if(fp == nil)
			fp = a->frame;
		break;
	case LCLIENT:
		s = getword(m);
		if(s == nil || !getulong(s, &i))
			return "usage: select client <client>";
		c = win2client(i);
		if(c == nil)
			return "unknown client";
		fp = view_clientframe(f->view, c);
		break;
	default:
		assert(!"can't get here");
	}

	if(fp == nil)
		return "invalid selection";

	focus_frame(fp, False);
	frame_to_top(fp);
	if(f->view == screen->sel)
		restack_view(f->view);
	return nil;
}

char*
select_area(Area *a, IxpMsg *m) {
	Frame *f;
	Area *ap;
	View *v;
	char *s;
	ulong i;
	int sym;

	v = a->view;
	s = getword(m);
	sym = getsym(s);
	
	switch(sym) {
	case LTOGGLE:
		if(!a->floating)
			ap = v->area;
		else if(v->revert && v->revert != a)
			ap = v->revert;
		else
			ap = v->area->next;
		break;
	case LUP:
	case LDOWN:
	case LCLIENT:
		return select_frame(a->sel, m, sym);
	case LLEFT:
		if(a->floating)
			return Ebadvalue;
		for(ap=v->area->next; ap->next; ap=ap->next)
			if(ap->next == a) break;
		break;
	case LRIGHT:
		if(a->floating)
			return Ebadvalue;
		ap = a->next;
		if(ap == nil)
			ap = v->area->next;
		break;
	case LTILDE:
		ap = v->area;
		break;
	default:
		if(!strcmp(s, "sel"))
			ap = v->sel;
		else {
			if(!getulong(s, &i) || i == 0)
				return Ebadvalue;
			for(ap=v->area->next; ap; ap=ap->next)
				if(--i == 0) break;
			if(i != 0)
				return Ebadvalue;
		}
		if((s = getword(m))) {
			if(!getulong(s, &i))
				return Ebadvalue;
			for(f = ap->frame; f; f = f->anext)
				if(--i == 0) break;
			if(i != 0)
				return Ebadvalue;
			focus_frame(f, True);
			return nil;
		}
	}

	focus_area(ap);
	return nil;
}
