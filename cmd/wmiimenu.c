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
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "blitz.h"


enum {
    OFF_NEXT, OFF_PREV, OFF_CURR, OFF_LAST
};

typedef struct Item Item;
struct Item {
    char *name;
    Item *next;
    Item *prev;
};

static char *command = 0;
static Item *allitems = nil;
static char *fontstr = 0;
static char *normcolstr = 0;
static char *selcolstr = 0;
static int done = 0;
static int retvalue = 0;

static Display *dpy;
static GC gc;
static Window win;
static XRectangle rect;
static XRectangle mrect;
static int screen;
static size_t nitems = 0;
static Item *sel = nil;
static Item *items = nil;
static Item *offset[OFF_LAST];
static unsigned int cmdw = 0;
static Pixmap pmap;
static const int seek = 30;     /* 30px */
static XFontStruct *font;

static void check_event(void);
static void draw_menu(void);
static void handle_kpress(XKeyEvent * e);
static void set_text(char *text);
static size_t update_items(char *prefix);

static char version[] = "wmiimenu - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";

static void
usage()
{
    fprintf(stderr, "%s", "usage: wmiimenu [-v]\n");
    exit(1);
}

static void
return_item(char *cmd)
{
    char *rc = cmd;

    if(!cmd || cmd[0] == 0)
        return;

    if(sel && strlen(sel->name))
        rc = cmd = sel->name;

    fprintf(stdout, "%s", rc);
    done = 1;
}

static void
show()
{
    set_text(0);
    XMapRaised(dpy, win);
    XSync(dpy, False);
    update_items(command);
    draw_menu();
    while(XGrabKeyboard
          (dpy, RootWindow(dpy, screen), True, GrabModeAsync,
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

void
set_text(char *text)
{
    if(command) {
        free(command);
        command = 0;
    }
    if(text && strlen(text)) {
        command = strdup(text);
    }
}

static void
update_offsets()
{
    Item *i;
    unsigned int w = cmdw + 2 * seek;

    if(!items)
        return;

    /* calc next offset */
    for(i = offset[OFF_CURR]; i; i = i->next) {
        w += XTextWidth(font, i->name,
                        strlen(i->name)) + mrect.height;
        if(w > mrect.width)
            break;
    }
    offset[OFF_NEXT] = i;

    w = cmdw + 2 * seek;
    for(i = offset[OFF_CURR]->prev; i && i->prev; i = i->prev) {
        w += XTextWidth(font, i->name,
                        strlen(i->name)) + mrect.height;
        if(w > mrect.width)
            break;
    }
    offset[OFF_PREV] = i;
}

static size_t
update_items(char *pattern)
{
    size_t plen = pattern ? strlen(pattern) : 0, len, max = 0;
    int matched = pattern ? plen == 0 : 1;
    Item *i, *new, *p, *maxitem = 0;

    cmdw = 0;
    offset[OFF_CURR] = offset[OFF_PREV] = offset[OFF_NEXT] = nil;

    while((i = items)) {
        items = items->next;
        free(i);
    }
    sel = items = nil;
    nitems = 0;

    /* build new items */
    for(p = allitems; p; p = p->next) {
        len = strlen(p->name);
        if(max < len) {
            maxitem = p;
            max = len;
        }
    }

    if(maxitem)
        cmdw = XTextWidth(font, maxitem->name, max) + mrect.height;

    for(p = allitems; p; p = p->next) {
        if(matched || !strncmp(pattern, p->name, plen)) {
            new = cext_emallocz(sizeof(Item));
            new->name = strdup(p->name);
            new->next=0;
            new->prev=0;
            nitems++;
            if(!items)
                offset[OFF_CURR] = sel = items = i = new;
	    else {
                i->next = new;
                new->prev = i;
                i = new;
            }
        }
    }
    
    for(p = allitems; p; p = p->next) {
        if(pattern && strstr(p->name, pattern)) {
            new = cext_emallocz(sizeof(Item));
            new->name = strdup(p->name);
            new->next=0;
            new->prev=0;
            nitems++;
            if(!items)
                offset[OFF_CURR] = sel = items = i = new;
            else {
                if(strncmp(pattern, p->name, plen)) { /* yuck... */
                    i->next = new;
                    new->prev = i;
                    i = new;
                }
            }
        }
    }

    update_offsets();
    return nitems;
}

/* creates draw structs for menu mode drawing */
static void
draw_menu()
{
    Draw d = { 0 };
    unsigned int offx = 0;
    Item *i = sel;

    d.gc = gc;
    d.font = font;
    d.drawable = pmap;
    d.rect = mrect;
    d.rect.x = 0;
    d.rect.y = 0;
    blitz_loadcolor(dpy, screen, normcolstr, &(d.color));
    blitz_drawlabelnoborder(dpy, &d);

    /* print command */
    d.align = WEST;
    d.font = font;
    d.data = command;
    if(cmdw && sel)
        d.rect.width = cmdw;
    offx += d.rect.width;
    blitz_drawlabelnoborder(dpy, &d);

    d.align = CENTER;
    if(sel) {
        blitz_loadcolor(dpy, screen, normcolstr, &(d.color));
        d.data = offset[OFF_PREV] ? "<" : nil;
        d.rect.x = offx;
        d.rect.width = seek;
        offx += d.rect.width;
        blitz_drawlabelnoborder(dpy, &d);

        /* determine maximum items */
        for(i = offset[OFF_CURR]; i && i != offset[OFF_NEXT]; i = i->next) {
            d.data = i->name;
            d.rect.x = offx;
            d.rect.width =
                XTextWidth(d.font, d.data, strlen(d.data)) + mrect.height;
            offx += d.rect.width;
            if(sel == i) {
                blitz_loadcolor(dpy, screen, selcolstr, &(d.color));
                blitz_drawlabel(dpy, &d);
            } else {
                blitz_loadcolor(dpy, screen, normcolstr, &(d.color));
                blitz_drawlabelnoborder(dpy, &d);
            }
        }

        blitz_loadcolor(dpy, screen, normcolstr, &(d.color));
        d.data = offset[OFF_NEXT] ? ">" : nil;
        d.rect.x = mrect.width - seek;
        d.rect.width = seek;
        blitz_drawlabelnoborder(dpy, &d);
    }
    XCopyArea(dpy, pmap, win, gc, 0, 0, mrect.width, mrect.height, 0, 0);
    XSync(dpy, False);
}

static void
handle_kpress(XKeyEvent * e)
{
    KeySym ksym;
    char buf[32];
    int num;
    static char text[4096];
    size_t len = 0;

    text[0] = 0;
    if(command) {
        cext_strlcpy(text, command, sizeof(text));
        len = strlen(text);
    }
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
            set_text(0);
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
        if(!sel)
            return;
        if(sel->prev) {
            sel = sel->prev;
        } else
            return;
        break;
    case XK_Tab:
        if(!sel)
            return;
        set_text(sel->name);
        update_items(command);
        break;
    case XK_Right:
        if(!sel)
            return;
        if(sel->next) {
            sel = sel->next;
        } else
            return;
        break;
    case XK_Return:
        if(sel)
            return_item(sel->name);
        else if(text)
            return_item(text);
	break;
    case XK_Escape:
	retvalue = 1;
	done = 1;
        break;
    case XK_BackSpace:
        if(len) {
            size_t i = len;
            if(i) {
                int prev_nitems;
                do
                    text[--i] = 0;
                while((prev_nitems = nitems) && i && prev_nitems == update_items(text));
            }
            set_text(text);
            update_items(command);
        }
        break;
    default:
        if((num == 1) && !iscntrl((int) buf[0])) {
            buf[num] = 0;
            if(len > 0)
                cext_strlcat(text, buf, sizeof(text));
            else
                cext_strlcpy(text, buf, sizeof(text));
            set_text(text);
            update_items(command);
        }
    }
    if(sel) {
        if(sel == offset[OFF_CURR]->prev) {
            offset[OFF_CURR] = offset[OFF_PREV];
            update_offsets();
        } else if(sel == offset[OFF_NEXT]) {
            offset[OFF_CURR] = offset[OFF_NEXT];
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

static void
update_geometry()
{
    mrect = rect;
    mrect.height = font->ascent + font->descent + 4;
    mrect.y = rect.height - mrect.height;
    XMoveResizeWindow(dpy, win, mrect.x, mrect.y, mrect.width,
                      mrect.height);
    XSync(dpy, False);
    XFreePixmap(dpy, pmap);
    pmap =
        XCreatePixmap(dpy, win, mrect.width, mrect.height,
                      DefaultDepth(dpy, screen));
    XSync(dpy, False);
}

void
init_allitems()
{
    char text[4096];
    allitems = cext_emallocz(sizeof(Item));
    allitems->prev = 0;
    Item *curitem = allitems;
    char ch;
    int i = 0;
    while ((ch = (char)fgetc(stdin)) != EOF) {
        if (!iscntrl(ch) && i<4095) {
		text[i]=ch;
                i++;
	} else {
            text[i] = 0;
            if (strlen(text)) {
                curitem->name = strdup(text);
                curitem->next = cext_emallocz(sizeof(Item));
                curitem->next->prev = curitem;
                curitem = curitem->next;
	    }
	    i=0;
        }
    }
    if (curitem == allitems)
        allitems = 0;
    if (curitem->prev)
        curitem->prev->next = 0;
    free(curitem);
}
								 
int
main(int argc, char *argv[])
{
    int i;
    XSetWindowAttributes wa;
    XGCValues gcv;

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
    font = blitz_getfont(dpy, fontstr);
    normcolstr = getenv("WMII_NORMCOLORS");
    if (!normcolstr || strlen(normcolstr) != 23)
	    normcolstr = strdup(BLITZ_NORMCOLORS);
    selcolstr = getenv("WMII_SELCOLORS");
    if (!selcolstr || strlen(selcolstr) != 23)
	    selcolstr = strdup(BLITZ_SELCOLORS);

    /* init */
    wa.override_redirect = 1;
    wa.background_pixmap = ParentRelative;
    wa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask
        | SubstructureRedirectMask | SubstructureNotifyMask;

    rect.x = rect.y = 0;
    rect.width = DisplayWidth(dpy, screen);
    rect.height = DisplayHeight(dpy, screen);
    mrect = rect;
    mrect.height = font->ascent + font->descent + 4;
    mrect.y = rect.height - mrect.height;

    win = XCreateWindow(dpy, RootWindow(dpy, screen), mrect.x, mrect.y,
                        mrect.width, mrect.height, 0, DefaultDepth(dpy,
                                                                   screen),
                        CopyFromParent, DefaultVisual(dpy, screen),
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
                      DefaultDepth(dpy, screen));

    /* initialize some more stuff */
    update_geometry();
    init_allitems();
    if(!allitems) {
        fprintf(stderr, "%s", "wmiimenu: feed me newline-separated items on stdin please.\n");
        exit(1);
    }

    /* main event loop */
    show();
    while (!done) {
	    check_event();
	    usleep(1000);
    }
    hide();

    XFreePixmap(dpy, pmap);
    XFreeGC(dpy, gc);
    XCloseDisplay(dpy);

    return retvalue;
}
