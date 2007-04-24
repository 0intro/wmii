/* Copyright ©2006-2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include "dat.h"
#include "fns.h"

/* Edit ,y/^[a-zA-Z].*\n.* {\n/d
 * Edit s/^([a-zA-Z].*)\n(.*) {\n/\1 \2;\n/
 * Edit ,x/^static.*\n/d
 */

#define strecmp(str, const) (strncmp((str), (const), sizeof(const)-1))
	
static char Ebadcmd[] = "bad command",
	Ebadvalue[] = "bad value";

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

static int
getbase(char **s) {
	if(!strecmp(*s, "0x")) {
		*s += 2;
		return 16;
	}
	if(strecmp(*s, "0")) {
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
	Client *c;
	Frame *f;
	Area *a;
	char *s;
	int i;
	Bool swap;

	s = getword(m);

	if(!strcmp(s, "send")) {
		swap = False;
		goto send;
	}
	if(!strcmp(s, "swap")) {
		swap = True;
		goto send;
	}
	if(!strcmp(s, "select"))
		return select_area(v->sel, m);
	if(!strcmp(s, "colmode")) {
		s = getword(m);

		if((a = strarea(v, s)) == nil || a->floating)
			return Ebadvalue;
		if((i = str2colmode(s)) == -1)
			return Ebadvalue;

		a->mode = i;
		arrange_column(a, True);
		restack_view(v);

		if(v == screen->sel)
			focus_view(screen, v);
		draw_frames();
		return nil;
	}
	return Ebadcmd;

send:
	s = getword(m);

	if(!(c = strclient(v, s)))
		return Ebadvalue;
	if(!(f = view_clientframe(v, c)))
		return Ebadvalue;

	return send_client(f, m, swap);
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
	char *s;
	ulong n;

	s = getword(m);

	if(!strcmp(s, "quit"))
		srv.running = 0;
	else if(!strcmp(s, "exec")) {
		execstr = emalloc(strlen(m->pos) + sizeof("exec "));
		sprintf(execstr, "exec %s", m->pos);
		srv.running = 0;
	}
	else if(!strcmp(s,"view"))
		select_view(m->pos);
	else if(!strcmp(s, "selcolors")) {
		fprintf(stderr, "%s: warning: selcolors have been removed\n", argv0);
		return Ebadcmd;
	}
	else if(!strcmp(s, "focuscolors"))
		return parse_colors(m, &def.focuscolor);
	else if(!strcmp(s, "normcolors"))
		return parse_colors(m, &def.normcolor);
	else if(!strcmp(s, "font")) {
		fn = loadfont(m->pos);
		if(fn) {
			freefont(def.font);
			def.font = fn;
			resize_bar(screen);
		}else
			return "can't load font";
	}
	else if(!strcmp(s, "border")) {
		if(!getulong(getword(m), &n))
			return Ebadvalue;
		def.border = n;
		/* XXX: Apply the change */
	}
	else if(!strcmp(s, "grabmod")) {
		s = getword(m);
		n = mod_key_of_str(s);

		if(!(n & (Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask)))
			return Ebadvalue;

		strncpy(def.grabmod, s, sizeof(def.grabmod));
		def.mod = n;
	}
	else
		return Ebadcmd;
	return nil;
}

char *
read_root_ctl() {
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

	if(!strcmp(s, "kill"))
		kill_client(c);
	else if(!strcmp(s, "Urgent"))
		set_urgent(c, True, True);
	else if(!strcmp(s, "NotUrgent"))
		set_urgent(c, False, True);
	else
		return Ebadcmd;
	return nil;
}

char *
send_client(Frame *f, Message *m, Bool swap) {
	Area *to, *a;
	Client *c;
	Frame *tf;
	View *v;
	char *s;
	ulong i;

	a = f->area;
	v = a->view;
	c = f->client;

	s = getword(m);

	if(!strcmp(s, "toggle")) {
		if(!a->floating)
			to = v->area;
		else if(c->revert && !c->revert->floating)
			to = c->revert;
		else
			to = v->area->next;
		goto send_area;
	}else if(!a->floating) {
		if(!strcmp(s, "left")) {
			if(a->floating)
				return Ebadvalue;
			for(to=v->area->next; to; to=to->next)
				if(a == to->next) break;
			if(!to && !swap && (f->anext || f != a->frame))
				to=new_column(v, v->area, 0);
			goto send_area;
		}
		else if(!strcmp(s, "right")) {
			if(a->floating)
				return Ebadvalue;
			to = a->next;
			if(!to && !swap && (f->anext || f != a->frame))
				to = new_column(v, a, 0);
			goto send_area;
		}
		else if(!strcmp(s, "up")) {
			tf = f->aprev;
			if(!tf)
				return Ebadvalue;
			tf = tf->aprev;
			goto send_frame;
		}
		else if(!strcmp(s, "down")) {
			tf = f->anext;
			if(!tf)
				return Ebadvalue;
			goto send_frame;
		}
		else {
			if(!getulong(s, &i) || i == 0)
				return Ebadvalue;
			for(to=v->area; to; to=to->next)
				if(!i--) break;
			goto send_area;
		}
	}
	return Ebadvalue;

send_frame:
	if(!swap) {
		remove_frame(f);
		insert_frame(tf, f);
	}else {
		if(!tf)
			return Ebadvalue;
		swap_frames(f, tf);
	}
	arrange_column(a, False);

	flushevents(EnterWindowMask, False);
	focus_frame(f, True);
	update_views();
	return nil;

send_area:
	if(!to)
		return Ebadvalue;
	if(!swap)
		send_to_area(to, f);
	else if(to->sel)
		swap_frames(f, to->sel);

	flushevents(EnterWindowMask, False);
	focus_frame(f, True);
	update_views();
	return nil;
}

char *
select_area(Area *a, Message *m) {
	Frame *p, *f;
	Area *ap;
	View *v;
	char *s;
	ulong i;

	v = a->view;
	f = a->sel;
	
	s = getword(m);
	
	
	if(!strcmp(s, "toggle")) {
		if(!a->floating)
			ap = v->area;
		else if(v->revert && v->revert != a)
			ap = v->revert;
		else
			ap = v->area->next;
	}
	else if(!strcmp(s, "left")) {
		if(a->floating)
			return Ebadvalue;
		for(ap=v->area->next; ap->next; ap=ap->next)
			if(ap->next == a) break;
	} 
	else if(!strcmp(s, "right")) {
		if(a->floating)
			return Ebadvalue;
		ap = a->next;
		if(ap == nil)
			ap = v->area->next;
	}
	else if(!strcmp(s, "up")) {
		if(!f)
			return Ebadvalue;
		for(p = f->area->frame; p->anext; p = p->anext)
			if(p->anext == f) break;
		goto focus_frame;
	}
	else if(!strcmp(s, "down")) {
		if(!f)
			return Ebadvalue;
		p = f->anext;
		if(p == nil)
			p = a->frame;
		goto focus_frame;
	}
	else if(!strcmp(s, "~"))
		ap = v->area;
	else {
		if(!getulong(s, &i) || i == 0)
			return Ebadvalue;
		for(ap=v->area->next; ap; ap=ap->next)
			if(!--i) break;
	}
	focus_area(ap);
	return nil;

focus_frame:
	focus_frame(p, False);
	frame_to_top(p);
	if(v == screen->sel)
		restack_view(v);
	return nil;
}
