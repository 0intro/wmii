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

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/keysymdef.h>
#include <X11/keysym.h>

char version[] = "@(#) 9menu version 1.8";

Display *dpy;		/* lovely X stuff */
int screen;
Window root;
Window menuwin;
GC gc;
unsigned long selbg;
unsigned long selfg;
unsigned long normbg;
unsigned long normfg;
unsigned long border;
char *sfgname = NULL;
char *sbgname = NULL;
char *nfgname = NULL;
char *nbgname = NULL;
char *brcname = NULL;
Colormap defcmap;
XColor color;
XFontStruct *font;
int g_argc;			/* for XSetWMProperties to use */
char **g_argv;
int f_argc;			/* for labels read from files */
char **f_argv;
char *geometry = "";
int savex, savey;

char *fontlist[] = {	/* default font list if no -font */
	"pelm.latin1.9",
	"lucm.latin1.9",
	"blit",
	"9x15bold",
	"9x15",
	"lucidasanstypewriter-12",
	"fixed",
	NULL
};

char *progname;		/* my name */
char *displayname;	/* X display */
char *fontname;		/* font */
char *filename;		/* file to read options or labels from */
enum { Warp, Teleport }  teleport;	/* teleport or warp */

char **labels;		/* list of labels and commands */
char **commands;
int numitems;

void usage(), run_menu(), spawn(), ask_wm_for_delete();
void set_wm_hints();
void redraw(), teleportmenu(), warpmouse(), restoremouse();
void memory();
int args();

/* memory --- print the out of memory message and die */

void
memory(s)
char *s;
{
	fprintf(stderr, "%s: couldn't allocate memory for %s\n", progname, s);
	exit(1);
}

/* args --- go through the argument list, set options */

int
args(int argc, char **argv)
{
	int i;
	if (argc == 0 || argv == NULL || argv[0] == '\0')
		return -1;
	for (i = 0; i < argc && argv[i] != NULL; i++) {
		if (strcmp(argv[i], "-display") == 0) {
			displayname = argv[i+1];
			i++;
		} else if (strcmp(argv[i], "-file") == 0) {
			filename = argv[i+1];
			i++;
		} else if (strcmp(argv[i], "-font") == 0) {
			fontname = argv[i+1];
			i++;
		} else if (strncmp(argv[i], "-nb", 3) == 0)
			nbgname = argv[i][3] ? &argv[i][3] : argv[++i];
		else if (strncmp(argv[i], "-nf", 3) == 0)
			nfgname = argv[i][3] ? &argv[i][3] : argv[++i];
		else if (strncmp(argv[i], "-sb", 3) == 0)
			sbgname = argv[i][3] ? &argv[i][3] : argv[++i];
		else if (strncmp(argv[i], "-sf", 3) == 0)
			sfgname = argv[i][3] ? &argv[i][3] : argv[++i];
		else if (strncmp(argv[i], "-br", 3) == 0)
			brcname = argv[i][3] ? &argv[i][3] : argv[++i];
		else if (strcmp(argv[i], "-teleport") == 0)
			teleport = Teleport;
		else if (strcmp(argv[i], "-warp") == 0)
			teleport = Warp;
		else if (strcmp(argv[i], "-version") == 0) {
			printf("%s\n", version);
			exit(0);
		} else if (argv[i][0] == '-')
			usage();
		else
			break;
	}
	return i;
}

/* main --- crack arguments, set up X stuff, run the main menu loop */

int
main(int argc, char **argv)
{
	int i, j;
	char *cp;
	XGCValues gv;
	unsigned long mask;
	int nlabels = 0;

	g_argc = argc;
	g_argv = argv;

	/* set default label name */
	if ((cp = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname = ++cp;

	++argv;
	--argc;

	i = args(argc, argv);

	numitems = argc - i;

	if (numitems <= 0 && filename == NULL)
		usage();

	if (filename) {
		/* Read options and labels from file */
		char fbuf[1024];
		FILE *fp;

		fp = fopen(filename, "r");
		if (fp == NULL) {
			fprintf(stderr, "%s: couldn't open '%s'\n", progname,
				filename);
			exit(1);
		}
		while (fgets(fbuf, sizeof fbuf, fp)) {
			char *s = fbuf;
			strtok(s, "\n");
			if (s[0] == '-') {
				char *temp[3];
				temp[0] = s;
				temp[1] = strchr(s, ' ');
				if (temp[1]) {
					*(temp[1]++) = '\0';
					s = malloc(strlen(temp[1]) + 1);
					if (s == NULL)
						memory("temporary argument");
					strcpy(s, temp[1]);
					temp[1] = s;
				}
				temp[2] = 0;
				args(temp[1] ? 2 : 1, temp);
				continue;
			}
			if (s[0] == '#')
				continue;
			/* allow - in menu items to be escaped */
			if (s[0] == '\\')
				++s;
			/* allocate space */
			if (f_argc < nlabels + 1) {
				int k;
				char **temp = malloc(sizeof(char *) * (f_argc + 5));
				if (temp == 0)
					memory("temporary item");

				for (k = 0; k < nlabels; k++)
					temp[k] = f_argv[k];

				free(f_argv);
				f_argv = temp;
				f_argc += 5;
			}
			f_argv[nlabels] = malloc(strlen(s) + 1);
			if (f_argv[nlabels] == NULL)
				memory("temporary text");
			strcpy(f_argv[nlabels], s);
			++nlabels;
		}
	}

	labels = (char **) malloc((numitems + nlabels) * sizeof(char *));
	commands = (char **) malloc((numitems + nlabels) * sizeof(char *));
	if (commands == NULL || labels == NULL)
		memory("command and label arrays");

	for (j = 0; j < numitems; j++) {
		labels[j] = argv[i + j];
		if ((cp = strchr(labels[j], ':')) != NULL) {
			*cp++ = '\0';
			commands[j] = cp;
		} else
			commands[j] = labels[j];
	}

	/*
	 * Now we no longer need i (our offset into argv) so we recycle it,
	 * while keeping the old value of j!
	 */
	for (i = 0; i < nlabels; i++) {
		labels[j] = f_argv[i];
		if ((cp = strchr(labels[j], ':')) != NULL) {
			*cp++ = '\0';
			commands[j] = cp;
		} else
			commands[j] = labels[j];
		++j;
	}

	/* And now we merge the totals */
	numitems += nlabels;

	dpy = XOpenDisplay(displayname);
	if (dpy == NULL) {
		fprintf(stderr, "%s: cannot open display", progname);
		if (displayname != NULL)
			fprintf(stderr, " %s", displayname);
		fprintf(stderr, "\n");
		exit(1);
	}
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	/*
	 * This used to be
	 * black = BlackPixel(dpy, screen);
	 * white = WhitePixel(dpy, screen);
	 */
	defcmap = DefaultColormap(dpy, screen);
	if (sbgname == NULL
	    || XParseColor(dpy, defcmap, sbgname, &color) == 0
	    || XAllocColor(dpy, defcmap, &color) == 0)
		selbg = BlackPixel(dpy, screen);
	else
		selbg = color.pixel;

	if (sfgname == NULL
	    || XParseColor(dpy, defcmap, sfgname, &color) == 0
	    || XAllocColor(dpy, defcmap, &color) == 0)
		selfg = WhitePixel(dpy, screen);
	else
		selfg = color.pixel;

	if (nbgname == NULL
	    || XParseColor(dpy, defcmap, nbgname, &color) == 0
	    || XAllocColor(dpy, defcmap, &color) == 0)
		normbg = selfg;
	else
		normbg = color.pixel;

	if (nfgname == NULL
	    || XParseColor(dpy, defcmap, nfgname, &color) == 0
	    || XAllocColor(dpy, defcmap, &color) == 0)
		normfg = selbg;
	else
		normfg = color.pixel;

	if (brcname == NULL
	    || XParseColor(dpy, defcmap, brcname, &color) == 0
	    || XAllocColor(dpy, defcmap, &color) == 0)
		border = selbg;
	else
		border = color.pixel;

	/* try user's font first */
	if (fontname != NULL) {
		font = XLoadQueryFont(dpy, fontname);
		if (font == NULL)
			fprintf(stderr, "%s: warning: can't load font %s\n",
				progname, fontname);
	}

	/* if no user font, try one of our default fonts */
	if (font == NULL) {
		for (i = 0; fontlist[i] != NULL; i++) {
			font = XLoadQueryFont(dpy, fontlist[i]);
			if (font != NULL)
				break;
		}
	}

	if (font == NULL) {
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
usage()
{
	fprintf(stderr, "usage: %s [-display <displayname>] [-font <fontname>] ", progname);
	fprintf(stderr, "[-file filename] [-geometry <geom>] ");
	fprintf(stderr, "[-{n,s}{f,b} <color>] ");
	fprintf(stderr, "[-teleport] [-warp]  [-version] menitem[:command] ...\n");
	exit(0);
}

/* run_menu --- put up the window, execute selected commands */

void
run_menu()
{
	XEvent ev;
	int i, cur, old, wide, high, dx, dy;

	dx = 0;
	for (i = 0; i < numitems; i++) {
		wide = XTextWidth(font, labels[i], strlen(labels[i])) + 4;
		if (wide > dx)
			dx = wide;
	}
	wide = dx;

	old = cur = -1;

	high = font->ascent + font->descent + 1;
	dy = numitems * high;

	set_wm_hints(wide, dy);

	enum {	MouseMask = 
		  ButtonPressMask
		| ButtonReleaseMask
		| ButtonMotionMask
		| PointerMotionMask,
		MenuMask =
		  MouseMask
		| StructureNotifyMask
		| ExposureMask
	};

	XSelectInput(dpy, menuwin, MenuMask);

	XMapWindow(dpy, menuwin);

	i = 0;		/* save menu Item position */

	for (;;) {
		XNextEvent(dpy, &ev);
		switch (ev.type) {
		default:
			fprintf(stderr, "%s: unknown ev.type %d\n",
				progname, ev.type);
			break;
		case ButtonRelease:
			/* allow button 1 or button 3 */
			if (ev.xbutton.button == Button2)
				break;
			i = ev.xbutton.y/high;
			if (ev.xbutton.x < 0 || ev.xbutton.x > wide)
				return;
			else if (i < 0 || i >= numitems)
				return;
			if (teleport == Warp)
				restoremouse();

			printf("%s\n", commands[i]);
			return;
		case ButtonPress:
		case MotionNotify:
			old = cur;
			cur = ev.xbutton.y/high;
			if (ev.xbutton.x < 0 || ev.xbutton.x > wide)
				cur = ~0;
			if (cur == old)
				break;
			redraw(cur, high, wide);
			break;
		case MapNotify:
			if (teleport == Teleport)
				teleportmenu(i, wide, high);
			else
				warpmouse(i, wide, high);
			redraw(cur = i, high, wide);
			if(XGrabPointer(dpy, menuwin, False, MouseMask,
				GrabModeAsync, GrabModeAsync,
				0, None, CurrentTime
				) != GrabSuccess) {
				fprintf(stderr, "Failed to grab the mouse\n");
			}
			break;
		case Expose:
			redraw(cur, high, wide);
			break;
		case MappingNotify:	/* why do we get this? */
			break;
		}
	}
}

/* set_wm_hints --- set all the window manager hints */

void
set_wm_hints(int wide, int high)
{
	XSetWindowAttributes wa = { 0 };
	unsigned int w, h;
	int x, y;

	/* fill in hints in order to parse geometry spec */
	XParseGeometry(geometry, &x, &y, &w, &h);
	wa.override_redirect = True;
	wa.border_pixel = border;
	wa.background_pixel = normbg;
	menuwin = XCreateWindow(dpy, root, 0, 0, wide, high,
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
redraw(int cur, int high, int wide)
{
	int tx, ty, i;

	for (i = 0; i < numitems; i++) {
		tx = (wide - XTextWidth(font, labels[i], strlen(labels[i]))) / 2;
		ty = i*high + font->ascent + 1;
		if (cur == i)
			XSetForeground(dpy, gc, selbg);
		else
			XSetForeground(dpy, gc, normbg);
		XFillRectangle(dpy, menuwin, gc, 0, i*high, wide, high);
		if (cur == i)
			XSetForeground(dpy, gc, selfg);
		else
			XSetForeground(dpy, gc, normfg);
		XDrawString(dpy, menuwin, gc, tx, ty, labels[i], strlen(labels[i]));
	}
}

/* teleportmenu --- move the menu to the right place */

void
teleportmenu(int cur, int wide, int high)
{
	int x, y, dummy;
	Window wdummy;

	if (XQueryPointer(dpy, menuwin, &wdummy, &wdummy, &x, &y,
			       &dummy, &dummy, &dummy))
		XMoveWindow(dpy, menuwin, x-wide/2, y-cur*high-high/2);
}

/* warpmouse --- bring the mouse to the menu */

void
warpmouse(int cur, int wide, int high)
{
	int dummy;
	Window wdummy;
	int offset;

	/* move tip of pointer into middle of menu item */
	offset = (font->ascent + font->descent + 1) / 2;
	offset += 6;	/* fudge factor */

	if (XQueryPointer(dpy, menuwin, &wdummy, &wdummy, &savex, &savey,
			       &dummy, &dummy, &dummy))
		XWarpPointer(dpy, None, menuwin, 0, 0, 0, 0,
				wide/2, cur*high-high/2+offset);
}

/* restoremouse --- put the mouse back where it was */

void
restoremouse()
{
	XWarpPointer(dpy, menuwin, root, 0, 0, 0, 0,
				savex, savey);
}
