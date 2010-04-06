#include "dat.h"
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#include "fns.h"

static Handlers handlers;

static int	ltwidth;

static void	_menu_draw(bool);

enum {
	ACCEPT = CARET_LAST,
	REJECT,
	HIST,
	KILL,
	CMPL_NEXT,
	CMPL_PREV,
	CMPL_FIRST,
	CMPL_LAST,
	CMPL_NEXT_PAGE,
	CMPL_PREV_PAGE,
};

void
menu_init(void) {
	WinAttr wa;

	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | KeyPressMask;
	barwin = createwindow(&scr.root, Rect(-1, -1, 1, 1), scr.depth, InputOutput,
			&wa, CWOverrideRedirect
			   | CWBackPixmap
			   | CWEventMask);
	sethandler(barwin, &handlers);
	mapwin(barwin);

	int i = 0;
	while(!grabkeyboard(barwin)) {
		if(i++ > 1000)
			fatal("can't grab keyboard");
		usleep(1000);
	}
}

static void
menu_unmap(long id, void *p) {

	USED(id, p);
	unmapwin(barwin);
	XFlush(display);
}

static void
selectitem(Item *i) {
	if(i != matchidx) {
		caret_set(input.filter_start, input.pos - input.string);
		caret_insert(i->string, 0);
		matchidx = i;
	}
}

static void
menu_cmd(int op, int motion) {
	int n;

	switch(op) {
	case HIST:
		n = input.pos - input.string;
		caret_insert(history_search(motion, input.string, n), true);
		input.pos = input.string + n;
		break;
	case KILL:
		caret_delete(BACKWARD, motion);
		break;
	default:
		goto next;
	}
	update_filter(true);
next:
	switch(op) {
	case ACCEPT:
		srv.running = false;
		if(!matchidx && matchfirst->retstring && !motion)
		if(input.filter_start == 0 && input.pos == input.end)
			menu_cmd(CMPL_FIRST, 0);
		if(!motion && matchidx && !strcmp(input.string, matchidx->string))
			print("%s", matchidx->retstring);
		else
			print("%s", input.string);
		break;
	case REJECT:
		srv.running = false;
		result = 1;
		break;
	case BACKWARD:
	case FORWARD:
		caret_move(op, motion);
		update_input();
		break;
	case CMPL_NEXT:
		selectitem(matchidx ? matchidx->next : matchfirst);
		break;
	case CMPL_PREV:
		selectitem((matchidx ? matchidx : matchstart)->prev);
		break;
	case CMPL_FIRST:
		matchstart = matchfirst;
		matchend = nil;
		selectitem(matchstart);
		break;
	case CMPL_LAST:
		selectitem(matchfirst->prev);
		break;
	case CMPL_NEXT_PAGE:
		if(matchend)
			selectitem(matchend->next);
		break;
	case CMPL_PREV_PAGE:
		matchend = matchstart->prev;
		matchidx = nil;
		_menu_draw(false);
		selectitem(matchstart);
		break;
	}
	menu_draw();
}

static void
_menu_draw(bool draw) {
	Rectangle r, rd, rp, r2, extent;
	CTuple *c;
	Item *i;
	int inputw, itemoff, end, pad, n, offset;

	r = barwin->r;
	r = rectsetorigin(r, ZP);

	pad = (font->height & ~1) + font->pad.min.x + font->pad.max.x;

	rd = r;
	rp = ZR; // SET(rp)
	if (prompt) {
		if (!promptw)
			promptw = textwidth(font, prompt) + 2 * ltwidth + pad;
		rd.min.x += promptw;

		rp = r;
		rp.max.x = promptw;
	}

	inputw = min(Dx(rd) / 3, maxwidth);
	inputw = max(inputw, textwidth(font, input.string)) + pad;
	itemoff = inputw + 2 * ltwidth;
	end = Dx(rd) - ltwidth;

	fill(ibuf, r, cnorm.bg);

	if(matchend && matchidx == matchend->next)
		matchstart = matchidx;
	else if(matchidx == matchstart->prev)
		matchend = matchidx;
	if (matchend == nil)
		matchend = matchstart;

	if(matchend == matchstart->prev && matchstart != matchidx) {
		n = itemoff;
		matchstart = matchend;
		for(i=matchend; ; i=i->prev) {
			n += i->width + pad;
			if(n > end)
				break;
			matchstart = i;
			if(i == matchfirst)
				break;
		}
	}

	if(!draw)
		return;

	r2 = rd;
	for(i=matchstart; i->string; i=i->next) {
		r2.min.x = promptw + itemoff;
		itemoff  = itemoff + i->width + pad;
		r2.max.x = promptw + min(itemoff, end);
		if(i != matchstart && itemoff > end)
			break;

		c = (i == matchidx) ? &csel : &cnorm;
		fill(ibuf, r2, c->bg);
		drawstring(ibuf, font, r2, Center, i->string, c->fg);
		matchend = i;
		if(i->next == matchfirst)
			break;
	}

	r2 = rd;
	r2.min.x = promptw + inputw;
	if(matchstart != matchfirst)
		drawstring(ibuf, font, r2, West, "<", cnorm.fg);
	if(matchend->next != matchfirst)
		drawstring(ibuf, font, r2, East, ">", cnorm.fg);

	r2 = rd;
	r2.max.x = promptw + inputw;
	drawstring(ibuf, font, r2, West, input.string, cnorm.fg);

	extent = textextents_l(font, input.string, input.pos - input.string, &offset);
	r2.min.x = promptw + offset + font->pad.min.x - extent.min.x + pad/2 - 1;
	r2.max.x = r2.min.x + 2;
	r2.min.y++;
	r2.max.y--;
	border(ibuf, r2, 1, cnorm.border);

	if (prompt)
		drawstring(ibuf, font, rp, West, prompt, cnorm.fg);

	border(ibuf, rd, 1, cnorm.border);
	copyimage(barwin, r, ibuf, ZP);
}

void
menu_draw(void) {
	_menu_draw(true);
}

void
menu_show(void) {
	Rectangle r;
	int height, pad;

	USED(menu_unmap);

	ltwidth = textwidth(font, "<");

	pad = (font->height & ~1)/2;
	height = labelh(font);

	r = scr.rect;
	if(ontop)
		r.max.y = r.min.y + height;
	else
		r.min.y = r.max.y - height;
	reshapewin(barwin, r);

	freeimage(ibuf);
	ibuf = allocimage(Dx(r), Dy(r), scr.depth);

	mapwin(barwin);
	raisewin(barwin);
	menu_draw();
}

static void
kdown_event(Window *w, XKeyEvent *e) {
	char **action, **p;
	char *key;
	char buf[32];
	int num;
	KeySym ksym;

	buf[0] = 0;
	num = XLookupString(e, buf, sizeof buf, &ksym, 0);
	key = XKeysymToString(ksym);
	if(IsKeypadKey(ksym))
		if(ksym == XK_KP_Enter)
			ksym = XK_Return;
		else if(ksym >= XK_KP_0 && ksym <= XK_KP_9)
			ksym = (ksym - XK_KP_0) + XK_0;

	if(IsFunctionKey(ksym)
	|| IsMiscFunctionKey(ksym)
	|| IsKeypadKey(ksym)
	|| IsPrivateKeypadKey(ksym)
	|| IsPFKey(ksym))
		return;

	action = find_key(key, e->state);
	if(action == nil || action[0] == nil) {
		if(num && !iscntrl(buf[0])) {
			caret_insert(buf, false);
			update_filter(true);
			menu_draw();
		}
	}
	else {
		long mask = 0;
#		define have(val) !!(mask & (1 << val))
		for(p=action+1; *p; p++)
			mask |= 1 << getsym(*p);
		int amount = (
			have(LCHAR) ? CHAR :
			have(LWORD) ? WORD :
			have(LLINE) ? LINE :
			-1);
		switch(getsym(action[0])) {
		case LACCEPT:
			menu_cmd(ACCEPT, have(LLITERAL));
			break;
		case LBACKWARD:
			menu_cmd(BACKWARD, amount);
			break;
		case LCOMPLETE:
			amount = (
				have(LNEXT)     ? CMPL_NEXT  :
				have(LPREV)     ? CMPL_PREV  :
				have(LNEXTPAGE) ? CMPL_NEXT_PAGE :
				have(LPREVPAGE) ? CMPL_PREV_PAGE :
				have(LFIRST)    ? CMPL_FIRST :
				have(LLAST)     ? CMPL_LAST  :
				CMPL_NEXT);
			menu_cmd(amount, 0);
			break;
		case LFORWARD:
			menu_cmd(FORWARD, amount);
			break;
		case LHISTORY:
			menu_cmd(HIST, have(LBACKWARD) ? BACKWARD : FORWARD);
			break;
		case LKILL:
			menu_cmd(KILL, amount);
			break;
		case LREJECT:
			menu_cmd(REJECT, 0);
			break;
		}
	}
}

static void
expose_event(Window *w, XExposeEvent *e) {

	USED(w);
	menu_draw();
}

static Handlers handlers = {
	.expose = expose_event,
	.kdown = kdown_event,
};

