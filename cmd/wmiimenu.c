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

#include "blitz.h"

static Bool done = False;
static int ret = 0;
static char text[4096];
static Color selcolor;
static Color normcolor;

static Display *dpy;
static Window win;
static XRectangle mrect;
static int screen;
static char **allitem = nil;
static size_t nallitem = 0;
static size_t allitemsz = 0;
static char **item = nil;
static size_t itemsz = 0;
static size_t nitem = 0;
static int sel = -1;
static size_t nextoff = 0;
static size_t prevoff = 0;
static size_t curroff = 0;
static unsigned int cmdw = 0;
static Draw draw = { 0 };
static const int seek = 30;     /* 30px */

static void check_event(void);
static void draw_menu(void);
static void handle_kpress(XKeyEvent * e);

static char version[] = "wmiimenu - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";

static void
usage()
{
    fprintf(stderr, "%s", "usage: wmiimenu [-v]\n");
    exit(1);
}

static void
update_offsets()
{
	size_t i;
    unsigned int w = cmdw + 2 * seek;

    if(!nitem)
        return;

    for(i = curroff; i < nitem; i++) {
        w += XTextWidth(draw.font, item[i], strlen(item[i])) + mrect.height;
        if(w > mrect.width)
            break;
    }
	nextoff = i;

    w = cmdw + 2 * seek;
    for(i = curroff; i > 0; i--) {
        w += XTextWidth(draw.font, item[i], strlen(item[i])) + mrect.height;
        if(w > mrect.width)
            break;
    }
	prevoff = i;
}

static size_t 
update_items(char *pattern)
{
    size_t plen = strlen(pattern);
    int i;

	curroff = prevoff = nextoff = 0;
	sel = -1;

	for(i = 0; i < nitem; i++)
		item[i] = nil; /* faster than cext_array_detach */
	nitem = 0;

    for(i = 0; i < nallitem; i++) {
        if(!plen || !strncmp(pattern, allitem[i], plen))
		{
			item = (char **)cext_array_attach((void **)item, allitem[i],
											sizeof(char *), &itemsz);
            nitem++;
		}
    }
    for(i = 0; i < nallitem; i++) {
        if(plen && strncmp(pattern, allitem[i], plen)
			&& strstr(allitem[i], pattern))
		{
			item = (char **)cext_array_attach((void **)item, allitem[i],
											sizeof(char *), &itemsz);
            nitem++;
		}
    }
	if(nitem)
		sel = 0;
    
    update_offsets();
	return nitem;
}

/* creates draw structs for menu mode drawing */
static void
draw_menu()
{
    unsigned int i, offx = 0;

    draw.rect = mrect;
    draw.rect.x = 0;
    draw.rect.y = 0;
	draw.color = normcolor;
    blitz_drawlabelnoborder(dpy, &draw);

    /* print command */
    draw.align = WEST;
    draw.data = text;
    if(cmdw && nitem)
        draw.rect.width = cmdw;
    offx += draw.rect.width;
    blitz_drawlabelnoborder(dpy, &draw);

    draw.align = CENTER;
    if(nitem) {
        draw.color = normcolor;
        draw.data = prevoff < curroff ? "<" : nil;
        draw.rect.x = offx;
        draw.rect.width = seek;
        offx += draw.rect.width;
        blitz_drawlabelnoborder(dpy, &draw);

        /* determine maximum items */
        for(i = curroff; i < nextoff; i++) {
            draw.data = item[i];
            draw.rect.x = offx;
            draw.rect.width =
                XTextWidth(draw.font, draw.data, strlen(draw.data)) + mrect.height;
            offx += draw.rect.width;
            if(sel == i) {
				draw.color = selcolor;
                blitz_drawlabel(dpy, &draw);
            } else {
				draw.color = normcolor;
                blitz_drawlabelnoborder(dpy, &draw);
            }
        }

        draw.color = normcolor;
        draw.data = nitem > nextoff ? ">" : nil;
        draw.rect.x = mrect.width - seek;
        draw.rect.width = seek;
        blitz_drawlabelnoborder(dpy, &draw);
    }
    XCopyArea(dpy, draw.drawable, win, draw.gc, 0, 0, mrect.width, mrect.height, 0, 0);
    XSync(dpy, False);
}

static void
handle_kpress(XKeyEvent * e)
{
    KeySym ksym;
    char buf[32];
    int num;
    size_t len = strlen(text);

    buf[0] = 0;
    num = XLookupString(e, buf, sizeof(buf), &ksym, 0);

    if(IsFunctionKey(ksym) || IsKeypadKey(ksym)
       || IsMiscFunctionKey(ksym) || IsPFKey(ksym)
       || IsPrivateKeypadKey(ksym))
        return;

    /* first check if a control mask is omitted */
    if(e->state & ShiftMask) {
        if(ksym == XK_ISO_Left_Tab)
            ksym = XK_Left;
    } else if(e->state & ControlMask) {
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
            text[0] = 0;
            update_items(0);
            draw_menu();
            return;
            break;
        default:               /* ignore other control sequences */
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
        if(!nitem)
            return;
        cext_strlcpy(text, item[sel], sizeof(text));
        update_items(text);
        break;
    case XK_Right:
        if(sel < 0 || (sel + 1 == nitem))
            return;
		sel++;
        break;
    case XK_Return:
        if(sel >= 0)
			fprintf(stdout, "%s", item[sel]);
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
            size_t i = len;
            if(i) {
                int prev_nitem;
                do
                    text[--i] = 0;
                while((prev_nitem = nitem) && i && prev_nitem == update_items(text));
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
        } else if((sel == nextoff) && (nitem > nextoff)) {
			curroff = nextoff;
            update_offsets();
		}
    }
    draw_menu();
}

static void
check_event()
{
    XEvent ev;

    while(XPending(dpy)) {
        XNextEvent(dpy, &ev);
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
    }
}

void
read_allitems()
{
    char *maxname = 0, *p, buf[1024];
	size_t len = 0, max = 0;

	while(fgets(buf, sizeof(buf), stdin)) {
		len = strlen(buf);
		buf[len - 1] = 0; /* removing \n */
		p = strdup(buf);
        if(max < len) {
			maxname = p;
            max = len;
        }
		allitem = (char **)cext_array_attach((void **)allitem, p,
					sizeof(char *), &allitemsz);
		nallitem++;
	}

    if(maxname)
        cmdw = XTextWidth(draw.font, maxname, max) + mrect.height;
}
								 
int
main(int argc, char *argv[])
{
    int i;
    XSetWindowAttributes wa;
	char *fontstr, *selcolstr, *normcolstr;

    /* command line args */
    for(i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
        switch (argv[i][1]) {
        case 'v':
            fprintf(stdout, "%s", version);
            exit(0);
            break;
        default:
            usage();
            break;
        }
    }

    dpy = XOpenDisplay(0);
    if(!dpy) {
        fprintf(stderr, "%s", "wmiimenu: cannot open display\n");
        exit(1);
    }
    screen = DefaultScreen(dpy);

    /* set font and colors */
    fontstr = getenv("WMII_FONT");
    if (!fontstr)
	    fontstr = strdup(BLITZ_FONT);
    draw.font = blitz_getfont(dpy, fontstr);
    normcolstr = getenv("WMII_NORMCOLORS");
    if (!normcolstr || strlen(normcolstr) != 23)
	    normcolstr = strdup(BLITZ_NORMCOLORS);
	blitz_loadcolor(dpy, screen, normcolstr, &normcolor);
    selcolstr = getenv("WMII_SELCOLORS");
    if (!selcolstr || strlen(selcolstr) != 23)
	    selcolstr = strdup(BLITZ_SELCOLORS);
	blitz_loadcolor(dpy, screen, selcolstr, &selcolor);

    wa.override_redirect = 1;
    wa.background_pixmap = ParentRelative;
    wa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask
        | SubstructureRedirectMask | SubstructureNotifyMask;

    mrect.width = DisplayWidth(dpy, screen);
    mrect.height = draw.font->ascent + draw.font->descent + 4;
    mrect.y = DisplayHeight(dpy, screen) - mrect.height;
	mrect.x = 0;

    win = XCreateWindow(dpy, RootWindow(dpy, screen), mrect.x, mrect.y,
                        mrect.width, mrect.height, 0, DefaultDepth(dpy, screen),
                        CopyFromParent, DefaultVisual(dpy, screen),
                        CWOverrideRedirect | CWBackPixmap | CWEventMask,
                        &wa);
    XDefineCursor(dpy, win, XCreateFontCursor(dpy, XC_xterm));
    XSync(dpy, False);

    /* pixmap */
    draw.gc = XCreateGC(dpy, win, 0, 0);
    draw.drawable = XCreatePixmap(dpy, win, mrect.width, mrect.height,
                   	   DefaultDepth(dpy, screen));

    XSync(dpy, False);
    read_allitems();

    text[0] = 0;
    XMapRaised(dpy, win);
    XSync(dpy, False);
    update_items(text);
    draw_menu();

    while(XGrabKeyboard
          (dpy, RootWindow(dpy, screen), True, GrabModeAsync,
           GrabModeAsync, CurrentTime) != GrabSuccess)
    	usleep(1000);

    /* main event loop */
    while (!done) {
	    check_event();
	    usleep(1000);
    }

    XFreePixmap(dpy, draw.drawable);
    XFreeGC(dpy, draw.gc);
	XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);

    return ret;
}
