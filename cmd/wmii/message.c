/* Copyright ©2006-2008 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <ctype.h>
#include "fns.h"

static char* msg_debug(IxpMsg*);
static char* msg_grow(View*, IxpMsg*);
static char* msg_nudge(View*, IxpMsg*);
static char* msg_selectframe(Frame*, IxpMsg*, int);
static char* msg_sendframe(Frame*, int, bool);

static char
	Ebadcmd[] = "bad command",
	Ebadvalue[] = "bad value",
	Ebadusage[] = "bad usage";

/* Edit |sort Edit |sed 's/"([^"]+)"/L\1/g' | tr 'a-z' 'A-Z' */
enum {
	LFULLSCREEN,
	LURGENT,
	LBAR,
	LBORDER,
	LCLIENT,
	LCOLMODE,
	LDEBUG,
	LDOWN,
	LEXEC,
	LFOCUSCOLORS,
	LFONT,
	LGRABMOD,
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
	LSWAP,
	LTOGGLE,
	LUP,
	LVIEW,
	LTILDE,
};
char *symtab[] = {
	"Fullscreen",
	"Urgent",
	"bar",
	"border",
	"client",
	"colmode",
	"debug",
	"down",
	"exec",
	"focuscolors",
	"font",
	"grabmod",
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
	"swap",
	"toggle",
	"up",
	"view",
	"~",
};

char* debugtab[] = {
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

/* Edit ,y/^[a-zA-Z].*\n.* {\n/d
 * Edit s/^([a-zA-Z].*)\n(.*) {\n/\1 \2;\n/
 * Edit ,x/^static.*\n/d
 */

static int
_bsearch(char *s, char **tab, int ntab) {
	int i, n, m, cmp;

	if(s == nil)
		return -1;

	n = ntab;
	i = 0;
	while(n) {
		m = n/2;
		cmp = strcmp(s, tab[i+m]);
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
getsym(char *s) {
	return _bsearch(s, symtab, nelem(symtab));
}

static bool
setdef(int *ptr, char *s, char *tab[], int ntab) {
	int i;

	i = _bsearch(s, tab, ntab);
	if(i >= 0)
		*ptr = i;
	return i >= 0;
}

static int
gettoggle(IxpMsg *m) {
	switch(getsym(msg_getword(m))) {
	case LON:	return On;
	case LOFF:	return Off;
	case LTOGGLE:	return Toggle;
	default:
		return -1;
	}
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
	default:
		return -1;
	}
}

static void
eatrunes(IxpMsg *m, int (*p)(Rune), int val) {
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

	eatrunes(m, isspacerune, true);
	ret = m->pos;
	eatrunes(m, isspacerune, false);
	n = chartorune(&r, m->pos);
	*m->pos = '\0';
	m->pos += n;
	eatrunes(m, isspacerune, true);

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

#define strbcmp(str, const) (strncmp((str), (const), sizeof(const)-1))	
static int
getbase(const char **s, long *sign) {
	const char *p;
	int ret;

	ret = 10;
	*sign = 1;
	if(**s == '-') {
		*sign = -1;
		*s += 1;
	}else if(**s == '+')
		*s += 1;

	p = *s;
	if(!strbcmp(p, "0x")) {
		*s += 2;
		ret = 16;
	}
	else if(isdigit(p[0])) {
		if(p[1] == 'r') {
			*s += 2;
			ret = p[0] - '0';
		}
		else if(isdigit(p[1]) && p[2] == 'r') {
			*s += 3;
			ret = 10*(p[0]-'0') + (p[1]-'0');
		}
	}
	else if(p[0] == '0') {
		ret = 8;
	}
	if(ret != 10 && (**s == '-' || **s == '+'))
		*sign = 0;
	return ret;
}

bool
getlong(const char *s, long *ret) {
	const char *end;
	char *rend;
	int base;
	long sign;

	end = s+strlen(s);
	base = getbase(&s, &sign);
	if(sign == 0)
		return false;

	*ret = sign * strtol(s, &rend, base);
	return (end == rend);
}

bool
getulong(const char *s, ulong *ret) {
	const char *end;
	char *rend;
	int base;
	long sign;

	end = s+strlen(s);
	base = getbase(&s, &sign);
	if(sign < 1)
		return false;

	*ret = strtoul(s, &rend, base);
	return (end == rend);
}

static char*
strend(char *s, int n) {
	int len;

	len = strlen(s);
	return s + max(0, len - n);
}

static Client*
strclient(View *v, char *s) {
	ulong id;

	/*
	 * sel
	 * 0x<window xid>
	 */

	if(s == nil)
		return nil;
	if(!strcmp(s, "sel"))
		return view_selclient(v);
	if(getulong(s, &id))
		return win2client(id);

	return nil;
}

Area*
strarea(View *v, const char *s) {
	Area *a;
	long i;

	/*
	 * sel
	 * ~
	 * <column number>
	 */

	if(!strcmp(s, "sel"))
		return v->sel;
	if(!strcmp(s, "~"))
		return v->floating;
	if(!getlong(s, &i) || i == 0)
		return nil;

	/* FIXME: Very broken! */
	if(i > 0) {
		for(a = v->firstarea; a; a = a->next)
			if(i-- == 0) break;
	}
	else {
		for(a = v->firstarea; a->next; a = a->next)
			;
		for(; a != v->firstarea; a = a->prev)
			if(++i == 0) break;
		if(a == v->firstarea)
			a = nil;
	}
	return a;
}

static Frame*
getframe(View *v, IxpMsg *m) {
	Client *c;
	Frame *f;
	Area *a;
	char *s;
	ulong l;

	s = msg_getword(m);
	if(!strcmp(s, "client")) {
		c = strclient(v, msg_getword(m));
		if(c == nil)
			return nil;
		return client_viewframe(c, v);
	}

	a = strarea(v, s);
	if(a == nil) {
		print("a == nil\n");
		return nil;
	}

	s = msg_getword(m);
	if(!s)
		return nil;
	if(!strcmp(s, "sel"))
		return a->sel;
	if(!getulong(s, &l))
		return nil;
	for(f=a->frame; f; f=f->anext)
		if(--l == 0) break;
	return f;
}

char*
message_client(Client *c, IxpMsg *m) {
	char *s;
	int i;

	s = msg_getword(m);

	/*
	 * Toggle ::= on
	 *	    | off
	 *	    | toggle
	 * Fullscreen <toggle>
	 * Urgent <toggle>
	 * kill
	 * slay
	 */

	switch(getsym(s)) {
	case LFULLSCREEN:
		i = gettoggle(m);
		if(i == -1)
			return Ebadusage;
		fullscreen(c, i);
		break;
	case LKILL:
		client_kill(c, true);
		break;
	case LSLAY:
		client_kill(c, false);
		break;
	case LURGENT:
		i = gettoggle(m);
		if(i == -1)
			return Ebadusage;
		client_seturgent(c, i, UrgManager);
		break;
	default:
		return Ebadcmd;
	}
	return nil;
}

char*
message_root(void *p, IxpMsg *m) {
	Font *fn;
	char *s, *ret;
	ulong n;

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
		if(!setdef(&screen->barpos, s, barpostab, nelem(barpostab)))
			return Ebadvalue;
		view_update(screen->sel);
		break;
	case LBORDER:
		if(!getulong(msg_getword(m), &n))
			return Ebadvalue;
		def.border = n;
		view_update(screen->sel);
		break;
	case LDEBUG:
		ret = msg_debug(m);
		break;
	case LEXEC:
		execstr = strdup(m->pos);
		srv.running = 0;
		break;
	case LFOCUSCOLORS:
		ret = msg_parsecolors(m, &def.focuscolor);
		view_update(screen->sel);
		break;
	case LFONT:
		fn = loadfont(m->pos);
		if(fn) {
			freefont(def.font);
			def.font = fn;
			bar_resize(screen);
		}else
			ret = "can't load font";
		view_update(screen->sel);
		break;
	case LGRABMOD:
		s = msg_getword(m);
		n = str2modmask(s);

		if((n & (Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask)) == 0)
			return Ebadvalue;

		utflcpy(def.grabmod, s, sizeof def.grabmod);
		def.mod = n;
		break;
	case LINCMODE:
		if(!setdef(&def.incmode, msg_getword(m), incmodetab, nelem(incmodetab)))
			return Ebadvalue;
		view_update(screen->sel);
		break;
	case LNORMCOLORS:
		ret = msg_parsecolors(m, &def.normcolor);
		view_update(screen->sel);
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
	bufprint("grabmod %s\n", def.grabmod);
	bufprint("incmode %s\n", incmodetab[screen->barpos]);
	bufprint("normcolors %s\n", def.normcolor.colstr);
	bufprint("view %s\n", screen->sel->name);
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
		a = strarea(v, s);
		if(a == nil) /* || a->floating) */
			return Ebadvalue;

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
	uint i;

	bufclear();
	bufprint("%s\n", v->name);

	/* select <area>[ <frame>] */
	bufprint("select %a", v->sel);
	if(v->sel->sel)
		bufprint(" %d", frame_idx(v->sel->sel));
	bufprint("\n");

	/* select client <client> */
	if(v->sel->sel)
		bufprint("select client %C\n", v->sel->sel->client);

	for(a = v->firstarea, i = 1; a; a = a->next, i++)
		bufprint("colmode %d %s\n", i, column_getmode(a));
	return buffer;
}

static char*
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

static bool
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

		if(!getlong(s, &l))
			return false;
		amt->x *= l;
		amt->y *= l;
	}
	return true;
}

static char*
msg_grow(View *v, IxpMsg *m) {
	Client *c;
	Frame *f;
	Rectangle r;
	Point amount;
	int dir;

	f = getframe(v, m);
	if(f == nil)
		return "bad frame";
	c = f->client;

	dir = getdirection(m);
	if(dir == -1)
		return "bad direction";

	amount.x = Dy(f->titlebar);
	amount.y = Dy(f->titlebar);
	if(amount.x < c->w.hints->inc.x)
		amount.x = c->w.hints->inc.x;
	if(amount.y < c->w.hints->inc.y)
		amount.y = c->w.hints->inc.y;

	if(!getamt(m, &amount))
		return Ebadvalue;

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

	f = getframe(v, m);
	if(f == nil)
		return "bad frame";

	dir = getdirection(m);
	if(dir == -1)
		return "bad direction";

	amount.x = Dy(f->titlebar);
	amount.y = Dy(f->titlebar);
	if(!getamt(m, &amount))
		return Ebadvalue;

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

char*
msg_parsecolors(IxpMsg *m, CTuple *col) {
	static char Ebad[] = "bad color string";
	Rune r;
	char c, *p;
	int i, j;

	/* '#%6x #%6x #%6x' */
	p = m->pos;
	for(i = 0; i < 3 && p < m->end; i++) {
		if(*p++ != '#')
			return Ebad;
		for(j = 0; j < 6; j++)
			if(p >= m->end || !isxdigit(*p++))
				return Ebad;

		chartorune(&r, p);
		if(i < 2) {
			if(r != ' ')
				return Ebad;
			p++;
		}else if(*p != '\0' && !isspacerune(r))
			return Ebad;
	}

	c = *p;
	*p = '\0';
	loadcolor(col, m->pos);
	*p = c;

	m->pos = p;
	eatrunes(m, isspacerune, true);
	return nil;
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
	case LUP:
	case LDOWN:
	case LCLIENT:
		return msg_selectframe(a->sel, m, sym);
	case LLEFT:
		/* XXX: Multihead. */
		if(a->floating)
			return Ebadvalue;
		for(ap=v->firstarea; ap->next; ap=ap->next)
			if(ap->next == a) break;
		break;
	case LRIGHT:
		/* XXX: Multihead. */
		if(a->floating)
			return Ebadvalue;
		ap = a->next;
		if(ap == nil)
			ap = v->firstarea;
		break;
	case LTILDE:
		ap = v->floating;
		break;
	default:
		if(!strcmp(s, "sel"))
			ap = v->sel;
		else {
			if(!getulong(s, &i) || i == 0)
				return Ebadvalue;
			/* XXX: Multihead. */
			for(ap=v->firstarea; ap; ap=ap->next)
				if(--i == 0) break;
			if(i != 0)
				return Ebadvalue;
		}
		if((s = msg_getword(m))) {
			if(!getulong(s, &i))
				return Ebadvalue;
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
msg_selectframe(Frame *f, IxpMsg *m, int sym) {
	Frame *fp;
	Client *c;
	Area *a;
	char *s;
	bool stack;
	ulong i, dy;

	if(!f)
		return Ebadvalue;
	a = f->area;

	stack = false;
	if(sym == LUP || sym == LDOWN) {
		s = msg_getword(m);
		if(s)
			if(!strcmp(s, "stack"))
				stack = true;
			else
				return Ebadvalue;
	}

	SET(fp);
	switch(sym) {
	case LUP:
		/* XXX: Stack. */
		if(stack) {
			for(; f->aprev && f->aprev->collapsed; f=f->aprev)
				;
			for(fp=a->frame; fp->anext; fp=fp->anext)
				if(fp->anext == f) break;
			for(; fp->aprev && fp->collapsed; fp=fp->aprev)
				;
		}else
			for(fp=a->frame; fp->anext; fp=fp->anext)
				if(fp->anext == f) break;
		break;
	case LDOWN:
		/* XXX: Stack. */
		if(stack) {
			for(fp=f->anext; fp && fp->collapsed; fp=fp->anext)
				;
			if(fp == nil)
				for(fp=a->frame; fp->collapsed; fp=fp->anext)
					;
		}else {
			fp = f->anext;
			if(fp == nil)
				fp = a->frame;
		}
		break;
	case LCLIENT:
		s = msg_getword(m);
		if(s == nil || !getulong(s, &i))
			return "usage: select client <client>";
		c = win2client(i);
		if(c == nil)
			return "unknown client";
		fp = client_viewframe(c, f->view);
		break;
	default:
		die("can't get here");
	}

	if(fp == nil)
		return "invalid selection";
	if(fp == f)
		return nil;
	/* XXX */
	if(fp->collapsed && !f->area->floating && f->area->mode == Coldefault) {
		dy = Dy(f->colr);
		f->colr.max.y = f->colr.min.y + Dy(fp->colr);
		fp->colr.max.y = fp->colr.min.y + dy;
		column_arrange(a, false);
	}
	if(!f->area->floating)
		frame_draw_all();

	frame_focus(fp);
	frame_restack(fp, nil);
	if(fp->view == screen->sel)
		view_restack(fp->view);
	return nil;
}

char*
msg_sendclient(View *v, IxpMsg *m, bool swap) {
	Area *to, *a;
	Frame *f;
	Client *c;
	char *s;
	ulong i;
	int sym;

	s = msg_getword(m);

	c = strclient(v, s);
	if(c == nil)
		return Ebadvalue;

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
		/* XXX: Multihead. */
		if(a->prev)
			to = a->prev;
		a = v->floating;
		break;
	case LRIGHT:
		if(a->floating)
			return Ebadvalue;
		/* XXX: Multihead. */
		to = a->next;
		break;
	case LTOGGLE:
		if(!a->floating)
			to = v->floating;
		else if(f->column)
			to = view_findarea(v, f->column, true);
		else
			to = view_findarea(v, v->selcol, true);
		break;
	case LTILDE:
		if(a->floating)
			return Ebadvalue;
		to = v->floating;
		break;
	default:
		if(!getulong(s, &i) || i == 0)
			return Ebadvalue;
		to = view_findarea(v, i, true);
		break;
	}

	if(!to && !swap && (f->anext || f != f->area->frame))
		to = column_new(v, a, screen->idx, 0);

	if(!to)
		return Ebadvalue;

	if(!swap)
		area_moveto(to, f);
	else if(to->sel)
		frame_swap(f, to->sel);
	else
		return Ebadvalue;

	frame_focus(client_viewframe(c, v));
	/* view_arrange(v); */
	view_update_all();
	return nil;
}

static char*
msg_sendframe(Frame *f, int sym, bool swap) {
	Client *c;
	Frame *fp;

	SET(fp);
	c = f->client;
	switch(sym) {
	case LUP:
		/* XXX: Multihead. */
		fp = f->aprev;
		if(!fp)
			return Ebadvalue;
		if(!swap)
			fp = fp->aprev;
		break;
	case LDOWN:
		/* XXX: Multihead. */
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

