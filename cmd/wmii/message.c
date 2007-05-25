/* Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include "dat.h"
#include "fns.h"

static char
	Ebadcmd[] = "bad command",
	Ebadvalue[] = "bad value";

/* Edit |sort   Edit s/"([^"]+)"/L\1/g   Edit |tr 'a-z' 'A-Z' */
enum {
	LFULLSCREEN,
	LNOTFULLSCREEN,
	LNOTURGENT,
	LURGENT,
	LBORDER,
	LCOLMODE,
	LDOWN,
	LEXEC,
	LFOCUSCOLORS,
	LFONT,
	LGRABMOD,
	LKILL,
	LLEFT,
	LNORMCOLORS,
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
	"NotFullscreen",
	"NotUrgent",
	"Urgent",
	"border",
	"colmode",
	"down",
	"exec",
	"focuscolors",
	"font",
	"grabmod",
	"kill",
	"left",
	"normcolors",
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

	n = nelem(symtab);
	i = 0;
	while(n) {
		m = n/2;
		cmp = strcmp(s, symtab[i+m]);
		if(cmp == 0)
			return i+m;
		if(cmp > 0) {
			i += m;
			n = n-m;
		}
		else
			n = m;
	}
	return -1;
}

static void
eatrunes(Message *m, int (*p)(Rune), int val) {
	Rune r;
	int n;

	while(m->pos < m->end) {
		n = chartorune(&r, m->pos);
		if(p(r) != val)
			break;
		m->pos += n;
	}
}

char *
getword(Message *m) {
	char *ret;
	Rune r;
	int n;
	
	eatrunes(m, isspacerune, 1);
	ret = m->pos;
	eatrunes(m, isspacerune, 0);
	n = chartorune(&r, m->pos);
	*m->pos = '\0';
	m->pos += n;
	eatrunes(m, isspacerune, 1);

	return ret;
}


#define strbcmp(str, const) (strncmp((str), (const), sizeof(const)-1))	
static int
getbase(char **s) {
	if(!strbcmp(*s, "0x")) {
		*s += 2;
		return 16;
	}
	if(!strbcmp(*s, "0")) {
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

	if(i > 0) {
		for(a = v->area; a; a = a->next)
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
message_view(View *v, Message *m) {
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
	assert(!"can't get here");
}

char *
parse_colors(Message *m, CTuple *col) {
	static regex_t reg;
	static Bool compiled;
	char c;

	if(!compiled) {
		compiled = 1;
		regcomp(&reg, "^#[0-9a-f]{6} #[0-9a-f]{6} #[0-9a-f]{6}([[:space:]]|$)",
				REG_EXTENDED|REG_NOSUB|REG_ICASE);
	}

	if(m->pos + 23 > m->end || regexec(&reg, m->pos, 0, 0, 0))
		return "bad value";

	c = m->pos[23];
	m->pos[23] = '\0';
	loadcolor(col, m->pos);
	m->pos[23] = c;

	m->pos += 23;
	eatrunes(m, isspacerune, 1);
	return nil;
}

char *
message_root(void *p, Message *m) {
	Font *fn;
	char *s, *ret;
	ulong n;

	ret = nil;
	s = getword(m);

	switch(getsym(s)) {
	case LQUIT:
		srv.running = 0;
		break;
	case LEXEC:
		execstr = emalloc(strlen(m->pos) + sizeof("exec "));
		sprintf(execstr, "exec %s", m->pos);
		srv.running = 0;
		break;
	case LVIEW:
		select_view(m->pos);
		break;
	case LSELCOLORS:
		fprintf(stderr, "%s: warning: selcolors have been removed\n", argv0);
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
		fn = loadfont(m->pos);
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

		if(!(n & (Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask)))
			return Ebadvalue;

		strncpy(def.grabmod, s, sizeof(def.grabmod));
		def.mod = n;
		break;
	default:
		return Ebadcmd;
	}
	return ret;
}

char *
read_root_ctl(void) {
	uint i = 0;
	i += snprintf(&buffer[i], (sizeof(buffer) - i), "view %s\n", screen->sel->name);
	i += snprintf(&buffer[i], (sizeof(buffer) - i), "focuscolors %s\n", def.focuscolor.colstr);
	i += snprintf(&buffer[i], (sizeof(buffer) - i), "normcolors %s\n", def.normcolor.colstr);
	i += snprintf(&buffer[i], (sizeof(buffer) - i), "font %s\n", def.font->name);
	i += snprintf(&buffer[i], (sizeof(buffer) - i), "grabmod %s\n", def.grabmod);
	i += snprintf(&buffer[i], (sizeof(buffer) - i), "border %d\n", def.border);
	return buffer;
}

char *
message_client(Client *c, Message *m) {
	char *s;
	
	s = getword(m);

	switch(getsym(s)) {
	case LKILL:
		kill_client(c);
		break;
	case LURGENT:
		set_urgent(c, True, True);
		break;
	case LNOTURGENT:
		set_urgent(c, False, True);
		break;
	case LFULLSCREEN:
		fullscreen(c, True);
		break;
	case LNOTFULLSCREEN:
		fullscreen(c, False);
		break;
	default:
		return Ebadcmd;
	}
	return nil;
}

static char*
send_frame(Frame *f, int sym, Bool swap) {
	Frame *fp;
	Area *a;
	
	a = f->area;

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
send_client(View *v, Message *m, Bool swap) {
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
	case LTOGGLE:
		if(!a->floating)
			to = v->area;
		else if(c->revert && !c->revert->floating)
			to = c->revert;
		else
			to = v->area->next;
		break;
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
	default:
		if(!getulong(s, &i) || i == 0)
			return Ebadvalue;

		for(to=v->area; to; to=to->next)
			if(!i--) break;
		break;
	}
	
	if(!to && !swap && (f->anext || f != a->frame))
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
select_frame(Frame *f, int sym) {
	Frame *fp;
	Area *a;

	if(!f)
		return Ebadvalue;
	a = f->area;

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
	default:
		assert(!"can't get here");
	}

	focus_frame(fp, False);
	frame_to_top(fp);
	if(f->view == screen->sel)
		restack_view(f->view);
	return nil;
}

char*
select_area(Area *a, Message *m) {
	Area *ap;
	View *v;
	char *s;
	ulong i;
	int sym;

	v = a->view;
	s = getword(m);
	sym = getsym(s);
	
	switch(sym) {
	case LUP:
	case LDOWN:
		return select_frame(a->sel, sym);
	case LTOGGLE:
		if(!a->floating)
			ap = v->area;
		else if(v->revert && v->revert != a)
			ap = v->revert;
		else
			ap = v->area->next;
		break;
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
		if(!getulong(s, &i) || i == 0)
			return Ebadvalue;
		for(ap=v->area->next; ap; ap=ap->next)
			if(!--i) break;
	}

	focus_area(ap);
	return nil;
}
