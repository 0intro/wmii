/* Copyright ©2007-2008 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include "dat.h"
#include <assert.h>
#include <sys/limits.h>
#include "fns.h"

Window *ewmhwin;

#define Net(x) ("_NET_" x)
#define	Action(x) ("_NET_WM_ACTION_" x)
#define	State(x) ("_NET_WM_STATE_" x)
#define	Type(x) ("_NET_WM_WINDOW_TYPE_" x)
#define NET(x) xatom(Net(x))
#define	ACTION(x) xatom(Action(x))
#define	STATE(x) xatom(State(x))
#define	TYPE(x) xatom(Type(x))

void
ewmh_init(void) {
	WinAttr wa;
	char myname[] = "wmii";
	long win[1];

	ewmhwin = createwindow(&scr.root,
		Rect(0, 0, 1, 1), 0 /*depth*/,
		InputOnly, &wa, 0);

	win[0] = ewmhwin->w;
	changeprop_long(&scr.root, Net("SUPPORTING_WM_CHECK"), "WINDOW", win, 1);
	changeprop_long(ewmhwin, Net("SUPPORTING_WM_CHECK"), "WINDOW", win, 1);
	changeprop_string(ewmhwin, Net("WM_NAME"), myname);
	changeprop_long(&scr.root, Net("DESKTOP_VIEWPORT"), "CARDINAL",
		(long[]){0, 0}, 2);

	long supported[] = {
		/* Misc */
		NET("SUPPORTED"),
		/* Root Properties */
		NET("ACTIVE_WINDOW"),
		NET("CURRENT_DESKTOP"),
		/* Client Properties */
		NET("WM_NAME"),
		NET("WM_STRUT_PARTIAL"),
		NET("WM_DESKTOP"),
		NET("FRAME_EXTENTS"),
		/* States */
		NET("WM_STATE"),
		STATE("DEMANDS_ATTENTION"),
		STATE("FULLSCREEN"),
		STATE("SHADED"),
		/* Window Types */
		NET("WM_WINDOW_TYPE"),
		TYPE("DIALOG"),
		TYPE("DOCK"),
		TYPE("NORMAL"),
		TYPE("SPLASH"),
		/* Actions */
		NET("WM_ALLOWED_ACTIONS"),
		ACTION("FULLSCREEN"),
		/* Desktops */
		NET("DESKTOP_NAMES"),
		NET("NUMBER_OF_DESKTOPS"),
		/* Client List */
		NET("CLIENT_LIST"),
		NET("CLIENT_LIST_STACKING"),
	};
	changeprop_long(&scr.root, Net("SUPPORTED"), "ATOM", supported, nelem(supported));
}

void
ewmh_updateclientlist(void) {
	Client *c;
	long *list;
	int i;

	i = 0;
	for(c=client; c; c=c->next)
		i++;
	list = emalloc(i * sizeof *list);
	for(c=client, i=0; c; c=c->next)
		list[i++] = c->w.w;
	changeprop_long(&scr.root, Net("CLIENT_LIST"), "WINDOW", list, i);
}

void
ewmh_updatestacking(void) {
	Vector_long vec;
	Frame *f;
	Area *a;
	View *v;

	vector_linit(&vec);

	for(v=view; v; v=v->next)
		for(f=v->area->stack; f; f=f->snext)
			if(f->client->sel == f)
				vector_lpush(&vec, f->client->w.w);
	for(v=view; v; v=v->next)
		for(a=v->area->next; a; a=a->next)
			for(f=a->frame; f; f=f->anext)
				if(f->client->sel == f)
					vector_lpush(&vec, f->client->w.w);

	changeprop_long(&scr.root, Net("CLIENT_LIST_STACKING"), "WINDOW", vec.ary, vec.n);
	vector_lfree(&vec);
}

void
ewmh_initclient(Client *c) {
	long allowed[] = {
		ACTION("FULLSCREEN"),
	};

	changeprop_long(&c->w, Net("WM_ALLOWED_ACTIONS"), "ATOM",
		allowed, nelem(allowed));
	ewmh_getwintype(c);
	ewmh_updateclientlist();
}

void
ewmh_destroyclient(Client *c) {
	Ewmh *e;

	ewmh_updateclientlist();

	e = &c->w.ewmh;
	if(e->timer)
		if(!ixp_unsettimer(&srv, e->timer))
			fprint(2, "Badness: %C: Can't unset timer\n", c);
}

static void
pingtimeout(long id, void *v) {
	Client *c;

	c = v;
	event("Unresponsive %C\n", c);
	c->w.ewmh.ping = 0;
	c->w.ewmh.timer = 0;
}

void
ewmh_pingclient(Client *c) {
	Ewmh *e;

	if(!(c->proto & ProtoPing))
		return;

	e = &c->w.ewmh;
	if(e->ping)
		return;

	sendmessage(&c->w, "WM_PROTOCOLS", Net("WM_PING"), c->w.w, 0, 0);
	e->ping = xtime++;
	e->timer = ixp_settimer(&srv, PingTime, pingtimeout, c);
}

int
ewmh_prop(Client *c, Atom a) {
	if(a == NET("WM_WINDOW_TYPE"))
		ewmh_getwintype(c);
	else
	if(a == NET("WM_STRUT_PARTIAL"))
		ewmh_getstrut(c);
	else
		return 0;
	return 1;
}

typedef struct Prop Prop;
struct Prop {
	char*	name;
	long	mask;
	Atom	atom;
};

static long
getmask(Prop *props, long *vals, int n) {
	Prop *p;
	long ret;

	if(props[0].atom == 0)
		for(p=props; p->name; p++)
			p->atom = xatom(p->name);

	ret = 0;
	while(n--) {
		Dprint(DEwmh, "\tvals[%d] = \"%A\"\n", n, vals[n]);
		for(p=props; p->name; p++)
			if(p->atom == vals[n]) {
				ret |= p->mask;
				break;
			}
	}
	return ret;
}

void
ewmh_getwintype(Client *c) {
	static Prop props[] = {
		{Type("DESKTOP"), TypeDesktop},
		{Type("DOCK"), TypeDock},
		{Type("TOOLBAR"), TypeToolbar},
		{Type("MENU"), TypeMenu},
		{Type("UTILITY"), TypeUtility},
		{Type("SPLASH"), TypeSplash},
		{Type("DIALOG"), TypeDialog},
		{Type("NORMAL"), TypeNormal},
		{0, }
	};
	long *types;
	long n, mask;

	n = getprop_long(&c->w, Net("WM_WINDOW_TYPE"), "ATOM",
		0L, &types, 16);
	Dprint(DEwmh, "ewmh_getwintype(%C) n = %ld\n", c, n);
	mask = getmask(props, types, n);
	free(types);

	c->w.ewmh.type = mask;
	if(mask & TypeDock) {
		c->borderless = 1;
		c->titleless = 1;
	}
}

long
ewmh_protocols(Window *w) {
	static Prop props[] = {
		{"WM_DELETE_WINDOW", ProtoDelete},
		{"WM_TAKE_FOCUS", ProtoTakeFocus},
		{Net("WM_PING"), ProtoPing},
		{0, }
	};
	long *protos;
	long n, mask;

	n = getprop_long(w, "WM_PROTOCOLS", "ATOM",
		0L, &protos, 16);
	Dprint(DEwmh, "ewmh_protocols(%W) n = %ld\n", w, n);
	mask = getmask(props, protos, n);
	free(protos);
	return mask;
}

void
ewmh_getstrut(Client *c) {
	enum {
		Left, Right, Top, Bottom,
		LeftMin, LeftMax,
		RightMin, RightMax,
		TopMin, TopMax,
		BottomMin, BottomMax,
		Last = BottomMax
	};
	long *strut;
	ulong n;

	if(c->strut)
		free(c->strut);
	c->strut = nil;

	n = getprop_long(&c->w, Net("WM_STRUT_PARTIAL"), "CARDINAL",
		0L, &strut, Last);
	if(n != nelem(strut)) {
		free(strut);
		n = getprop_long(&c->w, Net("WM_STRUT"), "CARDINAL",
			0L, &strut, 4L);
		if(n != 4) {
			free(strut);
			return;
		}
		strut = erealloc(strut, Last * sizeof *strut);
		strut[LeftMin] = strut[RightMin] = 0;
		strut[LeftMax] = strut[RightMax] = INT_MAX;
		strut[TopMin] = strut[BottomMin] = 0;
		strut[TopMax] = strut[BottomMax] = INT_MAX;
	}
	c->strut = emalloc(sizeof *c->strut);
	c->strut->left =   Rect(0,                strut[LeftMin],  strut[Left],      strut[LeftMax]);
	c->strut->right =  Rect(-strut[Right],    strut[RightMin], 0,                strut[RightMax]);
	c->strut->top =    Rect(strut[TopMin],    0,               strut[TopMax],    strut[Top]);
	c->strut->bottom = Rect(strut[BottomMin], -strut[Bottom],  strut[BottomMax], 0);
	free(strut);
}

int
ewmh_clientmessage(XClientMessageEvent *e) {
	Client *c;
	View *v;
	long *l;
	int msg, action, i;

	l = e->data.l;
	msg = e->message_type;
	Dprint(DEwmh, "ClientMessage: %A\n", msg);

	if(msg == NET("WM_STATE")) {
		enum {
			StateUnset,
			StateSet,
			StateToggle,
		};
		if(e->format != 32)
			return -1;
		c = win2client(e->window);
		if(c == nil)
			return 0;
		switch(l[0]) {
		case StateUnset:  action = Off;    break;
		case StateSet:    action = On;     break;
		case StateToggle: action = Toggle; break;
		default:
			return -1;
		}
		Dprint(DEwmh, "\tAction: %s\n", TOGGLE(action));
		for(i = 1; i <= 2; i++) {
			if(l[i] == 0)
				break;
			Dprint(DEwmh, "\tl[%d] = %A\n", i, l[i]);
			if(l[i] == STATE("FULLSCREEN"))
				fullscreen(c, action);
			else
			if(l[i] == STATE("DEMANDS_ATTENTION"))
				client_seturgent(c, action, UrgClient);
		}
		return 1;
	}else
	if(msg == NET("ACTIVE_WINDOW")) {
		if(e->format != 32)
			return -1;
		Dprint(DEwmh, "\tsource: %ld\n", l[0]);
		Dprint(DEwmh, "\twindow: 0x%lx\n", e->window);
		c = win2client(e->window);
		if(c == nil)
			return 1;
		Dprint(DEwmh, "\tclient: %s\n", clientname(c));
		if(l[0] != 2)
			return 1;
		focus(c, true);
		return 1;
	}else
	if(msg == NET("CURRENT_DESKTOP")) {
		if(e->format != 32)
			return -1;
		for(v=view, i=l[0]; v; v=v->next, i--)
			if(i == 0)
				break;
		Dprint(DEwmh, "\t%s\n", v->name);
		if(i == 0)
			view_select(v->name);
		return 1;
	}else
	if(msg == xatom("WM_PROTOCOLS")) {
		Dprint(DEwmh, "\t%A\n", l[0]);
		if(l[0] == NET("WM_PING")) {
			if(e->format != 32)
				return -1;
			if(e->window != scr.root.w)
				return -1;
			c = win2client(l[2]);
			if(c == nil)
				return 1;
			Dprint(DEwmh, "\tclient = [%C]\"%s\"\n", c, clientname(c));
			Dprint(DEwmh, "\ttimer = %ld, ping = %ld\n",
					c->w.ewmh.timer, c->w.ewmh.ping);
			if(c->w.ewmh.timer)
				ixp_unsettimer(&srv, c->w.ewmh.timer);
			c->w.ewmh.timer = 0;
			c->w.ewmh.ping = 0;
			return 1;
		}
	}

	return 0;
}

void
ewmh_framesize(Client *c) {
	Rectangle r;
	Frame *f;

	f = c->sel;
	r.min.x = f->crect.min.x;
	r.min.y = f->crect.min.y;
	r.max.x = f->r.max.x - f->crect.max.x;
	r.max.y = f->r.max.y - f->crect.max.y;

	long extents[] = {
		r.min.x, r.max.x,
		r.min.y, r.max.y,
	};
	changeprop_long(&c->w, Net("FRAME_EXTENTS"), "CARDINAL",
			extents, nelem(extents));
}

void
ewmh_updatestate(Client *c) {
	long state[16];
	Frame *f;
	int i;

	f = c->sel;
	if(f == nil || f->view != screen->sel)
		return;

	i = 0;
	if(f->collapsed)
		state[i++] = STATE("SHADED");
	if(c->fullscreen)
		state[i++] = STATE("FULLSCREEN");
	if(c->urgent)
		state[i++] = STATE("DEMANDS_ATTENTION");

	if(i > 0)
		changeprop_long(&c->w, Net("WM_STATE"), "ATOM", state, i);
	else
		delproperty(&c->w, Net("WM_STATE"));
}

/* Views */
void
ewmh_updateviews(void) {
	View *v;
	char **tags;
	long i;

	if(starting)
		return;

	for(v=view, i=0; v; v=v->next)
		i++;
	tags = emalloc((i + 1) * sizeof *tags);
	for(v=view, i=0; v; v=v->next)
		tags[i++] = v->name;
	tags[i] = nil;
	changeprop_textlist(&scr.root, Net("DESKTOP_NAMES"), "UTF8_STRING", tags);
	changeprop_long(&scr.root, Net("NUMBER_OF_DESKTOPS"), "CARDINAL", &i, 1);
	ewmh_updateview();
	ewmh_updateclients();
}

static int
viewidx(View *v) {
	View *vp;
	int i;

	for(vp=view, i=0; vp; vp=vp->next, i++)
		if(vp == v)
			break;
	assert(vp);
	return i;
}

void
ewmh_updateview(void) {
	long i;

	if(starting)
		return;

	i = viewidx(screen->sel);
	changeprop_long(&scr.root, Net("CURRENT_DESKTOP"), "CARDINAL", &i, 1);
}

void
ewmh_updateclient(Client *c) {
	long i;

	i = -1;
	if(c->sel)
		i = viewidx(c->sel->view);
	changeprop_long(&c->w, Net("WM_DESKTOP"), "CARDINAL", &i, 1);
}

void
ewmh_updateclients(void) {
	Client *c;

	if(starting)
		return;

	for(c=client; c; c=c->next)
		ewmh_updateclient(c);
}

