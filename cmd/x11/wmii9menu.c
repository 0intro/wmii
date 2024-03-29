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
#include <fmt.h>
#include <ixp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <stuff/clientutil.h>
#include <stuff/util.h>
#include <stuff/x.h>

char *argv0;
Display *display;
Screen scr;

char version[] = "wmii9menu-"VERSION" "COPYRIGHT", ©1994 David Hogan, Arnold Robbins";

static Window*	menuwin;

static CTuple	cnorm;
static CTuple	csel;
static Font*	font;

static int	wborder;

static char*	initial = "";
static int	cur;

static char**	labels;		/* list of labels and commands */
static char**	commands;
static int	numitems;

void usage(void);
void run_menu(void);
void create_window(void);
void size_window(int, int);
void redraw(int, int);
void warpmouse(int, int);

void
init_screens(void) {
	Rectangle *rects;
	Point p;
	int i, n;

	rects = xinerama_screens(&n);
	p = querypointer(&scr.root);
	for(i=0; i < n; i++) {
		if(rect_haspoint_p(rects[i], p))
			break;
	}
	if(i == n)
		i = 0;
	scr.rect = rects[i];
}

/* main --- crack arguments, set up X stuff, run the main menu loop */

int
main(int argc, char **argv)
{
	static char *address;
	char *cp;
	int i;

	ARGBEGIN{
	case 'v':
		lprint(1, "%s\n", version);
		return 0;
	case 'a':
		address = EARGF(usage());
		break;
	case 'i':
		initial = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;

	if(argc == 0)
		usage();

	initdisplay();
	xext_init();
	init_screens();
	create_window();

	numitems = argc;
	labels = emalloc(numitems * sizeof *labels);
	commands = emalloc(numitems * sizeof *labels);

	for(i = 0; i < numitems; i++) {
		labels[i] = argv[i];
		commands[i] = argv[i];
		if((cp = strchr(labels[i], ':')) != nil) {
			*cp++ = '\0';
			commands[i] = cp;
		}
		if(strcmp(labels[i], initial) == 0)
			cur = i;
	}

	client_init(address);

	wborder = strtol(readctl("/ctl", "border "), nil, 10);
	client_readconfig(&cnorm, &csel, &font);

	run_menu();

	XCloseDisplay(display);
	return 0;
}

void
usage(void)
{
	lprint(2, "usage: %s [-a <address>] [-i <arg>] <menitem>[:<command>] ...\n", argv0);
	lprint(2, "       %s -v\n", argv0);
	exit(0);
}

enum {
	MouseMask = ButtonPressMask
		  | ButtonReleaseMask
		  | ButtonMotionMask
		  | PointerMotionMask,
	MenuMask = MouseMask
		 | StructureNotifyMask
		 | ExposureMask
};

void
run_menu(void)
{
	XEvent ev;
	int i, old, wide, high;

	high = labelh(font);
	wide = 0;
	for(i = 0; i < numitems; i++)
		wide = max(wide, textwidth(font, labels[i]));
	wide += font->height & ~1;

	size_window(wide, high);
	warpmouse(wide, high);

	for(;;) {
		XNextEvent(display, &ev);
		switch (ev.type) {
		default:
			lprint(2, "%s: unknown ev.type %d\n", argv0, ev.type);
			break;
		case ButtonRelease:
			i = ev.xbutton.y / high;
			if(ev.xbutton.x < 0 || ev.xbutton.x > wide)
				return;
			else if(i < 0 || i >= numitems)
				return;

			lprint(1, "%s\n", commands[i]);
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
		case Expose:
			redraw(high, wide);
			break;
		case MapNotify:
		case ConfigureNotify:
		case MappingNotify:
			break;
		}
	}
}

void
create_window(void)
{
	WinAttr wa = { 0 };

	wa.override_redirect = true;
	menuwin = createwindow(&scr.root, Rect(-1, -1, 0, 0),
			       scr.depth, InputOutput,
			       &wa, CWOverrideRedirect);
	selectinput(menuwin, MenuMask);
	mapwin(menuwin);
	if(!grabpointer(menuwin, nil, 0, MouseMask))
		fatal("Failed to grab the mouse\n");
}

void
size_window(int wide, int high)
{
	Rectangle r;
	Point p;
	int h;

	h = high * numitems;
	r = Rect(0, 0, wide, h);

	p = querypointer(&scr.root);
	p.x -= wide / 2;
	p.x = max(p.x, scr.rect.min.x);
	p.x = min(p.x, scr.rect.max.x - wide);

	p.y -= cur * high + high / 2;
	p.y = max(p.y, scr.rect.min.y);
	p.y = min(p.y, scr.rect.max.y - h);

	reshapewin(menuwin, rectaddpt(r, p));
	setborder(menuwin, 1, &cnorm.border);
}

void
redraw(int high, int wide)
{
	Rectangle r;
	int i;

	r = Rect(0, 0, wide, high);
	for(i = 0; i < numitems; i++) {
		r = rectsetorigin(r, Pt(0, i * high));
		fillstring(menuwin, font, r, Center, labels[i], (cur == i ? &csel : &cnorm), 0);
	}
}

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

