/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "wmii.h"

#include <cext.h>

/* array indexes for file pointers */
typedef enum {
	M_CTL,
	M_SIZE,
	M_PRE_COMMAND,
	M_COMMAND,
	M_HISTORY,
	M_LOOKUP,
	M_TEXT_FONT,
	M_SELECTED_BG_COLOR,
	M_SELECTED_TEXT_COLOR,
	M_SELECTED_BORDER_COLOR,
	M_NORMAL_BG_COLOR,
	M_NORMAL_TEXT_COLOR,
	M_NORMAL_BORDER_COLOR,
	M_RETARDED,
	M_LAST
}               InputIndexes;

enum {
	OFF_NEXT, OFF_PREV, OFF_CURR, OFF_LAST
};

static IXPServer *ixps = 0;
static Display *dpy;
static GC       gc;
static Window   win;
static XRectangle rect;
static XRectangle mrect;
static int      screen_num;
static char    *sockfile = 0;
static File    *files[M_LAST];
static File   **items = 0;
static size_t   item_size = 0;
static int      item = 0;
static int      offset[OFF_LAST];
static unsigned int cmdw = 0;
static File   **history = 0;
static int      sel_history = 0;
static Pixmap   pmap;
static const int seek = 30;	/* 30px */

static void     check_event(Connection * c);
static void     draw_menu(void);
static void     handle_kpress(XKeyEvent * e);
static void     set_text(char *text);
static void     quit(void *obj, char *arg);
static void     display(void *obj, char *arg);
static int      update_items(char *prefix);

static Action   acttbl[2] = {
	{"quit", quit},
	{"display", display}
};

static char    *version[] = {
	"wmimenu - window manager improved menu - " VERSION "\n"
	" (C)opyright MMIV-MMV Anselm R. Garbe\n", 0
};

static void
usage()
{
	fprintf(stderr, "%s",
		"usage: wmimenu [-s <socket file>] [-r] [-v] [<x>,<y>,<width>,<height>]\n"
		"      -s      socket file (default: /tmp/.ixp-$USER/wmimenu-%s-%s)\n"
		"      -v      version info\n");
	exit(1);
}

static void
add_history(char *cmd)
{
	char            buf[MAX_BUF];
	snprintf(buf, MAX_BUF, "/history/%ld", (long) time(0));
	history = (File **) attach_item_begin((void **) history,
					      wmii_create_ixpfile(ixps, buf,
								  cmd),
					      sizeof(File *));
}

static void
_exec(char *cmd)
{
	char           *rc = cmd;

	if (!cmd || cmd[0] == '\0')
		return;

	if (items && items[0]) {
		if ((item >= 0) && items[item] && items[item]->size)
			rc = cmd = items[item]->content;
		else if ((item == -1) && items[0]->size)	/* autolight */
			rc = cmd = items[0]->content;
	}
	add_history(cmd);
	if (files[M_PRE_COMMAND]->content) {
		size_t          len = strlen(cmd) + files[M_PRE_COMMAND]->size + 2;
		rc = emalloc(len);
		snprintf(rc, len, "%s %s", (char *) files[M_PRE_COMMAND]->content,
			 cmd);
	}
	/* fallback */
	spawn(dpy, rc);
	/* cleanup */
	if (files[M_PRE_COMMAND]->content)
		free(rc);
}

static void
quit(void *obj, char *arg)
{
	ixps->runlevel = SHUTDOWN;
}

static void
show()
{
	set_text(0);
	XMapRaised(dpy, win);
	XSync(dpy, False);
	update_items(files[M_COMMAND]->content);
	draw_menu();
	while (XGrabKeyboard
	       (dpy, RootWindow(dpy, screen_num), True, GrabModeAsync,
		GrabModeAsync, CurrentTime) != GrabSuccess)
		usleep(1000);
}

static void
hide()
{
	XUngrabKeyboard(dpy, CurrentTime);
	XUnmapWindow(dpy, win);
	XSync(dpy, False);
}

static void
display(void *obj, char *arg)
{
	if (!arg)
		return;
	if (_strtonum(arg, 0, 1))
		show();
	else
		hide();
	check_event(0);
}

void
set_text(char *text)
{
	if (files[M_COMMAND]->content) {
		free(files[M_COMMAND]->content);
		files[M_COMMAND]->content = 0;
		files[M_COMMAND]->size = 0;
	}
	if (text && strlen(text)) {
		files[M_COMMAND]->content = strdup(text);
		files[M_COMMAND]->size = strlen(text);
	}
}

static void
update_offsets()
{
	int             i;
	XFontStruct    *font;
	unsigned int    w = cmdw + 2 * seek;

	if (!items)
		return;

	font = blitz_getfont(dpy, files[M_TEXT_FONT]->content);

	/* calc next offset */
	for (i = offset[OFF_CURR]; items[i]; i++) {
		w += XTextWidth(font, items[i]->content, strlen(items[i]->content)) + mrect.height;
		if (w > mrect.width)
			break;
	}
	offset[OFF_NEXT] = i;


	w = cmdw + 2 * seek;
	for (i = offset[OFF_CURR] - 1; i >= 0; i--) {
		w += XTextWidth(font, items[i]->content, strlen(items[i]->content)) + mrect.height;
		if (w > mrect.width)
			break;
	}
	offset[OFF_PREV] = i + 1;
	XFreeFont(dpy, font);
}

static int
update_items(char *pattern)
{
	size_t          plen = pattern ? strlen(pattern) : 0, size = 0, max = 0,
	                len;
	int             matched = pattern ? plen == 0 : 1;
	XFontStruct    *font;
	File           *f, *p, *maxitem = 0;

	cmdw = 0;
	item = -1;
	offset[OFF_CURR] = offset[OFF_PREV] = offset[OFF_NEXT] = 0;

	if (!files[M_LOOKUP]->content)
		return 0;
	f = ixp_walk(ixps, files[M_LOOKUP]->content);
	if (!f || !is_directory(f))
		return 0;

	font = blitz_getfont(dpy, files[M_TEXT_FONT]->content);

	/* build new items */
	for (p = f->content; p; p = p->next) {
		size++;
		len = strlen(p->name);
		if (max < len) {
			maxitem = p;
			max = len;
		}
	}

	if (maxitem) {
		if (files[M_RETARDED]->content)
			free(files[M_RETARDED]->content);
		files[M_RETARDED]->content = strdup(maxitem->name);
		files[M_RETARDED]->size = max;
		cmdw = XTextWidth(font, maxitem->name, max) + mrect.height;
	}
	if (size > item_size) {
		/* stores always the biggest amount of items in memory */
		if (items)
			free((File **) items);
		items = 0;
		item_size = size;
		if (item_size)
			items = (File **) emalloc((item_size + 1) * sizeof(File *));
	}
	size = 0;

	for (p = f->content; p; p = p->next) {
		if (!p->content)
			continue;	/* ignore bogus files */
		if (matched || !strncmp(pattern, p->name, plen)) {
			items[size++] = p;
			p->parent = 0;	/* HACK to prevent doubled items */
		}
	}

	for (p = f->content; p; p = p->next) {
		if (!p->content)
			continue;	/* ignore bogus files */
		if (p->parent && strstr(p->name, pattern))
			items[size++] = p;
		else
			p->parent = f;	/* restore HACK */
	}
	items[size] = 0;
	update_offsets();
	XFreeFont(dpy, font);
	return size;
}

/* creates draw structs for menu mode drawing */
static void
draw_menu()
{
	Draw            d = {0};
	unsigned int    offx = 0;
	int             i = 0;
	XFontStruct    *font = blitz_getfont(dpy, files[M_TEXT_FONT]->content);

	d.gc = gc;
	d.drawable = pmap;
	d.rect = mrect;
	d.rect.x = 0;
	d.rect.y = 0;
	d.bg = blitz_loadcolor(dpy, screen_num, files[M_NORMAL_BG_COLOR]->content);
	d.border = blitz_loadcolor(dpy, screen_num, files[M_NORMAL_BORDER_COLOR]->content);
	blitz_drawlabelnoborder(dpy, &d);

	/* print command */
	d.align = WEST;
	d.font = font;
	d.fg = blitz_loadcolor(dpy, screen_num, files[M_NORMAL_TEXT_COLOR]->content);
	d.data = files[M_COMMAND]->content;
	if (cmdw && items && items[0])
		d.rect.width = cmdw;
	offx += d.rect.width;
	blitz_drawlabelnoborder(dpy, &d);

	d.align = CENTER;
	if (items && items[0]) {
		d.bg = blitz_loadcolor(dpy, screen_num, files[M_NORMAL_BG_COLOR]->content);
		d.fg = blitz_loadcolor(dpy, screen_num, files[M_NORMAL_TEXT_COLOR]->content);
		d.data = offset[OFF_CURR] ? "<" : 0;
		d.rect.x = offx;
		d.rect.width = seek;
		offx += d.rect.width;
		blitz_drawlabelnoborder(dpy, &d);

		/* determine maximum items */
		for (i = offset[OFF_CURR]; items[i] && (i < offset[OFF_NEXT]); i++) {
			d.data = items[i]->name;
			d.rect.x = offx;
			d.rect.width = XTextWidth(d.font, d.data, strlen(d.data)) + mrect.height;
			offx += d.rect.width;
			if (i == item) {
				d.bg = blitz_loadcolor(dpy, screen_num, files[M_SELECTED_BG_COLOR]->content);
				d.fg = blitz_loadcolor(dpy, screen_num, files[M_SELECTED_TEXT_COLOR]->content);
				d.border = blitz_loadcolor(dpy, screen_num, files[M_SELECTED_BORDER_COLOR]->content);
				blitz_drawlabel(dpy, &d);
			} else if (!i && item == -1) {
				/* fg and bg are inverted */
				d.fg = blitz_loadcolor(dpy, screen_num, files[M_SELECTED_BG_COLOR]->content);
				d.bg = blitz_loadcolor(dpy, screen_num, files[M_SELECTED_TEXT_COLOR]->content);
				d.border = blitz_loadcolor(dpy, screen_num, files[M_SELECTED_BORDER_COLOR]->content);
				blitz_drawlabel(dpy, &d);
			} else {
				d.bg = blitz_loadcolor(dpy, screen_num, files[M_NORMAL_BG_COLOR]->content);
				d.fg = blitz_loadcolor(dpy, screen_num, files[M_NORMAL_TEXT_COLOR]->content);
				d.border = blitz_loadcolor(dpy, screen_num, files[M_NORMAL_BORDER_COLOR]->content);
				blitz_drawlabelnoborder(dpy, &d);
			}
		}

		d.bg = blitz_loadcolor(dpy, screen_num, files[M_NORMAL_BG_COLOR]->content);
		d.fg = blitz_loadcolor(dpy, screen_num, files[M_NORMAL_TEXT_COLOR]->content);
		d.data = items[i] ? ">" : 0;
		d.rect.x = mrect.width - seek;
		d.rect.width = seek;
		blitz_drawlabelnoborder(dpy, &d);
	}
	XCopyArea(dpy, pmap, win, gc, 0, 0, mrect.width, mrect.height, 0, 0);
	XSync(dpy, False);
	XFreeFont(dpy, font);
}

static void
handle_kpress(XKeyEvent * e)
{
	KeySym          ksym;
	char            buf[32];
	int             idx, num;
	static char     text[4096];
	size_t          len = 0;

	text[0] = '\0';
	if (files[M_COMMAND]->content) {
		_strlcpy(text, files[M_COMMAND]->content, sizeof(text));
		len = strlen(text);
	}
	buf[0] = '\0';
	num = XLookupString(e, buf, sizeof(buf), &ksym, 0);

	if (IsFunctionKey(ksym) || IsKeypadKey(ksym)
	    || IsMiscFunctionKey(ksym) || IsPFKey(ksym)
	    || IsPrivateKeypadKey(ksym))
		return;

	/* first check if a control mask is omitted */
	if (e->state & ShiftMask) {
		if (ksym == XK_ISO_Left_Tab)
			ksym = XK_Left;
	} else if (e->state & ControlMask) {
		switch (ksym) {
		case XK_E:
		case XK_e:
			ksym = XK_End;
			break;
		case XK_H:
		case XK_h:
			ksym = XK_BackSpace;
			break;
		case XK_J:
		case XK_j:
			ksym = XK_Return;
			break;
		case XK_U:
		case XK_u:
			set_text(0);
			update_items(0);
			draw_menu();
			return;
			break;
		default:	/* ignore other control sequences */
			return;
			break;
		}
	}
	switch (ksym) {
	case XK_Left:
		if (!items || !items[0])
			return;
		if (item > 0) {
			item--;
			set_text(items[item]->name);
		} else
			return;
		break;
	case XK_Right:
	case XK_Tab:
		if (!items || !items[0])
			return;
		if (items[item + 1]) {
			item++;
			set_text(items[item]->name);
		} else
			return;
		break;
	case XK_Down:
		if (history) {
			set_text(history[sel_history]->content);
			idx = index_prev_item((void **) history, history[sel_history]);
			if (idx >= 0)
				sel_history = idx;
		}
		update_items(files[M_COMMAND]->content);
		break;
	case XK_Up:
		if (history) {
			set_text(history[sel_history]->content);
			idx = index_next_item((void **) history, history[sel_history]);
			if (idx >= 0)
				sel_history = idx;
		}
		update_items(files[M_COMMAND]->content);
		break;
	case XK_Return:
		if (items && items[0]) {
			if (item >= 0)
				_exec(items[item]->name);
			else
				_exec(items[0]->name);
		} else if (text)
			_exec(text);
	case XK_Escape:
		hide();
		break;
	case XK_BackSpace:
		if (len) {
			int             size = 0;
			size_t          i = len;
			for (size = 0; items && items[size]; size++);
			if (i) {
				do
					text[--i] = '\0';
				while (size && i && size == update_items(text));
			}
			set_text(text);
			update_items(files[M_COMMAND]->content);
		}
		break;
	default:
		if ((num == 1) && !iscntrl((int) buf[0])) {
			buf[num] = '\0';
			if (len > 0)
				_strlcat(text, buf, sizeof(text));
			else
				_strlcpy(text, buf, sizeof(text));
			set_text(text);
			update_items(files[M_COMMAND]->content);
		}
	}
	if (items && item > 0) {
		if (item < offset[OFF_CURR]) {
			offset[OFF_CURR] = offset[OFF_PREV];
			update_offsets();
		} else if (item >= offset[OFF_NEXT]) {
			offset[OFF_CURR] = offset[OFF_NEXT];
			update_offsets();
		}
	}
	draw_menu();
}

static void
check_event(Connection * c)
{
	XEvent          ev;

	while (XPending(dpy)) {
		XNextEvent(dpy, &ev);
		switch (ev.type) {
		case KeyPress:
			handle_kpress(&ev.xkey);
			break;
		case Expose:
			if (ev.xexpose.count == 0) {
				draw_menu();
			}
			break;
		default:
			break;
		}
	}
}

static void
handle_after_write(IXPServer * s, File * f)
{
	int             i;
	size_t          len;

	if (files[M_CTL] == f) {
		for (i = 0; i < 2; i++) {
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
	} else if (files[M_SIZE] == f) {
		char           *size = files[M_SIZE]->content;
		if (size && strrchr(size, ',')) {
			blitz_strtorect(dpy, &rect, &mrect, size);
			XFreePixmap(dpy, pmap);
			XMoveResizeWindow(dpy, win, mrect.x, mrect.y,
					  mrect.width, mrect.height);
			XSync(dpy, False);
			pmap = XCreatePixmap(dpy, win, mrect.width, mrect.height,
					     DefaultDepth(dpy, screen_num));
			XSync(dpy, False);
			draw_menu();
		}
	} else if (files[M_COMMAND] == f) {
		update_items(files[M_COMMAND]->content);
		draw_menu();
	}
	check_event(0);
}

static void
handle_before_read(IXPServer * s, File * f)
{
	char            buf[64];
	if (f != files[M_SIZE])
		return;
	snprintf(buf, sizeof(buf), "%d,%d,%d,%d", mrect.x, mrect.y,
		 mrect.width, mrect.height);
	if (f->content)
		free(f->content);
	f->content = strdup(buf);
	f->size = strlen(buf);
}

static void
run(char *size)
{
	XSetWindowAttributes wa;
	XGCValues       gcv;

	/* init */
	if (!(files[M_CTL] = ixp_create(ixps, "/ctl"))) {
		perror("wmimenu: cannot connect IXP server");
		exit(1);
	}
	files[M_CTL]->after_write = handle_after_write;
	files[M_SIZE] = ixp_create(ixps, "/size");
	files[M_SIZE]->before_read = handle_before_read;
	files[M_SIZE]->after_write = handle_after_write;
	files[M_PRE_COMMAND] = ixp_create(ixps, "/precmd");
	files[M_COMMAND] = ixp_create(ixps, "/cmd");
	files[M_COMMAND]->after_write = handle_after_write;
	files[M_HISTORY] = ixp_create(ixps, "/history");
	add_history("");
	files[M_LOOKUP] = ixp_create(ixps, "/lookup");
	files[M_TEXT_FONT] = wmii_create_ixpfile(ixps, "/style/text-font", BLITZ_FONT);
	files[M_SELECTED_BG_COLOR] = wmii_create_ixpfile(ixps, "/sel-style/bg-color", BLITZ_SEL_BG_COLOR);
	files[M_SELECTED_TEXT_COLOR] = wmii_create_ixpfile(ixps, "/sel-style/text-color", BLITZ_SEL_FG_COLOR);
	files[M_SELECTED_BORDER_COLOR] = wmii_create_ixpfile(ixps, "/sel-style/border-color", BLITZ_SEL_BORDER_COLOR);
	files[M_NORMAL_BG_COLOR] = wmii_create_ixpfile(ixps, "/norm-style/bg-color", BLITZ_NORM_BG_COLOR);
	files[M_NORMAL_TEXT_COLOR] = wmii_create_ixpfile(ixps, "/norm-style/text-color", BLITZ_NORM_FG_COLOR);
	files[M_NORMAL_BORDER_COLOR] = wmii_create_ixpfile(ixps, "/norm-style/border-color", BLITZ_NORM_BORDER_COLOR);
	files[M_RETARDED] = ixp_create(ixps, "/retarded");

	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask
		| SubstructureRedirectMask | SubstructureNotifyMask;

	rect.x = rect.y = 0;
	rect.width = DisplayWidth(dpy, screen_num);
	rect.height = DisplayHeight(dpy, screen_num);
	blitz_strtorect(dpy, &rect, &mrect, size);
	if (!mrect.width)
		mrect.width = DisplayWidth(dpy, screen_num);
	if (!mrect.height)
		mrect.height = 40;

	win = XCreateWindow(dpy, RootWindow(dpy, screen_num), mrect.x, mrect.y,
			    mrect.width, mrect.height, 0, DefaultDepth(dpy,
								screen_num),
			    CopyFromParent, DefaultVisual(dpy, screen_num),
			    CWOverrideRedirect | CWBackPixmap | CWEventMask,
			    &wa);
	XDefineCursor(dpy, win, XCreateFontCursor(dpy, XC_xterm));
	XSync(dpy, False);

	/* window pixmap */
	gcv.function = GXcopy;
	gcv.graphics_exposures = False;

	gc = XCreateGC(dpy, win, 0, 0);
	pmap =
		XCreatePixmap(dpy, win, mrect.width, mrect.height,
			      DefaultDepth(dpy, screen_num));

	/* main event loop */
	run_server_with_fd_support(ixps, ConnectionNumber(dpy),
				   check_event, 0);
	deinit_server(ixps);
	XFreePixmap(dpy, pmap);
	XFreeGC(dpy, gc);
	XCloseDisplay(dpy);
}

int
main(int argc, char *argv[])
{
	char            size[64];
	int             i;

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
		fprintf(stderr, "%s", "wmimenu: cannot open display\n");
		exit(1);
	}
	screen_num = DefaultScreen(dpy);

	size[0] = '\0';
	if (argc > i)
		_strlcpy(size, argv[i], sizeof(size));

	ixps = wmii_setup_server(sockfile);
	items = 0;

	run(size);

	return 0;
}
