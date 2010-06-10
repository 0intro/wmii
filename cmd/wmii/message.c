/* Copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <ctype.h>
#include "fns.h"

static char* msg_grow(View*, IxpMsg*);
static char* msg_nudge(View*, IxpMsg*);
static char* msg_selectframe(Area*, IxpMsg*, int);
static char* msg_sendframe(Frame*, int, bool);

#define DIR(s) (\
	s == LUP	? North : \
	s == LDOWN	? South : \
	s == LLEFT	? West  : \
	s == LRIGHT	? East  : \
	(error(Ebadvalue), 0))

static char
	Ebadcmd[] = "bad command",
	Ebadvalue[] = "bad value",
	Ebadusage[] = "bad usage";

/* Edit |sort Edit |sed 's/"([^"]+)"/L\1/g' | tr 'a-z' 'A-Z' */
enum {
	LBAR,
	LBORDER,
	LCLIENT,
	LCOLMODE,
	LDEBUG,
	LDOWN,
	LEXEC,
	LFLOATING,
	LFOCUSCOLORS,
	LFONT,
	LFONTPAD,
	LFULLSCREEN,
	LGRABMOD,
	LGROUP,
	LGROW,
	LINCMODE,
	LKILL,
	LLEFT,
	LNORMCOLORS,
	LNUDGE,
	LOFF,
	LON,
	LQUIT,
	LRIGHT,
	LSELCOLORS,
	LSELECT,
	LSEND,
	LSLAY,
	LSPAWN,
	LSWAP,
	LTAGS,
	LTOGGLE,
	LUP,
	LURGENT,
	LVIEW,
	LTILDE,
};
char *symtab[] = {
	"bar",
	"border",
	"client",
	"colmode",
	"debug",
	"down",
	"exec",
	"floating",
	"focuscolors",
	"font",
	"fontpad",
	"fullscreen",
	"grabmod",
	"group",
	"grow",
	"incmode",
	"kill",
	"left",
	"normcolors",
	"nudge",
	"off",
	"on",
	"quit",
	"right",
	"selcolors",
	"select",
	"send",
	"slay",
	"spawn",
	"swap",
	"tags",
	"toggle",
	"up",
	"urgent",
	"view",
	"~",
};

char* debugtab[] = {
	"9p",
	"dnd",
	"event",
	"ewmh",
	"focus",
	"generic",
	"stack",
};

static char* barpostab[] = {
	"bottom",
	"top",
};
static char* incmodetab[] = {
	"ignore",
	"show",
	"squeeze",
};
static char* floatingtab[] = {
	"never", "off", "on", "always"
};
static char* toggletab[] = {
	"off", "on", "toggle",
};

/* Edit ,y/^[a-zA-Z].*\n.* {\n/d
 * Edit s/^([a-zA-Z].*)\n(.*) {\n/\1 \2;\n/
 * Edit ,x/^static.*\n/d
 */

static int
_bsearch(char *from, char **tab, int ntab) {
	int i, n, m, cmp;
	char *to, *end;
	Rune r;

	if(from == nil)
		return -1;

	end = buffer + sizeof buffer - UTFmax - 1;
	for(to=buffer; *from && to < end;) {
		from += chartorune(&r, from);
		if(r != 0x80) {
			r = tolowerrune(r);
			to += runetochar(to, &r);
		}
	}
	*to = '\0';
	to = buffer;

	n = ntab;
	i = 0;
	while(n) {
		m = n/2;
		cmp = strcmp(to, tab[i+m]);
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
_lsearch(char *from, char **tab, int ntab) {
	int i;

	if(from != nil)
		for(i=0; i < ntab; i++)
			if(!strcmp(from, tab[i]))
				return i;
	error(Ebadvalue);
	return 0;
}

static int
getsym(char *s) {
	return _bsearch(s, symtab, nelem(symtab));
}

static void
setdef(int *ptr, char *s, char *tab[], int ntab) {
	int i;

	i = _bsearch(s, tab, ntab);
	if(i < 0)
		error(Ebadvalue);
	*ptr = i;
}

static int
gettoggle(char *s) {
	return _lsearch(s, toggletab, nelem(toggletab));
}

static int
getdirection(IxpMsg *m) {
	int i;

	switch(i = getsym(msg_getword(m))) {
	case LLEFT:
	case LRIGHT:
	case LUP:
	case LDOWN:
		return i;
	}
	error(Ebadusage);
	return -1;
}

static ulong
msg_getulong(const char *s) {
	ulong l;

	if(!(s && getulong(s, &l)))
		error(Ebadvalue);
	return l;
}

static long
msg_getlong(const char *s) {
	long l;

	if(!(s && getlong(s, &l)))
		error(Ebadvalue);
	return l;
}

void
msg_eatrunes(IxpMsg *m, int (*p)(Rune), int val) {
	Rune r;
	int n;

	while(m->pos < m->end) {
		n = chartorune(&r, m->pos);
		if(p(r) != val)
			break;
		m->pos += n;
	}
	if(m->pos > m->end)
		m->pos = m->end;
}

char*
msg_getword(IxpMsg *m) {
	char *ret;
	Rune r;
	int n;

	msg_eatrunes(m, isspacerune, true);
	ret = m->pos;
	msg_eatrunes(m, isspacerune, false);
	n = chartorune(&r, m->pos);
	*m->pos = '\0';
	m->pos += n;
	msg_eatrunes(m, isspacerune, true);

	/* Filter out comments. */
	if(*ret == '#') {
		*ret = '\0';
		m->pos = m->end;
	}
	if(*ret == '\\')
		if(ret[1] == '\\' || ret[1] == '#')
			ret++;
	if(*ret == '\0')
		return nil;
	return ret;
}

static Client*
strclient(View *v, char *s) {
	Client *c;

	/*
	 * sel
	 * 0x<window xid>
	 */

	if(s && !strcmp(s, "sel"))
		c = view_selclient(v);
	else
		c = win2client(msg_getulong(s));
	if(c == nil)
		error(Ebadvalue);
	return c;
}

Area*
strarea(View *v, ulong scrn, const char *area) {
	Area *a;
	const char *screen;
	char *p;
	long i;

	/*
	 * sel
	 * ~
	 * <column number>
	 */

	if(area == nil)
		error(Ebadvalue);

	if((p = strchr(area, ':'))) {
		/* <screen>:<area> */
		*p++ = '\0';
		screen = area;
		area = p;

		if(!strcmp(screen, "sel"))
			scrn = v->selscreen;
		else
			scrn = msg_getulong(screen);
	}
	else if(!strcmp(area, "sel"))
		return v->sel;

	if(!strcmp(area, "sel")) {
		if(scrn != v->selscreen)
			error(Ebadvalue);
		return v->sel;
	}

	if(!strcmp(area, "~"))
		return v->floating;

	if(scrn < 0)
		error(Ebadvalue);

	i = msg_getlong(area);
	if(i == 0)
		error(Ebadvalue);

	if(i > 0) {
		for(a = v->areas[scrn]; a; a = a->next)
			if(i-- == 1) break;
	}
	else {
		/* FIXME: Switch to circularly linked list. */
		for(a = v->areas[scrn]; a->next; a = a->next)
			;
		for(; a; a = a->prev)
			if(++i == 0) break;
	}
	if(a == nil)
		error(Ebadvalue);
	return a;
}

static Frame*
getframe(View *v, int scrn, IxpMsg *m) {
	Frame *f;
	Area *a;
	char *s;
	ulong l;

	s = msg_getword(m);
	if(!s || !strcmp(s, "client"))
		f = client_viewframe(strclient(v, msg_getword(m)),
				     v);
	else {
		/* XXX: Multihead */
		a = strarea(v, scrn, s);

		s = msg_getword(m);
		f = nil;
		if(s && !strcmp(s, "sel"))
			f = a->sel;
		else {
			l = msg_getulong(s);
			for(f=a->frame; f; f=f->anext)
				if(--l == 0) break;
		}
	}
	if(f == nil)
		error(Ebadvalue);
	return f;
}

char*
readctl_client(Client *c) {
	bufclear();
	bufprint("%#C\n", c);
	bufprint("floating %s\n", floatingtab[c->floating + 1]);
	if(c->fullscreen >= 0)
		bufprint("fullscreen %d\n", c->fullscreen);
	else
		bufprint("fullscreen off\n");
	bufprint("group %#ulx\n", c->group ? c->group->leader : 0);
	if(c->pid)
		bufprint("pid %d\n", c->pid);
	bufprint("tags %s\n", c->tags);
	bufprint("urgent %s\n", TOGGLE(c->urgent));
	return buffer;
}

char*
message_client(Client *c, IxpMsg *m) {
	char *s;
	long l;

	s = msg_getword(m);

	/*
	 * Toggle ::= on
	 *	    | off
	 *	    | toggle
	 *	    | <screen>
	 * floating <toggle>
	 * fullscreen <toggle>
	 * kill
	 * slay
	 * tags <tags>
	 * urgent <toggle>
	 */

	switch(getsym(s)) {
	case LFLOATING:
		c->floating = -1 + _lsearch(msg_getword(m), floatingtab, nelem(floatingtab));
		break;
	case LFULLSCREEN:
		s = msg_getword(m);
		if(getlong(s, &l))
			fullscreen(c, On, l);
		else
			fullscreen(c, gettoggle(s), -1);
		break;
	case LGROUP:
		group_remove(c);
		c->w.hints->group = msg_getulong(msg_getword(m));
		if(c->w.hints->group)
			group_init(c);
		break;
	case LKILL:
		client_kill(c, true);
		break;
	case LSLAY:
		client_kill(c, false);
		break;
	case LTAGS:
		client_applytags(c, m->pos);
		break;
	case LURGENT:
		client_seturgent(c, gettoggle(msg_getword(m)), UrgManager);
		break;
	default:
		error(Ebadcmd);
	}
	return nil;
}

char*
message_root(void *p, IxpMsg *m) {
	Font *fn;
	char *s, *ret;
	ulong n;
	int i;

	USED(p);
	ret = nil;
	s = msg_getword(m);
	if(s == nil)
		return nil;

	if(!strcmp(s, "backtrace")) {
		backtrace(m->pos);
		return nil;
	}

	switch(getsym(s)) {
	case LBAR: /* bar on? <"top" | "bottom"> */
		s = msg_getword(m);
		if(!strcmp(s, "on"))
			s = msg_getword(m);
		setdef(&screen->barpos, s, barpostab, nelem(barpostab));
		view_update(selview);
		break;
	case LBORDER:
		def.border = msg_getulong(msg_getword(m));;
		view_update(selview);
		break;
	case LCOLMODE:
		setdef(&def.colmode, msg_getword(m), modes, Collast);
		break;
	case LDEBUG:
		ret = msg_debug(m);
		break;
	case LEXEC:
		execstr = strdup(m->pos);
		srv.running = 0;
		break;
	case LSPAWN:
		spawn_command(m->pos);
		break;
	case LFOCUSCOLORS:
		msg_parsecolors(m, &def.focuscolor);
		view_update(selview);
		break;
	case LFONT:
		fn = loadfont(m->pos);
		if(fn) {
			freefont(def.font);
			def.font = fn;
			for(n=0; n < nscreens; n++)
				bar_resize(screens[n]);
		}else
			ret = "can't load font";
		view_update(selview);
		break;
	case LFONTPAD:
		if(!getint(msg_getword(m), &def.font->pad.min.x) ||
		   !getint(msg_getword(m), &def.font->pad.max.x) ||
		   !getint(msg_getword(m), &def.font->pad.max.y) ||
		   !getint(msg_getword(m), &def.font->pad.min.y))
			ret = "invalid rectangle";
		else {
			for(n=0; n < nscreens; n++)
				bar_resize(screens[n]);
			view_update(selview);
		}
		break;
	case LGRABMOD:
		s = msg_getword(m);
		if(!parsekey(s, &i, nil) || i == 0)
			return Ebadvalue;

		utflcpy(def.grabmod, s, sizeof def.grabmod);
		def.mod = i;
		break;
	case LINCMODE:
		setdef(&def.incmode, msg_getword(m), incmodetab, nelem(incmodetab));
		view_update(selview);
		break;
	case LNORMCOLORS:
		msg_parsecolors(m, &def.normcolor);
		view_update(selview);
		break;
	case LSELCOLORS:
		warning("selcolors have been removed");
		return Ebadcmd;
	case LVIEW:
		view_select(m->pos);
		break;
	case LQUIT:
		srv.running = 0;
		break;
	default:
		return Ebadcmd;
	}
	return ret;
}

static void
printdebug(int mask) {
	int i, j;

	for(i=0, j=0; i < nelem(debugtab); i++)
		if(mask & (1<<i)) {
			if(j++ > 0) bufprint(" ");
			bufprint("%s", debugtab[i]);
		}
}

char*
readctl_root(void) {
	bufclear();
	bufprint("bar on %s\n", barpostab[screen->barpos]);
	bufprint("border %d\n", def.border);
	bufprint("colmode %s\n", modes[def.colmode]);
	if(debugflag) {
		bufprint("debug ");
		printdebug(debugflag);
		bufprint("\n");
	}
	if(debugfile) {
		bufprint("debugfile ");
		printdebug(debugfile);
		bufprint("\n");
	}
	bufprint("focuscolors %s\n", def.focuscolor.colstr);
	bufprint("font %s\n", def.font->name);
	bufprint("fontpad %d %d %d %d\n", def.font->pad.min.x, def.font->pad.max.x,
		 def.font->pad.max.y, def.font->pad.min.y);
	bufprint("grabmod %s\n", def.grabmod);
	bufprint("incmode %s\n", incmodetab[def.incmode]);
	bufprint("normcolors %s\n", def.normcolor.colstr);
	bufprint("view %s\n", selview->name);
	return buffer;
}

char*
message_view(View *v, IxpMsg *m) {
	Area *a;
	char *s;

	s = msg_getword(m);
	if(s == nil)
		return nil;

	/*
	 * area ::= ~
	 *        | <column number>
	 *        | sel
	 * direction ::= left
	 *             | right
	 *             | up
	 *             | down
	 * # This *should* be identical to <frame>
	 * place ::= <column number>
	 *	  #| ~ # This should be, but isn't.
	 *	   | <direction>
	 *         | toggle
	 * colmode ::= default
	 *           | stack
	 *           | normal
	 * column ::= sel
	 *          | <column number>
	 * frame ::= up
	 *         | down
	 *         | left
	 *         | right
	 *         | toggle
	 *         | client <window xid>
	 *         | sel
	 *         | ~
	 *         | <column> <frame number>
	 *         | <column>
	 * amount ::=
	 *	    | <number>
	 *          | <number>px
	 *
	 * colmode <area> <colmode>
	 * select <area>
	 * send <frame> <place>
	 * swap <frame> <place>
	 * grow <thing> <direction> <amount>
	 * nudge <thing> <direction> <amount>
	 * select <ordframe>
	 */

	switch(getsym(s)) {
	case LCOLMODE:
		s = msg_getword(m);
		a = strarea(v, screen->idx, s);

		s = msg_getword(m);
		if(s == nil || !column_setmode(a, s))
			return Ebadvalue;

		column_arrange(a, false);
		view_restack(v);

		view_update(v);
		return nil;
	case LGROW:
		return msg_grow(v, m);
	case LNUDGE:
		return msg_nudge(v, m);
	case LSELECT:
		return msg_selectarea(v->sel, m);
	case LSEND:
		return msg_sendclient(v, m, false);
	case LSWAP:
		return msg_sendclient(v, m, true);
	default:
		return Ebadcmd;
	}
	/* not reached */
}

char*
readctl_view(View *v) {
	Area *a;
	int s;

	bufclear();
	bufprint("%s\n", v->name);

	/* select <area>[ <frame>] */
	bufprint("select %a", v->sel);
	if(v->sel->sel)
		bufprint(" %d", frame_idx(v->sel->sel));
	bufprint("\n");

	/* select client <client> */
	if(v->sel->sel)
		bufprint("select client %#C\n", v->sel->sel->client);

	foreach_area(v, s, a)
		bufprint("colmode %a %s\n", a, column_getmode(a));
	return buffer;
}

char*
msg_debug(IxpMsg *m) {
	char *opt;
	int d;
	char add;

	bufclear();
	while((opt = msg_getword(m))) {
		add = '+';
		if(opt[0] == '+' || opt[0] == '-')
			add = *opt++;
		d = _bsearch(opt, debugtab, nelem(debugtab));
		if(d == -1) {
			bufprint(", %s", opt);
			continue;
		}
		if(add == '+')
			debugflag |= 1<<d;
		else
			debugflag &= ~(1<<d);
	}
	if(buffer[0] != '\0')
		return sxprint("Bad debug options: %s", buffer+2);
	return nil;
}

static void
getamt(IxpMsg *m, Point *amt) {
	char *s, *p;
	long l;

	s = msg_getword(m);
	if(s) {
		p = strend(s, 2);
		if(!strcmp(p, "px")) {
			*p = '\0';
			amt->x = 1;
			amt->y = 1;
		}

		l = msg_getlong(s);
		amt->x *= l;
		amt->y *= l;
	}
}

static char*
msg_grow(View *v, IxpMsg *m) {
	Client *c;
	Frame *f;
	Rectangle r;
	Point amount;
	int dir;

	f = getframe(v, screen->idx, m);
	c = f->client;

	dir = getdirection(m);

	amount.x = Dy(f->titlebar);
	amount.y = Dy(f->titlebar);
	if(amount.x < c->w.hints->inc.x)
		amount.x = c->w.hints->inc.x;
	if(amount.y < c->w.hints->inc.y)
		amount.y = c->w.hints->inc.y;
	getamt(m, &amount);

	if(f->area->floating)
		r = f->r;
	else
		r = f->colr;
	switch(dir) {
	case LLEFT:	r.min.x -= amount.x; break;
	case LRIGHT:	r.max.x += amount.x; break;
	case LUP:	r.min.y -= amount.y; break;
	case LDOWN:	r.max.y += amount.y; break;
	default:	abort();
	}

	if(f->area->floating)
		float_resizeframe(f, r);
	else
		column_resizeframe(f, r);

	return nil;
}

static char*
msg_nudge(View *v, IxpMsg *m) {
	Frame *f;
	Rectangle r;
	Point amount;
	int dir;

	f = getframe(v, screen->idx, m);
	dir = getdirection(m);

	amount.x = Dy(f->titlebar);
	amount.y = Dy(f->titlebar);
	getamt(m, &amount);

	if(f->area->floating)
		r = f->r;
	else
		r = f->colr;
	switch(dir) {
	case LLEFT:	r = rectaddpt(r, Pt(-amount.x, 0)); break;
	case LRIGHT:	r = rectaddpt(r, Pt( amount.x, 0)); break;
	case LUP:	r = rectaddpt(r, Pt(0, -amount.y)); break;
	case LDOWN:	r = rectaddpt(r, Pt(0,  amount.y)); break;
	default:	abort();
	}

	if(f->area->floating)
		float_resizeframe(f, r);
	else
		column_resizeframe(f, r);
	return nil;
}

void
msg_parsecolors(IxpMsg *m, CTuple *col) {
	static char Ebad[] = "bad color string";
	Rune r;
	char c, *p;
	int i, j;

	/* '#%6x #%6x #%6x' */
	p = m->pos;
	for(i = 0; i < 3 && p < m->end; i++) {
		if(*p++ != '#')
			error(Ebad);
		for(j = 0; j < 6; j++)
			if(p >= m->end || !isxdigit(*p++))
				error(Ebad);

		chartorune(&r, p);
		if(i < 2) {
			if(r != ' ')
				error(Ebad);
			p++;
		}else if(*p != '\0' && !isspacerune(r))
			error(Ebad);
	}

	c = *p;
	*p = '\0';
	loadcolor(col, m->pos);
	*p = c;

	m->pos = p;
	msg_eatrunes(m, isspacerune, true);
}

char*
msg_selectarea(Area *a, IxpMsg *m) {
	Frame *f;
	Area *ap;
	View *v;
	char *s;
	ulong i;
	int sym;

	v = a->view;
	s = msg_getword(m);
	sym = getsym(s);

	switch(sym) {
	case LTOGGLE:
		if(!a->floating)
			ap = v->floating;
		else if(v->revert && v->revert != a)
			ap = v->revert;
		else
			ap = v->firstarea;
		break;
	case LLEFT:
	case LRIGHT:
	case LUP:
	case LDOWN:
	case LCLIENT:
		return msg_selectframe(a, m, sym);
	case LTILDE:
		ap = v->floating;
		break;
	default:
		/* XXX: Multihead */
		ap = strarea(v, a->screen, s);
		if(ap->floating)
			return Ebadvalue;
		if((s = msg_getword(m))) {
			i = msg_getulong(s);
			for(f = ap->frame; f; f = f->anext)
				if(--i == 0) break;
			if(i != 0)
				return Ebadvalue;
			frame_focus(f);
			return nil;
		}
	}

	area_focus(ap);
	return nil;
}

static char*
msg_selectframe(Area *a, IxpMsg *m, int sym) {
	Client *c;
	Frame *f, *fp;
	char *s;
	bool stack;
	ulong i, dy;

	f = a->sel;
	fp = f;

	stack = false;
	if(sym == LUP || sym == LDOWN)
		if((s = msg_getword(m)))
			if(!strcmp(s, "stack"))
				stack = true;
			else
				return Ebadvalue;

	if(sym == LCLIENT) {
		s = msg_getword(m);
		i = msg_getulong(s);
		c = win2client(i);
		if(c == nil)
			return Ebadvalue;
		f = client_viewframe(c, a->view);
		if(!f)
			return Ebadvalue;
	}
	else if(!find(&a, &f, DIR(sym), true, stack))
		return Ebadvalue;

	area_focus(a);

	if(f != nil) {
		/* XXX */
		if(fp && fp->area == a)
		if(f->collapsed && !f->area->floating && f->area->mode == Coldefault) {
			dy = Dy(f->colr);
			f->colr.max.y = f->colr.min.y + Dy(fp->colr);
			fp->colr.max.y = fp->colr.min.y + dy;
			column_arrange(a, false);
		}

		frame_focus(f);
		frame_restack(f, nil);
		if(f->view == selview)
			view_restack(a->view);
	}
	return nil;
}

static char*
sendarea(Frame *f, Area *to, bool swap) {
	Client *c;

	c = f->client;
	if(!to)
		return Ebadvalue;

	if(!swap)
		area_moveto(to, f);
	else if(to->sel)
		frame_swap(f, to->sel);
	else
		return Ebadvalue;

	frame_focus(client_viewframe(c, f->view));
	/* view_arrange(v); */
	view_update_all();
	return nil;
}

char*
msg_sendclient(View *v, IxpMsg *m, bool swap) {
	Area *to, *a;
	Frame *f, *ff;
	Client *c;
	char *s;
	int sym;

	s = msg_getword(m);
	c = strclient(v, s);
	f = client_viewframe(c, v);
	if(f == nil)
		return Ebadvalue;

	a = f->area;
	to = nil;

	s = msg_getword(m);
	sym = getsym(s);

	/* FIXME: Should use a helper function. */
	switch(sym) {
	case LUP:
	case LDOWN:
		return msg_sendframe(f, sym, swap);
	case LLEFT:
		if(a->floating)
			return Ebadvalue;
		to = a->prev;
		break;
	case LRIGHT:
		if(a->floating)
			return Ebadvalue;
		to = a->next;
		break;
	case LTOGGLE:
		if(!a->floating)
			to = v->floating;
		else if(f->column)
			to = view_findarea(v, f->screen, f->column, true);
		else
			to = view_findarea(v, v->selscreen, v->selcol, true);
		break;
	case LTILDE:
		if(a->floating)
			return Ebadvalue;
		to = v->floating;
		break;
	default:
		to = strarea(v, v->selscreen, s);
		// to = view_findarea(v, scrn, i, true);
		break;
	}


	if(!to && !swap) {
		/* XXX: Multihead - clean this up, move elsewhere. */
		if(!f->anext && f == f->area->frame) {
			ff = f;
			to = a;
			if(!find(&to, &ff, DIR(sym), false, false))
				return Ebadvalue;
		}
		else {
			to = (sym == LLEFT) ? nil : a;
			to = column_new(v, to, a->screen, 0);
		}
	}

	return sendarea(f, to, swap);
}

static char*
msg_sendframe(Frame *f, int sym, bool swap) {
	Client *c;
	Area *a;
	Frame *fp;

	SET(fp);
	c = f->client;

	a = f->area;
	fp = f;
	if(!find(&a, &fp, DIR(sym), false, false))
		return Ebadvalue;
	if(a != f->area)
		return sendarea(f, a, swap);

	switch(sym) {
	case LUP:
		fp = f->aprev;
		if(!fp)
			return Ebadvalue;
		if(!swap)
			fp = fp->aprev;
		break;
	case LDOWN:
		fp = f->anext;
		if(!fp)
			return Ebadvalue;
		break;
	default:
		die("can't get here");
	}

	if(swap)
		frame_swap(f, fp);
	else {
		frame_remove(f);
		frame_insert(f, fp);
	}

	/* view_arrange(f->view); */

	frame_focus(client_viewframe(c, f->view));
	view_update_all();
	return nil;
}

void
warning(const char *fmt, ...) {
	va_list ap;
	char *s;

	va_start(ap, fmt);
	s = vsmprint(fmt, ap);
	va_end(ap);

	event("Warning %s\n", s);
	fprint(2, "%s: warning: %s\n", argv0, s);
	free(s);
}

