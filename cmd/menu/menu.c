#include "dat.h"
#include <ctype.h>
#include <string.h>
#include "fns.h"

static Window*	barwin;
static Handlers handlers;

static int	ltwidth;
static int	numlock;

static void	menu_draw(void);

enum {
	ACCEPT = CARET_LAST,
	REJECT,
	HIST_NEXT,
	HIST_PREV,
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

	ltwidth = textwidth(font, "<");

	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | KeyPressMask;
	barwin = createwindow(&scr.root, Rect(0, 0, 1, 1), scr.depth, InputOutput,
			&wa, CWOverrideRedirect
			   | CWBackPixmap
			   | CWEventMask);
	sethandler(barwin, &handlers);
}

static void
menu_unmap(long id, void *p) {

	USED(id, p);
	unmapwin(barwin);
	XFlush(display);
}

static char*
histtext(Item *i) {
	static char *orig;

	if(!histidx->string) {
		free(orig);
		orig = strdup(input.string);
	}
	return i->string ? i->string : orig;
}

static void
menu_cmd(int op, int motion) {

	switch(op) {
	case HIST_NEXT:
		if(histidx->next) {
			caret_insert(histtext(histidx->next), true);
			histidx = histidx->next;
		}
		break;
	case HIST_PREV:
		if(histidx->prev) {
			caret_insert(histtext(histidx->prev), true);
			histidx = histidx->prev;
		}
		break;
	case KILL:
		caret_delete(BACKWARD, motion);
		break;
	default:
		goto next;
	}
	update_filter();
next:
	switch(op) {
	case ACCEPT:
		srv.running = false;
		if(matchidx->retstring && !motion)
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
		break;
	case CMPL_NEXT:
		matchidx = matchidx->next;
		break;
	case CMPL_PREV:
		matchidx = matchidx->prev;
		break;
	case CMPL_FIRST:
		matchstart = matchfirst;
		matchidx = nil;
		matchend = nil;
		break;
	case CMPL_LAST:
		matchidx = matchfirst->prev;
		break;
	case CMPL_NEXT_PAGE:
		matchidx = matchend->next;
		break;
	case CMPL_PREV_PAGE:
		matchend = matchstart->prev;
		matchidx = nil;
		break;
	}
	menu_draw();
}

static void
menu_draw(void) {
	Rectangle r, r2;
	CTuple *c;
	Item *i;
	int inputw, itemoff, end, pad, n;

	r = barwin->r;
	r = rectsetorigin(r, ZP);
	r2 = r;

	pad = (font->height & ~1);
	inputw = min(Dx(r) / 3, maxwidth) + pad;
	itemoff = inputw + 2 * ltwidth;
	end = Dx(r) - ltwidth;

	fill(ibuf, r, cnorm.bg);

	if(matchend && matchidx == matchend->next)
		matchstart = matchidx;
	else if(matchidx == matchstart->prev)
		matchend = matchidx;

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

	if(matchidx == nil)
		matchidx = matchstart;

	for(i=matchstart; i->string; i=i->next) {
		r2.min.x = itemoff;
		itemoff  = itemoff + i->width + pad;
		r2.max.x = min(itemoff, end);
		if(i != matchstart && itemoff > end)
			break;

		c = (i == matchidx) ? &csel : &cnorm;
		fill(ibuf, r2, c->bg);
		drawstring(ibuf, font, r2, Center, i->string, c->fg);
		matchend = i;
		if(i->next == matchfirst)
			break;
	}

	r2 = r;
	r2.min.x = inputw;
	if(matchstart != matchfirst)
		drawstring(ibuf, font, r2, West, "<", cnorm.fg);
	if(matchend->next != matchfirst)
		drawstring(ibuf, font, r2, East, ">", cnorm.fg);
	r2 = r;
	r2.max.x = inputw;
	drawstring(ibuf, font, r2, West, input.string, cnorm.fg);

	r2.min.x = textwidth_l(font, input.string, input.pos - input.string) + pad/2;
	r2.max.x = r2.min.x + 2;
	r2.min.y++;
	r2.max.y--;
	border(ibuf, r2, 1, cnorm.border);

	border(ibuf, r, 1, cnorm.border);
	copyimage(barwin, r, ibuf, ZP);
}

void
menu_show(void) {
	Rectangle r;
	int height, pad;

	USED(menu_unmap);

	pad = (font->height & ~1)/2;
	height = font->height + 2;

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
	grabkeyboard(barwin);
}

static void
kdown_event(Window *w, XKeyEvent *e) {
	char buf[32];
	int num;
	KeySym ksym;

	buf[0] = 0;
	num = XLookupString(e, buf, sizeof buf, &ksym, 0);
	if(IsKeypadKey(ksym))
		if(ksym == XK_KP_Enter)
			ksym = XK_Return;
		else if(ksym >= XK_KP_0 && ksym <= XK_KP_9)
			ksym = (ksym - XK_KP_0) + XK_0;

	if(IsFunctionKey(ksym)
	|| IsKeypadKey(ksym)
	|| IsMiscFunctionKey(ksym)
	|| IsPFKey(ksym)
	|| IsPrivateKeypadKey(ksym))
		return;

	/* first check if a control mask is omitted */
	if(e->state & ControlMask) {
		switch (ksym) {
		default:
			return;
		case XK_bracketleft: /* Esc */
			menu_cmd(REJECT, 0);
			return;
		case XK_j:
		case XK_J:
		case XK_m:
		case XK_M:
			menu_cmd(ACCEPT, e->state&ShiftMask);
			return;
		case XK_n:
		case XK_N:
			menu_cmd(HIST_NEXT, 0);
			return;
		case XK_p:
		case XK_P:
			menu_cmd(HIST_PREV, 0);
			return;
		case XK_i: /* Tab */
		case XK_I:
			if(e->state & ShiftMask)
				menu_cmd(CMPL_PREV, 0);
			else
				menu_cmd(CMPL_NEXT, 0);
			return;
		case XK_h:
		case XK_H:
			menu_cmd(KILL, CHAR);
			return;
		case XK_BackSpace:
		case XK_w:
		case XK_W:
			menu_cmd(KILL, WORD);
			return;
		case XK_u:
		case XK_U:
			menu_cmd(KILL, LINE);
			return;
		}
	}
	/* Alt-<Key> - Vim */
	if((e->state & ~(numlock | LockMask)) & Mod1Mask) {
		switch(ksym) {
		default:
			return;
		case XK_h:
			menu_cmd(CMPL_PREV, 0);
			return;
		case XK_l:
			menu_cmd(CMPL_NEXT, 0);
			return;
		case XK_j:
			menu_cmd(CMPL_NEXT_PAGE, 0);
			return;
		case XK_k:
			menu_cmd(CMPL_PREV_PAGE, 0);
			return;
		case XK_g:
			menu_cmd(CMPL_FIRST, 0);
			return;
		case XK_G:
			menu_cmd(CMPL_LAST, 0);
			return;
		}
	}
	switch(ksym) {
	default:
		if(num && !iscntrl(buf[0])) {
			caret_insert(buf, false);
			update_filter();
			menu_draw();
		}
		break;
	case XK_Escape:
		menu_cmd(REJECT, 0);
		return;
	case XK_Return:
		menu_cmd(ACCEPT, e->state&ShiftMask);
		return;
	case XK_BackSpace:
		menu_cmd(KILL, CHAR);
		return;
	case XK_Up:
		menu_cmd(HIST_PREV, 0);
		return;
	case XK_Down:
		menu_cmd(HIST_NEXT, 0);
		return;
	case XK_Home:
		/* TODO: Caret. */
		menu_cmd(CMPL_FIRST, 0);
		return;
	case XK_End:
		/* TODO: Caret. */
		menu_cmd(CMPL_LAST, 0);
		return;
	case XK_Left:
		menu_cmd(BACKWARD, CHAR);
		return;
	case XK_Right:
		menu_cmd(FORWARD, CHAR);
		return;
	case XK_Next:
		menu_cmd(CMPL_NEXT_PAGE, 0);
		return;
	case XK_Prior:
		menu_cmd(CMPL_PREV_PAGE, 0);
		return;
	case XK_Tab:
		menu_cmd(CMPL_NEXT, 0);
		return;
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

