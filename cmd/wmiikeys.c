/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

#include "wmii.h"

/* array indexes for file pointers */
typedef enum {
    K_CTL,
    K_LOOKUP,
    K_GRAB_KB,
    K_FONT,
    K_FG_COLOR,
    K_BG_COLOR,
    K_BORDER_COLOR,
    K_LAST
} KeyIndexes;

typedef struct Shortcut Shortcut;

struct Shortcut {
    char name[MAX_BUF];
    unsigned long mod;
    KeyCode key;
    File *cmdfile;
    Shortcut *snext;
    Shortcut *next;
};

static IXPServer *ixps = nil;
static Display *dpy;
static GC gc;
static Window win;
static Window root;
static XRectangle krect;
static XRectangle rect;
static int screen_num;
static char *sockfile = nil;
static Shortcut *shortcuts = nil;
static File *files[K_LAST];
static int grabkb = 0;
static unsigned int num_lock_mask, valid_mask;
static char buf[MAX_BUF];
static XFontStruct *font;

static void grab_shortcut(Shortcut * s);
static void ungrab_shortcut(Shortcut * s);
static void draw_shortcut_box(char *text);
static void quit(void *obj, char *arg);

static Action acttbl[] = {
    {"quit", quit},
    {0, 0}
};

static char *version[] = {
    "wmiikeys - window manager improved keys - " VERSION "\n"
        "  (C)opyright MMIV-MMVI Anselm R. Garbe\n", 0
};

static void
usage()
{
    fprintf(stderr, "%s",
            "usage: wmiikeys [-s <socket file>] [-v]\n"
            "      -s     socket file (default: /tmp/.ixp-$USER/wmiikeys-$WMII_IDENT)\n"
            "      -v     version info\n");
    exit(1);
}

static void
center()
{
    krect.x = rect.width / 2 - krect.width / 2;
    krect.y = rect.height / 2 - krect.height / 2;
}

/* grabs shortcut on all screens */
static void
grab_shortcut(Shortcut * s)
{
    XGrabKey(dpy, s->key, s->mod, root,
             True, GrabModeAsync, GrabModeAsync);
    if(num_lock_mask) {
        XGrabKey(dpy, s->key, s->mod | num_lock_mask, root,
                 True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, s->key, s->mod | num_lock_mask | LockMask, root,
                 True, GrabModeAsync, GrabModeAsync);
    }
    XSync(dpy, False);
}

/*
 * don't handle evil keys anymore, just define more shortcuts if you cannot
 * live without evil key handling
 */
static void
ungrab_shortcut(Shortcut * s)
{
    XUngrabKey(dpy, s->key, s->mod, root);
    if(num_lock_mask) {
        XUngrabKey(dpy, s->key, s->mod | num_lock_mask, root);
        XUngrabKey(dpy, s->key, s->mod | num_lock_mask | LockMask, root);
    }
    XSync(dpy, False);
}

static void
create_shortcut(File * f)
{
    static char *chain[8];
    char *k;
    size_t i, toks;
    Shortcut *s = 0, *r = 0;

    cext_strlcpy(buf, f->name, sizeof(buf));
    toks = cext_tokenize(chain, 8, buf, ',');

    for(i = 0; i < toks; i++) {
        if(!s)
            r = s = cext_emallocz(sizeof(Shortcut));
        else {
            s->snext = cext_emallocz(sizeof(Shortcut));
            s = s->snext;
        }
        cext_strlcpy(s->name, chain[i], MAX_BUF);
        k = strrchr(chain[i], '-');
        if(k)
            k++;
        else
            k = chain[i];
        s->key = XKeysymToKeycode(dpy, XStringToKeysym(k));
        s->mod = blitz_strtomod(chain[i]);
    }
    if(r) {
        s->cmdfile = f;
        if(!shortcuts)
            shortcuts = r;
        else {
            for(s = shortcuts; s && s->next; s = s->next);
            s->next = r;
        }
        grab_shortcut(r);
    }
}

static void
destroy_shortcut(Shortcut * s, int ungrab)
{
    if(s->snext)
        destroy_shortcut(s->snext, 0);
    if(ungrab)
        ungrab_shortcut(s);
    free(s);
}

static void
next_keystroke(unsigned long *mod, KeyCode * key)
{
    XEvent e;
    KeySym sym;
    *mod = 0;
    do {
        XMaskEvent(dpy, KeyPressMask, &e);
        *mod |= e.xkey.state & valid_mask;
        *key = (KeyCode) e.xkey.keycode;
        sym = XKeycodeToKeysym(dpy, e.xkey.keycode, 0);
    } while(IsModifierKey(sym));
}

static void
emulate_key_press(unsigned long mod, KeyCode key)
{
    XEvent e;
    Window client_win;
    int revert;

    XGetInputFocus(dpy, &client_win, &revert);

    e.xkey.type = KeyPress;
    e.xkey.time = CurrentTime;
    e.xkey.window = client_win;
    e.xkey.display = dpy;
    e.xkey.state = mod;
    e.xkey.keycode = key;
    XSendEvent(dpy, client_win, True, KeyPressMask, &e);
    e.xkey.type = KeyRelease;
    XSendEvent(dpy, client_win, True, KeyReleaseMask, &e);
    XSync(dpy, False);
}

static void
handle_shortcut_chain(Window w, Shortcut * processed, char *prefix,
                      int grab)
{
    unsigned long mod;
    KeyCode key;
    Shortcut *s = processed->snext;

    if(grab) {
        XGrabKeyboard(dpy, w, True, GrabModeAsync, GrabModeAsync,
                      CurrentTime);
        XMapRaised(dpy, win);
    }
    draw_shortcut_box(prefix);
    next_keystroke(&mod, &key);

    if((processed->mod == mod) && (processed->key == key)) {
        /* double shortcut */
        emulate_key_press(mod, key);
    } else if((s->mod == mod) && (s->key == key)) {
        if(s->cmdfile && s->cmdfile->content)
            wmii_spawn(dpy, s->cmdfile->content);
        else if(s->snext) {
            snprintf(buf, sizeof(buf), "%s/%s", prefix, s->name);
            handle_shortcut_chain(w, s, buf, 0);
        }
    }
    if(grab) {
        XUngrabKeyboard(dpy, CurrentTime);
        XUnmapWindow(dpy, win);
        XSync(dpy, False);
    }
}

static void
handle_shortcut_gkb(Window w, unsigned long mod, KeyCode key)
{
    Shortcut *s;
    if(!files[K_LOOKUP]->content)
        return;

    for(s = shortcuts; s && ((s->mod != mod) || (s->key != key));
        s = s->next);
    if(s && s->cmdfile && s->cmdfile->content) {
        wmii_spawn(dpy, s->cmdfile->content);
        return;
    }
    XBell(dpy, 0);
}

static void
handle_shortcut(Window w, unsigned long mod, KeyCode key)
{
    Shortcut *s;
    if(!files[K_LOOKUP]->content)
        return;

    for(s = shortcuts; s && ((s->mod != mod) || (s->key != key));
        s = s->next);
    if(s && s->cmdfile && s->cmdfile->content) {
        wmii_spawn(dpy, s->cmdfile->content);
        return;
    }
    if(s && s->snext)
        handle_shortcut_chain(w, s, s->name, 1);
}

static void
quit(void *obj, char *arg)
{
    ixps->runlevel = SHUTDOWN;
}

static void
update()
{
    Shortcut *s;
    File *f, *p;
    if(!files[K_LOOKUP]->content)
        return;

    f = ixp_walk(ixps, files[K_LOOKUP]->content);

    if(!f || !is_directory(f))
        return;                 /* cannot update */

    /* destroy existing shortcuts if any */
    while((s = shortcuts)) {
        shortcuts = shortcuts->next;
        destroy_shortcut(s, 1);
    }
    shortcuts = nil;

    if(grabkb) {
        XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync,
                      CurrentTime);
        return;
    }
    /* create new shortcuts */
    for(p = f->content; p; p = p->next)
        create_shortcut(p);
}

/*
 * Function assumes following fs-structure:
 *
 * /box/style/font   		"<value>"
 * /box/style/fgcolor     	"#RRGGBBAA"
 * /box/style/bgcolor     	"#RRGGBBAA"
 * /box/style/bordercolor   "#RRGGBBAA"
 */
static void
draw_shortcut_box(char *text)
{
    Draw d = { 0 };

    d.font = font;
    krect.width = XTextWidth(d.font, text, strlen(text)) + krect.height;
    krect.height = font->ascent + font->descent + 4;
    center();
    XMoveResizeWindow(dpy, win, krect.x, krect.y, krect.width,
                      krect.height);

    /* default stuff */
    d.gc = gc;
    d.drawable = win;
    d.data = text;
    d.rect.y = 0;
    d.rect.width = krect.width;
    d.rect.height = krect.height;
    d.bg = blitz_loadcolor(dpy, screen_num, files[K_BG_COLOR]->content);
    d.fg = blitz_loadcolor(dpy, screen_num, files[K_FG_COLOR]->content);
    d.border =
        blitz_loadcolor(dpy, screen_num, files[K_BORDER_COLOR]->content);
    blitz_drawlabel(dpy, &d);
}

static void
check_event(Connection * c)
{
    XEvent ev;

    while(XPending(dpy)) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
        case KeyPress:
            ev.xkey.state &= valid_mask;
            if(grabkb)
                handle_shortcut_gkb(root, ev.xkey.state,
                                    (KeyCode) ev.xkey.keycode);
            else
                handle_shortcut(root, ev.xkey.state,
                                (KeyCode) ev.xkey.keycode);
            break;
        case KeymapNotify:
            update();
            break;
        default:
            break;
        }
    }
}

static void
handle_after_write(IXPServer * s, File * f)
{
    int i;
    size_t len;

    if(f == files[K_CTL]) {
        for(i = 0; acttbl[i].name; i++) {
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
    } else if(f == files[K_GRAB_KB]) {
        grabkb = blitz_strtonum(files[K_GRAB_KB]->content, 0, 1);
        if(!grabkb) {
            XUngrabKeyboard(dpy, CurrentTime);
            XUnmapWindow(dpy, win);
            XSync(dpy, False);
        } else
            update();
    } else if(f == files[K_FONT]) {
        XFreeFont(dpy, font);
        font = blitz_getfont(dpy, files[K_FONT]->content);
        krect = rect;
        krect.height = font->ascent + font->descent + 4;
        center();
        XMoveResizeWindow(dpy, win, krect.x, krect.y, krect.width,
                          krect.height);
    } else if(f == files[K_LOOKUP]) {
        update();
    }
    check_event(0);
}

static int
dummy_error_handler(Display * dpy, XErrorEvent * err)
{
    return 0;
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
        fprintf(stderr, "%s", "wmiikeys: cannot open display\n");
        exit(1);
    }
    XSetErrorHandler(dummy_error_handler);
    screen_num = DefaultScreen(dpy);

    /* init */
    ixps = wmii_setup_server(sockfile);

    if(!(files[K_CTL] = ixp_create(ixps, "/ctl"))) {
        perror("wmiikeys: cannot connect IXP server");
        exit(1);
    }
    files[K_CTL]->after_write = handle_after_write;
    files[K_LOOKUP] = ixp_create(ixps, "/lookup");
    files[K_LOOKUP]->after_write = handle_after_write;
    files[K_GRAB_KB] = wmii_create_ixpfile(ixps, "/grabkeyb", "0");
    files[K_GRAB_KB]->after_write = handle_after_write;
    files[K_FONT] = wmii_create_ixpfile(ixps, "/box/font", BLITZ_FONT);
    files[K_FONT]->after_write = handle_after_write;
    font = blitz_getfont(dpy, files[K_FONT]->content);
    files[K_FG_COLOR] =
        wmii_create_ixpfile(ixps, "/box/fgcolor", BLITZ_SEL_FG_COLOR);
    files[K_BG_COLOR] =
        wmii_create_ixpfile(ixps, "/box/bgcolor", BLITZ_SEL_BG_COLOR);
    files[K_BORDER_COLOR] =
        wmii_create_ixpfile(ixps, "/box/bordercolor",
                            BLITZ_SEL_BORDER_COLOR);

    wa.override_redirect = 1;
    wa.background_pixmap = ParentRelative;
    wa.event_mask =
        ExposureMask | SubstructureRedirectMask | SubstructureNotifyMask;

    root = RootWindow(dpy, screen_num);
    rect.x = rect.y = 0;
    rect.width = DisplayWidth(dpy, screen_num);
    rect.height = DisplayHeight(dpy, screen_num);
    krect.x = krect.y = 0;
    krect.width = krect.height = 1;

    wmii_init_lock_modifiers(dpy, &valid_mask, &num_lock_mask);

    win = XCreateWindow(dpy, RootWindow(dpy, screen_num), krect.x, krect.y,
                        krect.width, krect.height, 0, DefaultDepth(dpy,
                                                                   screen_num),
                        CopyFromParent, DefaultVisual(dpy, screen_num),
                        CWOverrideRedirect | CWBackPixmap | CWEventMask,
                        &wa);
    XDefineCursor(dpy, win, XCreateFontCursor(dpy, XC_left_ptr));
    XSync(dpy, False);

    gcv.function = GXcopy;
    gcv.graphics_exposures = False;

    gc = XCreateGC(dpy, win, 0, 0);

    /* main event loop */
    run_server_with_fd_support(ixps, ConnectionNumber(dpy),
                               check_event, 0);
    deinit_server(ixps);
    XFreeGC(dpy, gc);
    XCloseDisplay(dpy);

    return 0;
}
