#include "dat.h"
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#include "fns.h"

static Handlers	handlers;
static int	promptw;

void
menu_init(void) {
	WinAttr wa;

	wa.event_mask = ExposureMask | KeyPressMask;
	menu.win = createwindow(&scr.root, Rect(-1, -1, 1, 1), scr.depth, InputOutput,
				&wa, CWEventMask);
	if(scr.xim)
		menu.win->xic = XCreateIC(scr.xim,
					  XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
					  XNClientWindow, menu.win->xid,
					  XNFocusWindow, menu.win->xid,
					  nil);

	changeprop_long(menu.win, Net("WM_WINDOW_TYPE"), "ATOM", (long[]){ TYPE("MENU") }, 1);
	changeprop_string(menu.win, "_WMII_TAGS", "sel");
	changeprop_textlist(menu.win, "WM_CLASS", "STRING", (char*[3]){ "wimenu", "wimenu" });

	sethandler(menu.win, &handlers);
	mapwin(menu.win);

	int i = 0;
	while(!grabkeyboard(menu.win)) {
		if(i++ > 1000)
			fatal("can't grab keyboard");
		usleep(1000);
	}
}

void
menu_show(void) {
	Rectangle r;

	if(menu.prompt)
		promptw = textwidth(font, menu.prompt) + itempad;

	r = textextents_l(font, "<", 1, nil);
	menu.arrow = Pt(Dy(r) + itempad/2, Dx(r) + itempad/2);

	menu.height = labelh(font);

	freeimage(menu.buf);
	menu.buf = allocimage(Dx(scr.rect),
			      !!menu.rows * 2 * menu.arrow.y + (menu.rows + 1) * menu.height,
			      menu.win->depth);

	mapwin(menu.win);
	raisewin(menu.win);
	menu_draw();
}

/* I'd prefer to use ⌃ and ⌄, but few fonts support them. */
static void
drawarrow(Image *img, Rectangle r, int up, Color *col) {
	Point p[3], pt;

	pt = Pt(menu.arrow.x - itempad/2, menu.arrow.y - itempad/2 & ~1);

	p[1] = Pt(r.min.x + Dx(r)/2,	up ? r.min.y + itempad/4 : r.max.y - itempad/4);
	p[0] = Pt(p[1].x - pt.x/2,	up ? p[1].y + pt.y	 : p[1].y - pt.y);
	p[2] = Pt(p[1].x + pt.x/2,	p[0].y);
	drawpoly(img, p, nelem(p), CapProjecting, 1, col);
}

static Rectangle
slice(Rectangle *rp, int x, int y) {
	Rectangle r;

	r = *rp;
	if(x) rp->min.x += x, r.max.x = min(rp->min.x, rp->max.x);
	if(y) rp->min.y += y, r.max.y = min(rp->min.y, rp->max.y);
	return r;
}

static bool
nextrect(Item *i, Rectangle *rp, Rectangle *src) {
	Rectangle r;

	if(menu.rows)
		r = slice(src, 0, menu.height);
	else
		r = slice(src, i->width, 0);
	return (Dx(*src) >= 0 && Dy(*src) >= 0) && (*rp = r, 1);
}

void
menu_draw(void) {
	Rectangle barr, extent, itemr, inputr, r, r2;
	Item *item;
	int inputw, offset;

	barr = r2 = Rect(0, 0, Dx(menu.win->r), menu.height);

	inputw = max(match.maxwidth + textwidth_l(font, input.string, min(input.filter_start, strlen(input.string))),
		     max(itempad    + textwidth(font, input.string),
			 Dx(barr) / 3));

	/* Calculate items box, w/ and w/o arrows */
	if(menu.rows) {
		menu.itemr = barr;
		menu.itemr.max.y += Dy(barr) * (menu.rows - 1);
		if(menu.ontop)
			menu.itemr = rectaddpt(menu.itemr, Pt(0, Dy(barr)));
		itemr = menu.itemr;
		if(match.start != match.first)
			menu.itemr = rectaddpt(menu.itemr, Pt(0, menu.arrow.y));
	}
	else {
		itemr = r2;
		slice(&itemr, inputw + promptw, 0);
		menu.itemr = Rect(itemr.min.x + menu.arrow.x, itemr.min.y,
				  itemr.max.x - menu.arrow.x, itemr.max.y);
	}

	fill(menu.buf, menu.buf->r, &cnorm.bg);

	/* Draw items */
	item = match.start, r2 = menu.itemr;
	nextrect(item, &r, &r2);
	do {
		match.end = item;
		if(item->string)
			fillstring(menu.buf, font, r, West, item->string,
				   (item == match.sel ? &csel : &cnorm), 0);
		item = item->next;
	} while(item != match.first && nextrect(item, &r, &r2));

	/* Adjust dimensions for arrows/number of items */
	if(menu.rows)
		itemr.max.y = r.max.y + (match.end->next != match.first ? menu.arrow.y : 0);
	else
		itemr.max.x = r.max.x + menu.arrow.x;
	if(menu.rows && !menu.ontop)
		barr = rectaddpt(barr, Pt(0, itemr.max.y));

	/* Draw indicators */
	if(!menu.rows && match.start != match.first)
		drawstring(menu.buf, font, itemr, West, "<", &cnorm.fg);
	if(!menu.rows && match.end->next != match.first)
		drawstring(menu.buf, font, itemr, East, ">", &cnorm.fg);

	if(menu.rows && match.start != match.first)
		drawarrow(menu.buf, itemr, 1, &cnorm.fg);
	if(menu.rows && match.end->next != match.first)
		drawarrow(menu.buf, itemr, 0, &cnorm.fg);

	/* Draw prompt */
	r2 = barr;
	if(menu.prompt)
		drawstring(menu.buf, font, slice(&r2, promptw, 0),
			   West, menu.prompt, &cnorm.fg);

	/* Border input/horizontal items */
	border(menu.buf, r2, 1, &cnorm.border);

	/* Draw input */
	inputr = slice(&r2, inputw, 0);
	drawstring(menu.buf, font, inputr, West, input.string, &cnorm.fg);

	/* Draw cursor */
	extent = textextents_l(font, input.string, input.pos - input.string, &offset);
	r2 = insetrect(inputr, 2);
	r2.min.x = inputr.min.x - extent.min.x + offset + font->pad.min.x + itempad/2 - 1;
	r2.max.x = r2.min.x + 1;
	fill(menu.buf, r2, &cnorm.border);

	/* Reshape window */
	r = scr.rect;
	if(menu.ontop)
		r.max.y = r.min.y + itemr.max.y;
	else
		r.min.y = r.max.y - barr.max.y;
	reshapewin(menu.win, r);

	/* Border window */
	r = rectsubpt(r, r.min);
	border(menu.buf, r, 1, &cnorm.border);
	copyimage(menu.win, r, menu.buf, ZP);
}

static Item*
pagestart(Item *i) {
	Rectangle r, r2;

	r = menu.itemr;
	nextrect(i, &r2, &r);
	while(i->prev != match.first->prev && nextrect(i->prev, &r2, &r))
		i = i->prev;
	return i;
}

static void
selectitem(Item *i) {
	if(i != match.sel) {
		caret_set(input.filter_start, input.pos - input.string);
		caret_insert(i->string, 0);
		match.sel = i;
		if(i == match.start->prev)
			match.start = pagestart(i);
		if(i == match.end->next)
			match.start = i;
	}
}

static void
paste(void *aux, char *str) {
	if(str)
		caret_insert(str, false);
	menu_draw();
}

static bool
kdown_event(Window *w, void *aux, XKeyEvent *e) {
	char **action, **p;
	char *key;
	char buf[128];
	int num, status;
	KeySym ksym;

	if(XFilterEvent((XEvent*)e, w->xid))
		return false;

	status = XLookupBoth;
	if(w->xic)
		num = Xutf8LookupString(w->xic, e, buf, sizeof buf - 1, &ksym, &status);
	else
		num = XLookupString(e, buf, sizeof buf - 1, &ksym, nil);

	if(status != XLookupChars && status != XLookupKeySym && status != XLookupBoth)
		return false;

	if(status == XLookupKeySym || status == XLookupBoth) {
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
			return false;
		action = find_key(key, e->state);
	}

	if(status == XLookupChars || action == nil || action[0] == nil) {
		if(num && !iscntrl(buf[0])) {
			buf[num] = '\0';
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
		default:
			return false;
		case LHISTORY:
			num = input.pos - input.string;
			amount = have(LBACKWARD) ? BACKWARD : FORWARD;
			caret_insert(history_search(amount, input.string, num), true);
			input.pos = input.string + num;
			update_filter(true);
			break;
		case LKILL:
			caret_delete(BACKWARD, amount);
			update_filter(true);
			break;
		case LDELETE:
			caret_delete(FORWARD, amount);
			update_filter(true);
			break;

		case LACCEPT:
			srv.running = false;
			if(!have(LLITERAL) && !match.sel && match.start->retstring)
				if(input.filter_start == 0 && input.pos == input.end)
					selectitem(match.start);

			if(!have(LLITERAL) && match.sel && !strcmp(input.string, match.sel->string))
				lprint(1, "%s", match.sel->retstring);
			else
				lprint(1, "%s", input.string);
			break;
		case LBACKWARD:
			caret_move(BACKWARD, amount);
			update_input();
			break;
		case LCOMPLETE:
			if(have(LNEXT))
				selectitem(match.sel ? match.sel->next : match.first);
			else if(have(LPREV))
				selectitem((match.sel ? match.sel : match.start)->prev);
			else if(have(LFIRST)) {
				match.start = match.first;
				selectitem(match.start);
			}
			else if(have(LLAST))
				selectitem(match.first->prev);
			else if(have(LNEXTPAGE))
				selectitem(match.end->next);
			else if(have(LPREVPAGE)) {
				match.start = pagestart(match.start->prev);
				selectitem(match.start);
			}
			break;
		case LFORWARD:
			caret_move(FORWARD, amount);
			update_input();
			break;
		case LPASTE:
			getselection(action[1] ? action[1] : "PRIMARY", paste, nil);
			break;
		case LREJECT:
			srv.running = false;
			result = 1;
			break;
		}
		menu_draw();
	}
	return false;
}

static bool
expose_event(Window *w, void *aux, XExposeEvent *e) {

	USED(w);
	menu_draw();
	return false;
}

static Handlers handlers = {
	.expose = expose_event,
	.kdown = kdown_event,
};

