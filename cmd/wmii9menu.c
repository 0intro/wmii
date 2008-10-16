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

#define IXP_NO_P9_
#define IXP_P9_STRUCTS
#include <ixp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include <x11.h>

char version[] = "@(#) wmii9menu version 1.8";

static Window*	menuwin;

static CTuple	cnorm;
static CTuple	csel;
static Font*	font;

static IxpClient* client;
static IxpCFid*	ctlfid;
static char 	ctl[1024];
static char*	ectl;
static char*	address;

static int	wborder;

char	buffer[8092];
char*	_buffer;

static char*
readctl(char *key) {
	char *s, *p;
	int nkey, n;

	nkey = strlen(key);
	p = ctl - 1;
	do {
		p++;
		if(!strncmp(p, key, nkey)) {
			p += nkey;
			s = strchr(p, '\n');
			n = (s ? s : ectl) - p;
			s = freelater(emalloc(n + 1));
			s[n] = '\0';
			return strncpy(s, p, n);
		}
	} while((p = strchr(p, '\n')));
	return "";
}

/* for XSetWMProperties to use */
int g_argc;
char **g_argv;

char *initial = "";
int cur;

static char**	labels;		/* list of labels and commands */
static char**	commands;
static int	numitems;

void usage(void);
void run_menu(void);
void create_window(void);
void size_window(int, int);
void redraw(int, int);
void warpmouse(int, int);
void memory(void);
int args(void);

/* args --- go through the argument list, set options */

struct {
	char *name, **var;
} argtab[] = {
	{"initial", &initial},
	{"a", &address},
	{0, },
}, *ap;

/* main --- crack arguments, set up X stuff, run the main menu loop */

int
main(int argc, char **argv)
{
	int i, n;
	char *cp;

	g_argc = argc;
	g_argv = argv;

	/* set default label name */
	if((cp = strrchr(argv[0], '/')) != nil)
		argv0 = ++cp;
	else
		argv0 = argv[0];

	for(i = 1; i < argc && argv[i][0] == '-'; i++) {
		if(strcmp(argv[i], "-version") == 0) {
			printf("%s\n", version);
			exit(0);
		}

		SET(n);
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

	initdisplay();
	create_window();

	numitems = argc;

	labels = emalloc(numitems * sizeof *labels);
	commands = emalloc(numitems * sizeof *labels);

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

	if(address && *address)
		client = ixp_mount(address);
	else
		client = ixp_nsmount("wmii");
	if(client == nil)
		fatal("can't mount: %r\n");

	ctlfid = ixp_open(client, "ctl", OREAD);
	i = ixp_read(ctlfid, ctl, 1023);
	ectl = ctl + i;

	wborder = strtol(readctl("border "), nil, 10);
	loadcolor(&cnorm, readctl("normcolors "));
	loadcolor(&csel, readctl("focuscolors "));
	font = loadfont(readctl("font "));
	if(!font)
		fatal("Can't load font");

	run_menu();

	XCloseDisplay(display);
	return 0;
}

/* usage --- print a usage message and die */

void
usage(void)
{
	fprintf(stderr, "usage: %s -version\n", argv0);
	fprintf(stderr, "       %s [-a <address>] [-initial <arg>] menitem[:command] ...\n", argv0);
	exit(0);
}

/* run_menu --- put up the window, execute selected commands */

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

void
run_menu(void)
{
	XEvent ev;
	int i, old, wide, high;

	high = labelh(font);
	for(i = 0; i < numitems; i++)
		wide = max(wide, textwidth(font, labels[i]));
	wide += font->height & ~1;

	size_window(wide, high * i);
	warpmouse(wide, high);

	for(;;) {
		XNextEvent(display, &ev);
		switch (ev.type) {
		default:
			fprintf(stderr, "%s: unknown ev.type %d\n",
				argv0, ev.type);
			break;
		case ButtonRelease:
			i = ev.xbutton.y / high;
			if(ev.xbutton.x < 0 || ev.xbutton.x > wide)
				return;
			else if(i < 0 || i >= numitems)
				return;

			printf("%s\n", commands[i]);
			return;
		case ButtonPress:
		case MotionNotify:
			old = cur;
			cur = ev.xbutton.y / high;
			if(ev.xbutton.x < 0 || ev.xbutton.x > wide)
				cur = ~0;
			if(cur == old)
				break;
			redraw(high, wide);
			break;
		case MapNotify:
			redraw(high, wide);
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
create_window(void)
{
	WinAttr wa = { 0 };
	XEvent e;

	wa.override_redirect = true;
	menuwin = createwindow(&scr.root, Rect(-1, -1, 0, 0),
			       scr.depth, InputOutput,
			       &wa, CWOverrideRedirect);
	selectinput(menuwin, MenuMask);
	mapwin(menuwin);
	XMaskEvent(display, StructureNotifyMask, &e);
	if(!grabpointer(menuwin, nil, 0, MouseMask))
		fatal("Failed to grab the mouse\n");
	XSetCommand(display, menuwin->w, g_argv, g_argc);
}

void
size_window(int wide, int high)
{
	Point p;
	int h;

	h = high * numitems;

	p = querypointer(&scr.root);
	p.x -= wide / 2;
	if(p.x < 0)
		p.x = 0;
	else if(p.x + wide > Dy(scr.rect))
		p.x = Dy(scr.rect) - wide;

	p.y -= cur * high + high / 2;
	if(p.y < 0)
		p.y = 0;
	else if(p.y + h > Dy(scr.rect))
		p.y = Dy(scr.rect) - h;

	reshapewin(menuwin, Rpt(p, addpt(p, Pt(wide, high))));

	//XSetWindowBackground(display, menuwin->w, cnorm.bg);
	setborder(menuwin, 1, cnorm.border);
}

/* redraw --- actually redraw the menu */

void
redraw(int high, int wide)
{
	Rectangle r;
	CTuple *c;
	int i;

	r = Rect(0, 0, wide, high);
	for(i = 0; i < numitems; i++) {
		if(cur == i)
			c = &csel;
		else
			c = &cnorm;
		r = rectsetorigin(r, Pt(0, i * high));
		fill(menuwin, r, c->bg);
		drawstring(menuwin, font, r, Center, labels[i], c->fg);
	}
	drawstring(menuwin, font, Rect(0, 0, 15, 15), West, "foo", cnorm.fg);
}

/* warpmouse --- bring the mouse to the menu */

void
warpmouse(int wide, int high)
{
	Point p;
	int offset;

	/* move tip of pointer into middle of menu item */
	offset = labelh(font) / 2;
	offset += 6;	/* fudge factor */

	p = Pt(wide / 2, cur*high - high/2 + offset);
	p = addpt(p, menuwin->r.min);

	warppointer(p);
}

