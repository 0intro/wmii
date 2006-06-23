/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * (C)opyright MMVI      Sander van Dijk <a dot h dot vandijk at gmail dot com>
 * See LICENSE file for license details.
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <blitz.h>
#include <cext.h>

VECTOR(ItemVector, char *);
static char *title = nil;
static Bool done = False;
static int ret = 0;
static char text[4096];
static BlitzColor selcolor;
static BlitzColor normcolor;
static Window win;
static XRectangle irect;
static ItemVector allitem = {0};
static ItemVector item = {0};
static int sel = -1;
static unsigned int nextoff = 0;
static unsigned int prevoff = 0;
static unsigned int curroff = 0;
static unsigned int cmdw = 0;
static unsigned int twidth = 0;
static unsigned int cwidth = 0;
static Blitz blz = {0};
static BlitzBrush brush = {0};
static const int seek = 30;		/* 30px */

static void draw_menu(void);
static void handle_kpress(XKeyEvent * e);

static char version[] = "wmiimenu - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";

static Vector *
item2vector(ItemVector *iv)
{
	return (Vector *) iv;
}

static void
usage()
{
	fprintf(stderr, "%s", "usage: wmiimenu [-v] [-t <title>]\n");
	exit(1);
}

static void
update_offsets()
{
	unsigned int i;
	unsigned int tw, w = cmdw + 2 * seek;

	if(!item.size)
		return;

	for(i = curroff; i < item.size; i++) {
		tw = blitz_textwidth(brush.font, item.data[i]);
		if(tw > irect.width / 3)
			tw = irect.width / 3;
		w += tw + irect.height;
		if(w > irect.width)
			break;
	}
	nextoff = i;

	w = cmdw + 2 * seek;
	for(i = curroff; i > 0; i--) {
		tw = blitz_textwidth(brush.font, item.data[i]);
		if(tw > irect.width / 3)
			tw = irect.width / 3;
		w += tw + irect.height;
		if(w > irect.width)
			break;
	}
	prevoff = i;
}

static unsigned int
update_items(char *pattern)
{
	unsigned int plen = strlen(pattern);
	int i;

	if(*pattern)
		cmdw = cwidth;
	else
		cmdw = twidth;

	curroff = prevoff = nextoff = 0;
	sel = -1;

	while(item.size)
		cext_vdetach(item2vector(&item), item.data[0]);

	for(i = 0; i < allitem.size; i++)
		if(!plen || !strncmp(pattern, allitem.data[i], plen)) 
			cext_vattach(item2vector(&item), allitem.data[i]);
	for(i = 0; i < allitem.size; i++)
		if(plen && strncmp(pattern, allitem.data[i], plen)
				&& strstr(allitem.data[i], pattern))
			cext_vattach(item2vector(&item), allitem.data[i]);
	if(item.size)
		sel = 0;

	update_offsets();
	return item.size;
}

/* creates brush structs for brush mode drawing */
static void
draw_menu()
{
	unsigned int i, offx = 0;

	brush.align = WEST;

	brush.rect = irect;
	brush.rect.x = 0;
	brush.rect.y = 0;
	brush.color = normcolor;
	brush.border = True;
	blitz_draw_tile(&brush);

	/* print command */
	if(!title || text[0]) {
		brush.color = normcolor;
		cmdw = cwidth;
		if(cmdw && item.size)
			brush.rect.width = cmdw;
		blitz_draw_label(&brush, text);
	}
	else {
		cmdw = twidth;
		brush.color = selcolor;
		brush.rect.width = cmdw;
		blitz_draw_label(&brush, title);
	}
	offx += brush.rect.width;

	brush.align = CENTER;
	if(item.size) {
		brush.color = normcolor;
		brush.rect.x = offx;
		brush.rect.width = seek;
		offx += brush.rect.width;
		blitz_draw_label(&brush, prevoff < curroff ? "<" : nil);

		/* determine maximum items */
		for(i = curroff; i < nextoff; i++) {
			brush.border = False;
			brush.rect.x = offx;
			brush.rect.width = blitz_textwidth(brush.font, item.data[i]);
			if(brush.rect.width > irect.width / 3)
				brush.rect.width = irect.width / 3;
			brush.rect.width += irect.height;
			if(sel == i) {
				brush.color = selcolor;
				brush.border = True;
			}
			else
				brush.color = normcolor;
			blitz_draw_label(&brush, item.data[i]);
			offx += brush.rect.width;
		}

		brush.color = normcolor;
		brush.border = False;
		brush.rect.x = irect.width - seek;
		brush.rect.width = seek;
		blitz_draw_label(&brush, item.size > nextoff ? ">" : nil);
	}
	XCopyArea(blz.display, brush.drawable, win, brush.gc, 0, 0, irect.width,
			irect.height, 0, 0);
	XSync(blz.display, False);
}

static void
handle_kpress(XKeyEvent * e)
{
	KeySym ksym;
	char buf[32];
	int num;
	unsigned int len = strlen(text);

	buf[0] = 0;
	num = XLookupString(e, buf, sizeof(buf), &ksym, 0);

	if(IsFunctionKey(ksym) || IsKeypadKey(ksym)
			|| IsMiscFunctionKey(ksym) || IsPFKey(ksym)
			|| IsPrivateKeypadKey(ksym))
		return;

	/* first check if a control mask is omitted */
	if(e->state & ControlMask) {
		switch (ksym) {
		case XK_H:
		case XK_h:
			ksym = XK_BackSpace;
			break;
		case XK_I:
		case XK_i:
			ksym = XK_Tab;
			break;
		case XK_J:
		case XK_j:
			ksym = XK_Return;
			break;
		case XK_N:
		case XK_n:
			ksym = XK_Right;
			break;
		case XK_P:
		case XK_p:
			ksym = XK_Left;
			break;
		case XK_U:
		case XK_u:
			text[0] = 0;
			update_items(text);
			draw_menu();
			return;
			break;
		case XK_bracketleft:
			ksym = XK_Escape;
			break;
		default:	/* ignore other control sequences */
			return;
			break;
		}
	}
	switch (ksym) {
	case XK_Left:
		if(sel <= 0)
			return;
		sel--;
		break;
	case XK_Tab:
		if(!item.size)
			return;
		cext_strlcpy(text, item.data[sel], sizeof(text));
		update_items(text);
		break;
	case XK_Right:
		if(sel < 0 || (sel + 1 == item.size))
			return;
		sel++;
		break;
	case XK_Return:
		if(e->state & ShiftMask) {
			if(text)
				fprintf(stdout, "%s", text);
		}
		else if(sel >= 0)
			fprintf(stdout, "%s", item.data[sel]);
		else if(text)
			fprintf(stdout, "%s", text);
		fflush(stdout);
		done = True;
		break;
	case XK_Escape:
		ret = 1;
		done = True;
		break;
	case XK_BackSpace:
		if(len) {
			unsigned int i = len;
			if(i) {
				int prev_nitem;
				do
					text[--i] = 0;
				while((prev_nitem = item.size) && i &&
						prev_nitem == update_items(text));
			}
			update_items(text);
		}
		break;
	default:
		if((num == 1) && !iscntrl((int) buf[0])) {
			buf[num] = 0;
			if(len > 0)
				cext_strlcat(text, buf, sizeof(text));
			else
				cext_strlcpy(text, buf, sizeof(text));
			update_items(text);
		}
	}
	if(sel >= 0) {
		if(sel == curroff - 1) {
			curroff = prevoff;
			update_offsets();
		} else if((sel == nextoff) && (item.size > nextoff)) {
			curroff = nextoff;
			update_offsets();
		}
	}
	draw_menu();
}

static char *
read_allitems()
{
	static char *maxname = nil;
    char *p, buf[1024];
	unsigned int len = 0, max = 0;

	while(fgets(buf, sizeof(buf), stdin)) {
		len = strlen(buf);
		if (buf[len - 1] == '\n') /* there might be no \n after the last item */
			buf[len - 1] = 0; /* removing \n */
		p = strdup(buf);
		if(max < len) {
			maxname = p;
			max = len;
		}
		cext_vattach(item2vector(&allitem), p);
	}

	return maxname;
}

int
main(int argc, char *argv[])
{
	int i;
	XSetWindowAttributes wa;
	char *maxname, *p;
	BlitzFont font = {0};
	GC gc;
	Drawable pmap;
	XEvent ev;

	/* command line args */
	for(i = 1; i < argc; i++) {
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'v':
				fprintf(stdout, "%s", version);
				exit(0);
				break;
			case 't':
				if(++i < argc)
					title = argv[i];
				else
					usage();
				break;
			default:
				usage();
				break;
			}
		else
			usage();
	}

	blz.display = XOpenDisplay(0);
	if(!blz.display) {
		fprintf(stderr, "%s", "wmiimenu: cannot open display\n");
		exit(1);
	}
	blz.screen = DefaultScreen(blz.display);
	blz.root = RootWindow(blz.display, blz.screen);

	maxname = read_allitems();

	/* grab as early as possible, but after reading all items!!! */
	while(XGrabKeyboard
			(blz.display, blz.root, True, GrabModeAsync,
			 GrabModeAsync, CurrentTime) != GrabSuccess)
		usleep(1000);

	font.fontstr = getenv("WMII_FONT");
	if (!font.fontstr)
		font.fontstr = strdup(BLITZ_FONT);
	blitz_loadfont(&blz, &font);

	if((p = getenv("WMII_NORMCOLORS")))
		cext_strlcpy(normcolor.colstr, p, sizeof(normcolor.colstr));
	if(strlen(normcolor.colstr) != 23)
		cext_strlcpy(normcolor.colstr, BLITZ_NORMCOLORS, sizeof(normcolor.colstr));
	blitz_loadcolor(&blz, &normcolor);

	if((p = getenv("WMII_SELCOLORS")))
		cext_strlcpy(selcolor.colstr, p, sizeof(selcolor.colstr));
	if(strlen(selcolor.colstr) != 23)
		cext_strlcpy(selcolor.colstr, BLITZ_SELCOLORS, sizeof(selcolor.colstr));
	blitz_loadcolor(&blz, &selcolor);

	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask
		| SubstructureRedirectMask | SubstructureNotifyMask;

	irect.width = DisplayWidth(blz.display, blz.screen);
	irect.height = font.ascent + font.descent + 4;
	irect.y = DisplayHeight(blz.display, blz.screen) - irect.height;
	irect.x = 0;

	win = XCreateWindow(blz.display, blz.root, irect.x, irect.y,
			irect.width, irect.height, 0, DefaultDepth(blz.display, blz.screen),
			CopyFromParent, DefaultVisual(blz.display, blz.screen),
			CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
	XDefineCursor(blz.display, win, XCreateFontCursor(blz.display, XC_xterm));
	XSync(blz.display, False);

	/* pixmap */
	gc = XCreateGC(blz.display, win, 0, 0);
	pmap = XCreatePixmap(blz.display, win, irect.width, irect.height,
			DefaultDepth(blz.display, blz.screen));

	XSync(blz.display, False);

	brush.blitz = &blz;
	brush.color = normcolor;
	brush.drawable = pmap;
	brush.gc = gc;
	brush.font = &font;

	if(maxname)
		cwidth = blitz_textwidth(brush.font, maxname) + irect.height;
	if(cwidth > irect.width / 3)
		cwidth = irect.width / 3;

	if(title) {
		twidth = blitz_textwidth(brush.font, title) + irect.height;
		if(twidth > irect.width / 3)
			twidth = irect.width / 3;
	}

	cmdw = title ? twidth : cwidth;

	text[0] = 0;
	update_items(text);
	XMapRaised(blz.display, win);
	draw_menu();
	XSync(blz.display, False);

	/* main event loop */
	while(!XNextEvent(blz.display, &ev)) {
		switch (ev.type) {
			case KeyPress:
				handle_kpress(&ev.xkey);
				break;
			case Expose:
				if(ev.xexpose.count == 0) {
					draw_menu();
				}
				break;
			default:
				break;
		}
		if(done)
			break;
	}

	XUngrabKeyboard(blz.display, CurrentTime);
	XFreePixmap(blz.display, pmap);
	XFreeGC(blz.display, gc);
	XDestroyWindow(blz.display, win);
	XCloseDisplay(blz.display);

	return ret;
}
