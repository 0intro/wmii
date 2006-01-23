/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "wmii.h"

/* array indexes for file pointers */
typedef enum {
    M_CTL,
    M_GEOMETRY,
    M_PRE_COMMAND,
    M_COMMAND,
    M_HISTORY,
    M_LOOKUP,
    M_FONT,
    M_SEL_BG_COLOR,
    M_SEL_TEXT_COLOR,
    M_SEL_BORDER_COLOR,
    M_NORM_BG_COLOR,
    M_NORM_TEXT_COLOR,
    M_NORM_BORDER_COLOR,
    M_LAST
} InputIndexes;

enum {
    OFF_NEXT, OFF_PREV, OFF_CURR, OFF_LAST
};

typedef struct Item Item;
struct Item {
    File *file;
    Item *next;
    Item *prev;
};

static IXPServer *ixps = 0;
static Display *dpy;
static GC gc;
static Window win;
static XRectangle rect;
static XRectangle mrect;
static int screen_num;
static char *sockfile = 0;
static File *files[M_LAST];
static size_t nitems = 0;
static Item *sel = nil;
static Item *items = nil;
static Item *selhist = nil;
static Item *history = nil;
static Item *offset[OFF_LAST];
static unsigned int cmdw = 0;
static Pixmap pmap;
static const int seek = 30;     /* 30px */
static XFontStruct *font;
static Align align = SOUTH;

static void check_event(Connection * c);
static void draw_menu(void);
static void handle_kpress(XKeyEvent * e);
static void set_text(char *text);
static void quit(void *obj, char *arg);
static void display(void *obj, char *arg);
static size_t update_items(char *prefix);

static Action acttbl[2] = {
    {"quit", quit},
    {"display", display}
};

static char *version[] = {
    "wmiimenu - window manager improved menu - " VERSION "\n"
        " (C)opyright MMIV-MMVI Anselm R. Garbe\n", 0
};

static void
usage()
{
    fprintf(stderr, "%s",
            "usage: wmiimenu [-s <socket file>] [-r] [-v]\n"
            "      -s      socket file (default: /tmp/.ixp-$USER/wmiimenu-%s-%s)\n"
            "      -v      version info\n");
    exit(1);
}

static void
add_history(char *cmd)
{
    Item *h, *new;
    char buf[MAX_BUF];
    snprintf(buf, MAX_BUF, "/history/%ld", (long) time(0));

    new = cext_emallocz(sizeof(Item));
    new->file = wmii_create_ixpfile(ixps, buf, cmd);
    for(h = history; h && h->next; h = h->next);
    if(!h)
        history = new;
    else {
        h->next = new;
        new->prev = h;
    }
    selhist = new;
}

static void
exec_item(char *cmd)
{
    char *rc = cmd;

    if(!cmd || cmd[0] == 0)
        return;

    if(sel && sel->file->size)
        rc = cmd = sel->file->content;
    add_history(cmd);

    if(files[M_PRE_COMMAND]->content) {
        size_t len = strlen(cmd) + files[M_PRE_COMMAND]->size + 2;
        rc = cext_emallocz(len);
        snprintf(rc, len, "%s %s", (char *) files[M_PRE_COMMAND]->content,
                 cmd);
    }
    /* fallback */
    wmii_spawn(dpy, rc);
    /* cleanup */
    if(files[M_PRE_COMMAND]->content)
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
    while(XGrabKeyboard
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
    if(!arg)
        return;
    if(blitz_strtonum(arg, 0, 1))
        show();
    else
        hide();
    check_event(0);
}

void
set_text(char *text)
{
    if(files[M_COMMAND]->content) {
        free(files[M_COMMAND]->content);
        files[M_COMMAND]->content = 0;
        files[M_COMMAND]->size = 0;
    }
    if(text && strlen(text)) {
        files[M_COMMAND]->content = strdup(text);
        files[M_COMMAND]->size = strlen(text);
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
        w += XTextWidth(font, i->file->content,
                        strlen(i->file->content)) + mrect.height;
        if(w > mrect.width)
            break;
    }
    offset[OFF_NEXT] = i;

    w = cmdw + 2 * seek;
    for(i = offset[OFF_CURR]->prev; i && i->prev; i = i->prev) {
        w += XTextWidth(font, i->file->content,
                        strlen(i->file->content)) + mrect.height;
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
    File *f, *p, *maxitem;
    Item *i, *new;

    if(!files[M_LOOKUP]->content)
        return 0;
    f = ixp_walk(ixps, files[M_LOOKUP]->content);
    if(!f || !is_directory(f))
        return 0;

    cmdw = 0;
    offset[OFF_CURR] = offset[OFF_PREV] = offset[OFF_NEXT] = nil;

    while((i = items)) {
        items = items->next;
        free(i);
    }
    sel = items = nil;
    nitems = 0;

    /* build new items */
    for(p = f->content; p; p = p->next) {
        len = strlen(p->name);
        if(max < len) {
            maxitem = p;
            max = len;
        }
    }

    if(maxitem)
        cmdw = XTextWidth(font, maxitem->name, max) + mrect.height;

    for(p = f->content; p; p = p->next) {
        if(matched || !strncmp(pattern, p->name, plen)) {
            new = cext_emallocz(sizeof(Item));
            new->file = p;
            nitems++;
            if(!items)
                offset[OFF_CURR] = sel = items = i = new;
            else {
                i->next = new;
                new->prev = i;
                i = new;
            }
            p->parent = 0;      /* HACK to prevent doubled items */
        }
    }

    for(p = f->content; p; p = p->next) {
        if(p->parent && strstr(p->name, pattern)) {
            new = cext_emallocz(sizeof(Item));
            new->file = p;
            nitems++;
            if(!items)
                offset[OFF_CURR] = sel = items = i = new;
            else {
                i->next = new;
                new->prev = i;
                i = new;
            }
        } else
            p->parent = f;      /* restore HACK */
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
    d.bg =
        blitz_loadcolor(dpy, screen_num, files[M_NORM_BG_COLOR]->content);
    d.border =
        blitz_loadcolor(dpy, screen_num,
                        files[M_NORM_BORDER_COLOR]->content);
    blitz_drawlabelnoborder(dpy, &d);

    /* print command */
    d.align = WEST;
    d.font = font;
    d.fg =
        blitz_loadcolor(dpy, screen_num,
                        files[M_NORM_TEXT_COLOR]->content);
    d.data = files[M_COMMAND]->content;
    if(cmdw && sel)
        d.rect.width = cmdw;
    offx += d.rect.width;
    blitz_drawlabelnoborder(dpy, &d);

    d.align = CENTER;
    if(sel) {
        d.bg =
            blitz_loadcolor(dpy, screen_num,
                            files[M_NORM_BG_COLOR]->content);
        d.fg =
            blitz_loadcolor(dpy, screen_num,
                            files[M_NORM_TEXT_COLOR]->content);
        d.data = offset[OFF_PREV] ? "<" : nil;
        d.rect.x = offx;
        d.rect.width = seek;
        offx += d.rect.width;
        blitz_drawlabelnoborder(dpy, &d);

        /* determine maximum items */
        for(i = offset[OFF_CURR]; i && i != offset[OFF_NEXT]; i = i->next) {
            d.data = i->file->name;
            d.rect.x = offx;
            d.rect.width =
                XTextWidth(d.font, d.data, strlen(d.data)) + mrect.height;
            offx += d.rect.width;
            /*fprintf(stderr, "%s (%d, %d, %d, %d)\n", item->name, d.rect.x, d.rect.y, d.rect.width, d.rect.height); */
            if(sel == i) {
                d.bg =
                    blitz_loadcolor(dpy, screen_num,
                                    files[M_SEL_BG_COLOR]->content);
                d.fg =
                    blitz_loadcolor(dpy, screen_num,
                                    files[M_SEL_TEXT_COLOR]->content);
                d.border =
                    blitz_loadcolor(dpy, screen_num,
                                    files[M_SEL_BORDER_COLOR]->content);
                blitz_drawlabel(dpy, &d);
            } else {
                d.bg =
                    blitz_loadcolor(dpy, screen_num,
                                    files[M_NORM_BG_COLOR]->content);
                d.fg =
                    blitz_loadcolor(dpy, screen_num,
                                    files[M_NORM_TEXT_COLOR]->content);
                d.border =
                    blitz_loadcolor(dpy, screen_num,
                                    files[M_NORM_BORDER_COLOR]->content);
                blitz_drawlabelnoborder(dpy, &d);
            }
        }

        d.bg =
            blitz_loadcolor(dpy, screen_num,
                            files[M_NORM_BG_COLOR]->content);
        d.fg =
            blitz_loadcolor(dpy, screen_num,
                            files[M_NORM_TEXT_COLOR]->content);
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
    if(files[M_COMMAND]->content) {
        cext_strlcpy(text, files[M_COMMAND]->content, sizeof(text));
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
        set_text(sel->file->name);
        update_items(files[M_COMMAND]->content);
        break;
    case XK_Right:
        if(!sel)
            return;
        if(sel->next) {
            sel = sel->next;
        } else
            return;
        break;
    case XK_Down:
        if(selhist) {
            set_text(selhist->file->content);
            if(selhist->next)
                selhist = selhist->next;
        }
        update_items(files[M_COMMAND]->content);
        break;
    case XK_Up:
        if(selhist) {
            set_text(selhist->file->content);
            if(selhist->prev)
                selhist = selhist->prev;
        }
        update_items(files[M_COMMAND]->content);
        break;
    case XK_Return:
        if(sel)
            exec_item(sel->file->name);
        else if(text)
            exec_item(text);
    case XK_Escape:
        hide();
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
            update_items(files[M_COMMAND]->content);
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
            update_items(files[M_COMMAND]->content);
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
check_event(Connection * c)
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
    if(align == SOUTH)
        mrect.y = rect.height - mrect.height;
    XMoveResizeWindow(dpy, win, mrect.x, mrect.y, mrect.width,
                      mrect.height);
    XSync(dpy, False);
    XFreePixmap(dpy, pmap);
    pmap =
        XCreatePixmap(dpy, win, mrect.width, mrect.height,
                      DefaultDepth(dpy, screen_num));
    XSync(dpy, False);
}

static void
handle_after_write(IXPServer * s, File * f)
{
    int i;
    size_t len;

    if(files[M_CTL] == f) {
        for(i = 0; i < 2; i++) {
            len = strlen(acttbl[i].name);
            if(!strncmp(acttbl[i].name, (char *) f->content, len)) {
                if(strlen(f->content) > len) {
                    acttbl[i].func(0, &((char *) f->content)[len + 1]);
                } else {
                    acttbl[i].func(0, 0);
                }
                break;
            }
        }
    } else if(files[M_GEOMETRY] == f) {
        if(f->content) {
            if(!strncmp(f->content, "south", 6))
                align = SOUTH;
            else if(!strncmp(f->content, "north", 6))
                align = NORTH;
            update_geometry();
            draw_menu();
        }
    } else if(files[M_FONT] == f) {
        XFreeFont(dpy, font);
        font = blitz_getfont(dpy, files[M_FONT]->content);
        update_geometry();
        draw_menu();
    } else if(files[M_COMMAND] == f) {
        update_items(files[M_COMMAND]->content);
        draw_menu();
    }
    check_event(0);
}

static void
handle_before_read(IXPServer * s, File * f)
{
    char buf[64];
    if(f != files[M_GEOMETRY])
        return;
    snprintf(buf, sizeof(buf), "%d %d %d %d", mrect.x, mrect.y,
             mrect.width, mrect.height);
    if(f->content)
        free(f->content);
    f->content = strdup(buf);
    f->size = strlen(buf);
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
            fprintf(stdout, "%s", version[0]);
            exit(0);
            break;
        case 's':
            if(i + 1 < argc)
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
    if(!dpy) {
        fprintf(stderr, "%s", "wmiimenu: cannot open display\n");
        exit(1);
    }
    screen_num = DefaultScreen(dpy);

    ixps = wmii_setup_server(sockfile);

    /* init */
    if(!(files[M_CTL] = ixp_create(ixps, "/ctl"))) {
        perror("wmiimenu: cannot connect IXP server");
        exit(1);
    }
    files[M_CTL]->after_write = handle_after_write;
    files[M_GEOMETRY] = ixp_create(ixps, "/geometry");
    files[M_GEOMETRY]->before_read = handle_before_read;
    files[M_GEOMETRY]->after_write = handle_after_write;
    files[M_PRE_COMMAND] = ixp_create(ixps, "/precmd");
    files[M_COMMAND] = ixp_create(ixps, "/cmd");
    files[M_COMMAND]->after_write = handle_after_write;
    files[M_HISTORY] = ixp_create(ixps, "/history");
    add_history("");
    files[M_LOOKUP] = ixp_create(ixps, "/lookup");
    files[M_FONT] = wmii_create_ixpfile(ixps, "/font", BLITZ_FONT);
    files[M_FONT]->after_write = handle_after_write;
    font = blitz_getfont(dpy, files[M_FONT]->content);
    files[M_SEL_BG_COLOR] =
        wmii_create_ixpfile(ixps, "/sstyle/bgcolor", BLITZ_SEL_BG_COLOR);
    files[M_SEL_TEXT_COLOR] =
        wmii_create_ixpfile(ixps, "/sstyle/fgcolor", BLITZ_SEL_FG_COLOR);
    files[M_SEL_BORDER_COLOR] =
        wmii_create_ixpfile(ixps, "/sstyle/bordercolor",
                            BLITZ_SEL_BORDER_COLOR);
    files[M_NORM_BG_COLOR] =
        wmii_create_ixpfile(ixps, "/nstyle/bgcolor", BLITZ_NORM_BG_COLOR);
    files[M_NORM_TEXT_COLOR] =
        wmii_create_ixpfile(ixps, "/nstyle/fgcolor", BLITZ_NORM_FG_COLOR);
    files[M_NORM_BORDER_COLOR] =
        wmii_create_ixpfile(ixps, "/nstyle/bordercolor",
                            BLITZ_NORM_BORDER_COLOR);

    wa.override_redirect = 1;
    wa.background_pixmap = ParentRelative;
    wa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask
        | SubstructureRedirectMask | SubstructureNotifyMask;

    rect.x = rect.y = 0;
    rect.width = DisplayWidth(dpy, screen_num);
    rect.height = DisplayHeight(dpy, screen_num);
    mrect = rect;
    mrect.height = font->ascent + font->descent + 4;
    mrect.y = rect.height - mrect.height;

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
    run_server_with_fd_support(ixps, ConnectionNumber(dpy), check_event,
                               0);
    deinit_server(ixps);
    XFreePixmap(dpy, pmap);
    XFreeGC(dpy, gc);
    XCloseDisplay(dpy);

    return 0;
}
