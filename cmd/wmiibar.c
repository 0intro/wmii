/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

#include "wmii.h"

/* array indexes for file pointers */
typedef enum {
	B_CTL,
	B_NEW,
	B_EXPANDABLE,
	B_GEOMETRY,
	B_FONT,
	B_FG_COLOR,
	B_BORDER_COLOR,
	B_BG_COLOR,
	B_LAST
} BarIndexes;

typedef struct {
	File *root;
	Draw d;
} Item;

static unsigned int id = 0;
static IXPServer *ixps = 0;
static Display *dpy;
static GC gc;
static Window win;
static XRectangle rect;
static XRectangle brect;
static int screen_num;
static int displayed = 0;
static char *sockfile = 0;
static File *file[B_LAST];
static Container items = {0};
static Pixmap pmap;
static XFontStruct *font;
static Align align = SOUTH;

static Draw zero_draw = { 0 };

static void draw_bar(void *obj, char *arg);
static void quit(void *obj, char *arg);
static void display(void *obj, char *arg);
static void reset(void *obj, char *arg);
static void _destroy(void *obj, char *arg);
static void handle_after_write(IXPServer * s, File * f);

static Action acttbl[] = {
	{"quit", quit},
	{"display", display},
	{"update", draw_bar},
	{"reset", reset},
	{"destroy", _destroy},
	{0, 0}
};

static char *version[] = {
	"wmiibar - window manager improved bar - " VERSION "\n"
		"  (C)opyright MMIV-MMV Anselm R. Garbe\n", 0
};

static void usage()
{
	fprintf(stderr, "%s",
			"usage: wmiibar -s <socket file> [-v]\n"
			"      -s    socket file\n" "      -v    version info\n");
	exit(1);
}

/**
 * <path>/data          "<txt value>"
 * <path>/fgcolor      "#RRGGBBAA"
 * <path>/bgcolor      "#RRGGBBAA"
 * <path>/bordercolor  "#RRGGBBAA"
 * <path>/b1press       "<command>"
 * <path>/b2press       "<command>"
 * <path>/b3press       "<command>"
 * <path>/b4press       "<command>"
 * <path>/b5press       "<command>"
 */
static void create_label(char *path)
{
	File *f;
	char buf[MAX_BUF];
	int i;

	snprintf(buf, MAX_BUF, "%s/data", path);
	f = ixp_create(ixps, buf);
	f->after_write = handle_after_write;
	snprintf(buf, MAX_BUF, "%s/fgcolor", path);
	wmii_create_ixpfile(ixps, buf, file[B_FG_COLOR]->content);
	snprintf(buf, MAX_BUF, "%s/bgcolor", path);
	wmii_create_ixpfile(ixps, buf, file[B_BG_COLOR]->content);
	snprintf(buf, MAX_BUF, "%s/bordercolor", path);
	wmii_create_ixpfile(ixps, buf, file[B_BORDER_COLOR]->content);
	for (i = 1; i < 6; i++) {	/* 5 buttons events */
		snprintf(buf, MAX_BUF, "%s/b%dpress", path, i);
		ixp_create(ixps, buf);
	}
}

static void _destroy(void *obj, char *arg)
{
	char buf[512];
	if (!arg)
		return;
	snprintf(buf, sizeof(buf), "/%s", arg);
	ixps->remove(ixps, buf);
	draw_bar(0, 0);
}

static void reset(void *obj, char *arg)
{
	int i;
	char buf[512];
	for (i = 0; i < id; i++) {
		snprintf(buf, sizeof(buf), "/%d", i);
		ixps->remove(ixps, buf);
	}
	id = 0;
	draw_bar(0, 0);
}

static void quit(void *obj, char *arg)
{
	ixps->runlevel = SHUTDOWN;
}

static void display(void *obj, char *arg)
{
	if (!arg)
		return;
	displayed = blitz_strtonum(arg, 0, 1);
	if (displayed) {
		XMapRaised(dpy, win);
		draw_bar(0, 0);
	} else {
		XUnmapWindow(dpy, win);
		XSync(dpy, False);
	}
}

static void init_draw_label(char *path, Draw *d)
{
	char buf[MAX_BUF];
	File *f;

	/* text stuff */
	snprintf(buf, MAX_BUF, "%s/data", path);
	f = ixp_walk(ixps, buf);
	d->data = f->content;
	/* style stuff */
	snprintf(buf, MAX_BUF, "%s/fgcolor", path);
	f = ixp_walk(ixps, buf);
	d->fg = blitz_loadcolor(dpy, screen_num, f->content);
	snprintf(buf, MAX_BUF, "%s/bgcolor", path);
	f = ixp_walk(ixps, buf);
	d->bg = blitz_loadcolor(dpy, screen_num, f->content);
	snprintf(buf, MAX_BUF, "%s/bordercolor", path);
	f = ixp_walk(ixps, buf);
	d->border = blitz_loadcolor(dpy, screen_num, f->content);
}

static void init_item(char *path, Item *i)
{
	i->d = zero_draw;
	i->root = ixp_walk(ixps, path);
	i->d.gc = gc;
	i->d.drawable = pmap;
	i->d.rect = brect;
	i->d.rect.y = 0;
	init_draw_label(path, &i->d);
}

static int comp_str(const void *s1, const void *s2)
{
	return strcmp(*(char **) s1, *(char **) s2);
}

static void draw()
{
	Item *item;
	unsigned int i, w, xoff = 0;
	unsigned expandable = 0;
	char buf[32];
	size_t size = cext_sizeof(&items);

	if (!size)
		return;

	expandable = blitz_strtonum(file[B_EXPANDABLE]->content, 0, size);
	snprintf(buf, sizeof(buf), "/%d", expandable);
	if (!ixp_walk(ixps, buf))
		expandable = 0;

	w = 0;
	/* precalc */
	for (i = 0; i < size; i++)
		if (i != expandable) {
			item = cext_list_get_item(&items, i);
			item->d.rect.width = brect.height;
			if (item->d.data) {
				if (!strncmp(item->d.data, "%m:", 3))
					/* meter */
					item->d.rect.width = brect.height / 2;
				else
					item->d.rect.width += XTextWidth(font, item->d.data, strlen(item->d.data));
			}
			w += item->d.rect.width;
		}
	if (w > brect.width) {
		/* failsafe mode, give all labels same width */
		w = brect.width / size;
		for (i = 0; i < size; i++) {
			item = cext_list_get_item(&items, i);
			item->d.rect.width = w;
		}
		item->d.rect.width = brect.width - item->d.rect.x;
	}
	else {
		item = cext_list_get_item(&items, expandable);
		item->d.rect.width = brect.width - w;
	}
	for (i = 0; i < size; i++) {
		item = cext_list_get_item(&items, i);
		item->d.font = font;
		item->d.rect.x = xoff;
		xoff += item->d.rect.width;
		if (item->d.data && !strncmp(item->d.data, "%m:", 3))
			blitz_drawmeter(dpy, &item->d);
		else
			blitz_drawlabel(dpy, &item->d);
	}
	XCopyArea(dpy, pmap, win, gc, 0, 0, brect.width, brect.height, 0, 0);
	XSync(dpy, False);
}

static void draw_bar(void *obj, char *arg)
{
	File *label = 0;
	unsigned int i = 0, n = 0;
	Item *item;
	char buf[512];

	if (!displayed)
		return;
	while ((item = cext_stack_get_top_item(&items))) {
		cext_detach_item(&items, item);
		free(item);
	}
	snprintf(buf, sizeof(buf), "%s", "/0");
	label = ixp_walk(ixps, buf);
	if (!label) {
		Draw d = { 0 };
		/* default stuff */
		d.gc = gc;
		d.drawable = pmap;
		d.rect.width = brect.width;
		d.rect.height = brect.height;
		d.bg = blitz_loadcolor(dpy, screen_num, file[B_BG_COLOR]->content);
		d.fg = blitz_loadcolor(dpy, screen_num, file[B_FG_COLOR]->content);
		d.border = blitz_loadcolor(dpy, screen_num, file[B_BORDER_COLOR]->content);
		blitz_drawlabelnoborder(dpy, &d);
	} else {
		File *f;
		char **paths = 0;
		/*
		 * take order into account, directory names are used in
		 * alphabetical order
		 */
		n = 0;
		for (f = label; f; f = f->next)
			n++;
		paths = cext_emallocz(sizeof(char *) * n);
		i = 0;
		for (f = label; f; f = f->next)
			paths[i++] = f->name;
		qsort(paths, n, sizeof(char *), comp_str);
		for (i = 0; i < n; i++) {
			snprintf(buf, sizeof(buf), "/%s", paths[i]);
			item = cext_emallocz(sizeof(Item));
			init_item(buf, item);
			cext_attach_item(&items, item);
		}
		free(paths);
		draw();
	}
}

static int comp_item_root(void *file, void *item)
{
	File *f = file;
	Item *i = item;

	return f == i->root;
}

static Item *get_item_for_file(File *f)
{
	return cext_find_item(&items, f, comp_item_root);
}

static void iter_buttonpress(void *item, void *bpress)
{
	XButtonPressedEvent *e = bpress;
	Item *i = item;
	File *p;
	char buf[MAX_BUF];
	char path[512];

	if (blitz_ispointinrect(e->x, e->y, &i->d.rect)) {
		path[0] = 0;
		wmii_get_ixppath(i->root, path, sizeof(path));
		snprintf(buf, MAX_BUF, "%s/b%upress", path, e->button);
		if ((p = ixp_walk(ixps, buf)))
			if (p->content)
				wmii_spawn(dpy, p->content);
		return;
	}
}

static void handle_buttonpress(XButtonPressedEvent *e)
{
	cext_list_iterate(&items, e, iter_buttonpress);
}

static void check_event(Connection * e)
{
	XEvent ev;

	while (XPending(dpy)) {
		XNextEvent(dpy, &ev);
		switch (ev.type) {
		case ButtonPress:
			handle_buttonpress(&ev.xbutton);
			break;
		case Expose:
			if (ev.xexpose.count == 0) {
				/* XRaiseWindow(dpy, win); */
				draw_bar(0, 0);
			}
			break;
		default:
			break;
		}
	}
}

static void update_geometry()
{
	brect = rect;
	brect.height = font->ascent + font->descent + 4;
	if (align == SOUTH)
		brect.y = rect.height - brect.height;
	XMoveResizeWindow(dpy, win, brect.x, brect.y, brect.width, brect.height);
	XSync(dpy, False);
	XFreePixmap(dpy, pmap);
	pmap = XCreatePixmap(dpy, win, brect.width, brect.height, DefaultDepth(dpy, screen_num));
	XSync(dpy, False);
}

static void handle_after_write(IXPServer * s, File * f)
{
	int i;
	size_t len;
	Item *item;
	char buf[512];

	buf[0] = 0;
	if (!strncmp(f->name, "data", 5)) {
		if ((item = get_item_for_file(f->parent))) {
			wmii_get_ixppath(f->parent, buf, sizeof(buf));
			init_draw_label(buf, &item->d);
			draw();
		}
	} else if (file[B_GEOMETRY] == f) {
		if (f->content) {
			if (!strncmp(f->content, "south", 6))
				align = SOUTH;
			else if(!strncmp(f->content, "north", 6))
				align = NORTH;
			update_geometry();
			draw_bar(0, 0);
		}
	} else if (file[B_CTL] == f) {
		for (i = 0; acttbl[i].name; i++) {
			len = strlen(acttbl[i].name);
			if (!strncmp(acttbl[i].name, (char *) f->content, len)) {
				if (strlen(f->content) > len) {
					acttbl[i].func(0, &((char *) f->content)[len + 1]);
				} else {
					acttbl[i].func(0, 0);
				}
				break;
			}
		}
	} else if (file[B_FONT] == f) {
		XFreeFont(dpy, font);
		font = blitz_getfont(dpy, file[B_FONT]->content);
		update_geometry();
		draw_bar(0, 0);
	}
	check_event(0);
}

static void handle_before_read(IXPServer * s, File * f)
{
	char buf[64];
	if (f == file[B_GEOMETRY]) {
		if (f->content)
			free(f->content);
		if (align == SOUTH)
			f->content = strdup("south");
		else
			f->content = strdup("north");
		f->size = strlen(f->content);
	} else if (f == file[B_NEW]) {
		snprintf(buf, sizeof(buf), "%d", id++);
		if (f->content)
			free(f->content);
		f->content = strdup(buf);
		f->size = strlen(buf);
		create_label(buf);
		draw_bar(0, 0);
	}
}

static int dummy_error_handler(Display * dpy, XErrorEvent * err)
{
	return 0;
}

int main(int argc, char *argv[])
{
	int i;
	XSetWindowAttributes wa;
	XGCValues gcv;

	/* command line args */
	for (i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
		switch (argv[i][1]) {
		case 'v':
			fprintf(stdout, "%s", version[0]);
			exit(0);
			break;
		case 's':
			if (i + 1 < argc)
				sockfile = argv[++i];
			else
				usage();
			break;
		default:
			usage();
			break;
		}
	}

	dpy = XOpenDisplay(0);
	if (!dpy) {
		fprintf(stderr, "%s", "wmiibar: cannot open display\n");
		exit(1);
	}
	XSetErrorHandler(dummy_error_handler);
	screen_num = DefaultScreen(dpy);

	ixps = wmii_setup_server(sockfile);


	/* init */
	if (!(file[B_CTL] = ixp_create(ixps, "/ctl"))) {
		perror("wmiibar: cannot connect IXP server");
		exit(1);
	}
	file[B_CTL]->after_write = handle_after_write;
	file[B_NEW] = ixp_create(ixps, "/new");
	file[B_NEW]->before_read = handle_before_read;
	file[B_FONT] = wmii_create_ixpfile(ixps, "/font", BLITZ_FONT);
	file[B_FONT]->after_write = handle_after_write;
	font = blitz_getfont(dpy, file[B_FONT]->content);
	file[B_BG_COLOR] = wmii_create_ixpfile(ixps, "/bgcolor", BLITZ_NORM_BG_COLOR);
	file[B_FG_COLOR] = wmii_create_ixpfile(ixps, "/fgcolor", BLITZ_NORM_FG_COLOR);
	file[B_BORDER_COLOR] = wmii_create_ixpfile(ixps, "/bordercolor", BLITZ_NORM_BORDER_COLOR);
	file[B_GEOMETRY] = ixp_create(ixps, "/geometry");
	file[B_GEOMETRY]->before_read = handle_before_read;
	file[B_GEOMETRY]->after_write = handle_after_write;
	file[B_EXPANDABLE] = ixp_create(ixps, "/expandable");

	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | ButtonPressMask | SubstructureRedirectMask | SubstructureNotifyMask;

	rect.x = rect.y = 0;
	rect.width = DisplayWidth(dpy, screen_num);
	rect.height = DisplayHeight(dpy, screen_num);
	brect = rect;
	brect.height = font->ascent + font->descent + 4;
	brect.y = rect.height - brect.height;

	win = XCreateWindow(dpy, RootWindow(dpy, screen_num), brect.x, brect.y,
						brect.width, brect.height, 0, DefaultDepth(dpy, screen_num),
						CopyFromParent, DefaultVisual(dpy, screen_num),
						CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
	XDefineCursor(dpy, win, XCreateFontCursor(dpy, XC_left_ptr));
	XSync(dpy, False);

	gcv.function = GXcopy;
	gcv.graphics_exposures = False;
	gc = XCreateGC(dpy, win, 0, 0);

	pmap = XCreatePixmap(dpy, win, brect.width, brect.height, DefaultDepth(dpy, screen_num));

	/* main event loop */
	run_server_with_fd_support(ixps, ConnectionNumber(dpy), check_event, 0);
	deinit_server(ixps);
	XFreePixmap(dpy, pmap);
	XFreeGC(dpy, gc);
	XCloseDisplay(dpy);

	return 0;
}
