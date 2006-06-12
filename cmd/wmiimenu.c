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
static Display *dpy;
static Window win;
static XRectangle irect;
static int screen;
static ItemVector allitem = {0};
static ItemVector item = {0};
static int sel = -1;
static unsigned int nextoff = 0;
static unsigned int prevoff = 0;
static unsigned int curroff = 0;
static unsigned int cmdw = 0;
static unsigned int twidth = 0;
static unsigned int cwidth = 0;
static Blitz blitz = { 0 };
static BlitzDraw draw = { 0 };
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
		tw = blitz_textwidth(dpy, &draw.font, item.data[i]);
		if(tw > irect.width / 3)
			tw = irect.width / 3;
		w += tw + irect.height;
		if(w > irect.width)
			break;
	}
	nextoff = i;

	w = cmdw + 2 * seek;
	for(i = curroff; i > 0; i--) {
		tw = blitz_textwidth(dpy, &draw.font, item.data[i]);
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

/* creates draw structs for menu mode drawing */
static void
draw_menu()
{
	unsigned int i, offx = 0;

	draw.align = WEST;

	draw.rect = irect;
	draw.rect.x = 0;
	draw.rect.y = 0;
	draw.color = normcolor;
	draw.data = nil;
	blitz_drawlabel(dpy, &draw);

	/* print command */
	if(!title || text[0]) {
		draw.data = text;
		draw.color = normcolor;
		cmdw = cwidth;
		if(cmdw && item.size)
			draw.rect.width = cmdw;
		blitz_drawlabel(dpy, &draw);
	}
	else {
		cmdw = twidth;
		draw.data = title;
		draw.color = selcolor;
		draw.rect.width = cmdw;
		blitz_drawlabel(dpy, &draw);
		blitz_drawborder(dpy, &draw);
	}
	offx += draw.rect.width;

	draw.align = CENTER;
	if(item.size) {
		draw.color = normcolor;
		draw.data = prevoff < curroff ? "<" : nil;
		draw.rect.x = offx;
		draw.rect.width = seek;
		offx += draw.rect.width;
		blitz_drawlabel(dpy, &draw);

		/* determine maximum items */
		for(i = curroff; i < nextoff; i++) {
			draw.data = item.data[i];
			draw.rect.x = offx;
			draw.rect.width = blitz_textwidth(dpy, &draw.font, draw.data);
			if(draw.rect.width > irect.width / 3)
				draw.rect.width = irect.width / 3;
			draw.rect.width += irect.height;
			if(sel == i) {
				draw.color = selcolor;
				blitz_drawlabel(dpy, &draw);
				blitz_drawborder(dpy, &draw);
			} else {
				draw.color = normcolor;
				blitz_drawlabel(dpy, &draw);
			}
			offx += draw.rect.width;
		}

		draw.color = normcolor;
		draw.data = item.size > nextoff ? ">" : nil;
		draw.rect.x = irect.width - seek;
		draw.rect.width = seek;
		blitz_drawlabel(dpy, &draw);
	}
	XCopyArea(dpy, draw.drawable, win, draw.gc, 0, 0, irect.width,
			irect.height, 0, 0);
	XSync(dpy, False);
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
	char *fontstr, *selcolstr, *normcolstr, *maxname;
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

	dpy = XOpenDisplay(0);
	if(!dpy) {
		fprintf(stderr, "%s", "wmiimenu: cannot open display\n");
		exit(1);
	}
	screen = DefaultScreen(dpy);

	maxname = read_allitems();

	/* grab as early as possible, but after reading all items!!! */
	while(XGrabKeyboard
			(dpy, RootWindow(dpy, screen), True, GrabModeAsync,
			 GrabModeAsync, CurrentTime) != GrabSuccess)
		usleep(1000);

	/* set font and colors */
	blitz_init(&blitz, dpy);
	fontstr = getenv("WMII_FONT");
	if (!fontstr)
		fontstr = strdup(BLITZ_FONT);
	blitz_loadfont(dpy, &draw.font, fontstr);
	normcolstr = getenv("WMII_NORMCOLORS");
	if (!normcolstr || strlen(normcolstr) != 23)
		normcolstr = strdup(BLITZ_NORMCOLORS);
	blitz_loadcolor(&blitz, &normcolor, normcolstr);
	selcolstr = getenv("WMII_SELCOLORS");
	if (!selcolstr || strlen(selcolstr) != 23)
		selcolstr = strdup(BLITZ_SELCOLORS);
	blitz_loadcolor(&blitz, &selcolor, selcolstr);

	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask
		| SubstructureRedirectMask | SubstructureNotifyMask;

	irect.width = DisplayWidth(dpy, screen);
	irect.height = draw.font.ascent + draw.font.descent + 4;
	irect.y = DisplayHeight(dpy, screen) - irect.height;
	irect.x = 0;

	win = XCreateWindow(dpy, RootWindow(dpy, screen), irect.x, irect.y,
			irect.width, irect.height, 0, DefaultDepth(dpy, screen),
			CopyFromParent, DefaultVisual(dpy, screen),
			CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
	XDefineCursor(dpy, win, XCreateFontCursor(dpy, XC_xterm));
	XSync(dpy, False);

	/* pixmap */
	draw.gc = XCreateGC(dpy, win, 0, 0);
	draw.drawable = XCreatePixmap(dpy, win, irect.width, irect.height,
			DefaultDepth(dpy, screen));

	XSync(dpy, False);

	if(maxname)
		cwidth = blitz_textwidth(dpy, &draw.font, maxname) + irect.height;
	if(cwidth > irect.width / 3)
		cwidth = irect.width / 3;

	if(title) {
		twidth = blitz_textwidth(dpy, &draw.font, title) + irect.height;
		if(twidth > irect.width / 3)
			twidth = irect.width / 3;
	}

	cmdw = title ? twidth : cwidth;

	text[0] = 0;
	update_items(text);
	XMapRaised(dpy, win);
	draw_menu();
	XSync(dpy, False);

	/* main event loop */
	while(!XNextEvent(dpy, &ev)) {
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

	blitz_deinit(&blitz);
	XUngrabKeyboard(dpy, CurrentTime);
	XFreePixmap(dpy, draw.drawable);
	XFreeGC(dpy, draw.gc);
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);

	return ret;
}
