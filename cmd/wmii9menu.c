/* Licence
 * =======
 * 
 *   9menu is free software, and is Copyright (c) 1994 by David Hogan and
 *   Arnold Robbins. Permission is granted to all sentient beings to use
 *   this software, to make copies of it, and to distribute those copies,
 *   provided that:
 * 
 *       (1) the copyright and licence notices are left intact
 *       (2) the recipients are aware that it is free software
 *       (3) any unapproved changes in functionality are either
 *             (i) only distributed as patches
 *         or (ii) distributed as a new program which is not called 9menu
 *                 and whose documentation gives credit where it is due
 *       (4) the authors are not held responsible for any defects
 *           or shortcomings in the software, or damages caused by it.
 * 
 *   There is no warranty for this software.  Have a nice day.
 * 
 * --
 * Arnold Robbins
 * arnold@skeeve.com
 *
 * 9menu.c
 *
 * This program puts up a window that is just a menu, and executes
 * commands that correspond to the items selected.
 *
 * Initial idea: Arnold Robbins
 * Version using libXg: Matty Farrow (some ideas borrowed)
 * This code by: David Hogan and Arnold Robbins
 */

/* 
 * Heavily modified by Kris Maglione for use with wmii.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xutil.h>
#include <util.h>

char version[] = "@(#) wmii9menu version 1.8";

/* lovely X stuff */
Display *dpy;
int screen;
Window root;
Window menuwin;
Colormap defcmap;
XColor color;
XFontStruct *font;
GC gc;

ulong selfg, selbg;
ulong normfg, normbg;
ulong border;
char *sfgname, *sbgname;
char *nfgname, *nbgname;
char *brcname;

/* for XSetWMProperties to use */
int g_argc;
char **g_argv;

char *initial = "";
int cur;

char *fontlist[] = {	/* default font list if no -font */
	"pelm.latin1.9",
	"lucm.latin1.9",
	"blit",
	"9x15bold",
	"9x15",
	"lucidasanstypewriter-12",
	"fixed",
	nil
};

char *progname;		/* my name */
char *displayname;	/* X display */
char *fontname;		/* font */

char **labels;		/* list of labels and commands */
char **commands;
int numitems;

void usage(void);
void run_menu(void);
void create_window(int, int);
void redraw(int, int);
void warpmouse(int, int);
void memory(void);
int args(void);

/* args --- go through the argument list, set options */

struct {
	char *name, **var;
} argtab[] = {
	{"display", &displayname},
	{"initial", &initial},
	{"font", &fontname},
	{"nb", &nbgname},
	{"nf", &nfgname},
	{"sb", &sbgname},
	{"sf", &sfgname},
	{"br", &brcname},
	{0, },
}, *ap;

static ulong
getcolor(char *name, ulong def) {
	if((name != nil)
	 && (XParseColor(dpy, defcmap, name, &color) != 0)
	 && (XAllocColor(dpy, defcmap, &color) != 0))
		return color.pixel;
	else
		return def;
}

/* main --- crack arguments, set up X stuff, run the main menu loop */

int
main(int argc, char **argv)
{
	int i, n;
	char *cp;
	XGCValues gv;
	ulong mask;

	g_argc = argc;
	g_argv = argv;

	/* set default label name */
	if((cp = strrchr(argv[0], '/')) != nil)
		progname = ++cp;
	else
		progname = argv[0];

	for(i = 1; i < argc && argv[i][0] == '-'; i++) {
		if(strcmp(argv[i], "-version") == 0) {
			printf("%s\n", version);
			exit(0);
		}

		for(ap = argtab; ap->name; ap++) {
			n = strlen(ap->name);
			if(strncmp(ap->name, argv[i]+1, n) == 0)
				break;
		}
		if(ap->name == 0)
			usage();

		if(argv[i][n+1] != '\0')
			*ap->var = &argv[i][n+1];
		else {
			if(argc <= i+1)
				usage();
			*ap->var = argv[++i];
		}
	}
	argc -= i, argv += i;

	if(argc == 0)
		usage();

	numitems = argc;

	labels = emalloc(numitems * sizeof(*labels));
	commands = emalloc(numitems * sizeof(*labels));

	for(i = 0; i < numitems; i++) {
		labels[i] = argv[i];
		if((cp = strchr(labels[i], ':')) != nil) {
			*cp++ = '\0';
			commands[i] = cp;
		} else
			commands[i] = labels[i];
		if(strcmp(labels[i], initial) == 0)
			cur = i;
	}

	dpy = XOpenDisplay(displayname);
	if(dpy == nil) {
		fprintf(stderr, "%s: cannot open display", progname);
		if(displayname != nil)
			fprintf(stderr, " %s", displayname);
		fprintf(stderr, "\n");
		exit(1);
	}
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	defcmap = DefaultColormap(dpy, screen);

	selbg = getcolor(sbgname, BlackPixel(dpy, screen));
	selfg = getcolor(sfgname, WhitePixel(dpy, screen));
	normbg = getcolor(nbgname, selfg);
	normfg = getcolor(nfgname, selbg);
	border = getcolor(brcname, selbg);

	/* try user's font first */
	if(fontname != nil) {
		font = XLoadQueryFont(dpy, fontname);
		if(font == nil)
			fprintf(stderr, "%s: warning: can't load font '%s'\n",
				progname, fontname);
	}

	/* if no user font, try one of our default fonts */
	if(font == nil) {
		for(i = 0; fontlist[i] != nil; i++) {
			font = XLoadQueryFont(dpy, fontlist[i]);
			if(font != nil)
				break;
		}
	}

	if(font == nil) {
		fprintf(stderr, "%s: fatal: cannot load a font\n", progname);
		exit(1);
	}

	gv.foreground = normfg;
	gv.background = normbg;
	gv.font = font->fid;
	gv.line_width = 0;
	mask = GCForeground | GCBackground | GCFont | GCLineWidth;
	gc = XCreateGC(dpy, root, mask, &gv);

	run_menu();

	XCloseDisplay(dpy);
	exit(0);
}

/* usage --- print a usage message and die */

void
usage(void)
{
	fprintf(stderr, "usage: %s [-display <displayname>] [-font <fontname>] ", progname);
	fprintf(stderr, "[-{n,s}{f,b} <color>] [-br <color>] ");
	fprintf(stderr, "[-version] menitem[:command] ...\n");
	exit(0);
}

/* run_menu --- put up the window, execute selected commands */

void
run_menu(void)
{
	XEvent ev;
	int i, old, wide, high, dx, dy;

	dx = 0;
	for(i = 0; i < numitems; i++) {
		wide = XTextWidth(font, labels[i], strlen(labels[i])) + 4;
		if(wide > dx)
			dx = wide;
	}
	wide = dx;

	high = font->ascent + font->descent + 1;
	dy = numitems * high;

	enum {
	MouseMask = 
		  ButtonPressMask
		| ButtonReleaseMask
		| ButtonMotionMask
		| PointerMotionMask,
	MenuMask =
		  MouseMask
		| StructureNotifyMask
		| ExposureMask
	};

	create_window(wide, high);
	warpmouse(wide, high);

	XSelectInput(dpy, menuwin, MenuMask);

	XMapWindow(dpy, menuwin);

	i = 0;		/* save menu Item position */

	for(;;) {
		XNextEvent(dpy, &ev);
		switch (ev.type) {
		default:
			fprintf(stderr, "%s: unknown ev.type %d\n",
				progname, ev.type);
			break;
		case ButtonRelease:
			i = ev.xbutton.y/high;
			if(ev.xbutton.x < 0 || ev.xbutton.x > wide)
				return;
			else if(i < 0 || i >= numitems)
				return;

			printf("%s\n", commands[i]);
			return;
		case ButtonPress:
		case MotionNotify:
			old = cur;
			cur = ev.xbutton.y/high;
			if(ev.xbutton.x < 0 || ev.xbutton.x > wide)
				cur = ~0;
			if(cur == old)
				break;
			redraw(high, wide);
			break;
		case MapNotify:
			redraw(high, wide);
			if(XGrabPointer(dpy, menuwin,
					False, MouseMask,
					GrabModeAsync, GrabModeAsync,
					0, None, CurrentTime) != GrabSuccess)
				fprintf(stderr, "Failed to grab the mouse\n");
			break;
		case Expose:
			redraw(high, wide);
			break;
		case MappingNotify:	/* why do we get this? */
			break;
		}
	}
}

/* set_wm_hints --- set all the window manager hints */

void
create_window(int wide, int high)
{
	XSetWindowAttributes wa = { 0 };
	uint h;
	int x, y, dummy;
	Window wdummy;
	
	h = high * numitems;

	XQueryPointer(dpy, root, &wdummy, &wdummy, &x, &y,
				&dummy, &dummy, (uint*)&dummy);
	x -= wide / 2;
	if(x < 0)
		x = 0;
	else if(x + wide > DisplayWidth(dpy, screen))
		x = DisplayWidth(dpy, screen) - wide;

	y -= cur * high + high / 2;
	if(y < 0)
		y = 0;
	else if(y + h > DisplayHeight(dpy, screen))
		y = DisplayHeight(dpy, screen) - h;

	wa.override_redirect = True;
	wa.border_pixel = border;
	wa.background_pixel = normbg;
	menuwin = XCreateWindow(dpy, root, x, y, wide, h,
				1, DefaultDepth(dpy, screen), CopyFromParent,
				DefaultVisual(dpy, screen),
				  CWOverrideRedirect
				| CWBackPixel
				| CWBorderPixel
				| CWEventMask,
				&wa);

	XSetCommand(dpy, menuwin, g_argv, g_argc);
}

/* redraw --- actually redraw the menu */

void
redraw(int high, int wide)
{
	int tx, ty, i;

	for(i = 0; i < numitems; i++) {
		tx = (wide - XTextWidth(font, labels[i], strlen(labels[i]))) / 2;
		ty = i*high + font->ascent + 1;
		if(cur == i)
			XSetForeground(dpy, gc, selbg);
		else
			XSetForeground(dpy, gc, normbg);
		XFillRectangle(dpy, menuwin, gc, 0, i*high, wide, high);
		if(cur == i)
			XSetForeground(dpy, gc, selfg);
		else
			XSetForeground(dpy, gc, normfg);
		XDrawString(dpy, menuwin, gc, tx, ty, labels[i], strlen(labels[i]));
	}
}

/* warpmouse --- bring the mouse to the menu */

void
warpmouse(int wide, int high)
{
	int offset;

	/* move tip of pointer into middle of menu item */
	offset = (font->ascent + font->descent + 1) / 2;
	offset += 6;	/* fudge factor */

	XWarpPointer(dpy, None, menuwin, 0, 0, 0, 0,
				wide/2, cur*high-high/2+offset);
}
