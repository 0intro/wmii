/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

#include "wmii.h"

/*
 * filesystem specification
 * / 					Droot
 * /font				Ffont		<xlib font name>
 * /color				Fcolor		<#RRGGBB> <#RRGGBB> <#RRGGBB>
 * /ctl					Fctl		command interface
 * /reset				Freset		setup interface
 * /shortcut/			Dshortcut
 * /shortcut/foo		Fshortcut   shortcut file
 */

/* 8-bit qid.path.type */
enum {                          
    Droot,
    Dshortcut,
	Fctl,
    Ffont,
    Fcolor,
	Freset,
    Fshortcut
};

typedef struct Shortcut Shortcut;

struct Shortcut {
	unsigned short id;
    char name[256];
	char *cmd;
    unsigned long mod;
    KeyCode key;
    Shortcut *next;
};

static IXPServer srv;
static Display *dpy;
static Window win;
static Window root;
static XRectangle rect;
static int screen;
static Shortcut **shortcut = nil;
static size_t shortcutsz = 0;
static size_t nshortcut = 0;
static int grabkb = 0;
static unsigned int num_lock_mask, valid_mask;
static char *font;
static char *colstr;
static Draw box;
Qid root_qid;

static void draw_shortcut_box(char *prefix);

static char version[] = "wmiikeys - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";

static void
usage()
{
    fprintf(stderr, "%s", "usage: wmiikeys [-a <address>] [-v]\n");
    exit(1);
}

/* X stuff */

static void
center()
{
    int x = rect.width / 2 - box.rect.width / 2;
    int y = rect.height / 2 - box.rect.height / 2;
    XMoveResizeWindow(dpy, win, x, y, box.rect.width, box.rect.height);
	XSync(dpy, False);
}

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

static Shortcut *
create_shortcut(char *name, char *cmd)
{
	char buf[256];
    char *chain[8];
    char *k;
    size_t i, toks;
	static unsigned short id = 1;
    Shortcut *s = 0, *r = 0;

    cext_strlcpy(buf, name, sizeof(buf));
    toks = cext_tokenize(chain, 8, buf, ',');

    for(i = 0; i < toks; i++) {
        if(!s)
            r = s = cext_emallocz(sizeof(Shortcut));
        else {
            s->next = cext_emallocz(sizeof(Shortcut));
            s = s->next;
        }
        cext_strlcpy(s->name, chain[i], sizeof(s->name));
        k = strrchr(chain[i], '-');
        if(k)
            k++;
        else
            k = chain[i];
        s->key = XKeysymToKeycode(dpy, XStringToKeysym(k));
        s->mod = blitz_strtomod(chain[i]);
    }
	r->id = id++;
	return r;

}

/* create
    if(r) {
        s->cmd = cmd;
		shortcut = (Shortcut **)cext_attach_array((void **)shortcut, r, &shortcutsz);
		nshortcut++;
        grab_shortcut(r);
    }
	*/

static void
destroy_shortcut(Shortcut *s)
{
    if(s->next)
        destroy_shortcut(s->next);
    free(s);
}

static void
next_keystroke(unsigned long *mod, KeyCode *key)
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
handle_shortcut_chain(Window w, Shortcut *processed, char *prefix)
{
	char buf[256];
    unsigned long mod;
    KeyCode key;
    Shortcut *s = processed->next;

    draw_shortcut_box(prefix);
    next_keystroke(&mod, &key);

    if((processed->mod == mod) && (processed->key == key)) {
        /* double shortcut */
        emulate_key_press(mod, key);
    } else if((s->mod == mod) && (s->key == key)) {
        if(s->cmd)
            wmii_spawn(dpy, s->cmd);
        else if(s->next) {
            snprintf(buf, sizeof(buf), "%s/%s", prefix, s->name);
            handle_shortcut_chain(w, s, buf);
        }
    }
}

static void
handle_shortcut_gkb(Window w, unsigned long mod, KeyCode key)
{
	size_t i;
	for(i = 0; i < nshortcut; i++) {
		Shortcut *s = shortcut[i];
		if((s->mod == mod) && (s->key == key)) {
			if(s->cmd)
        		wmii_spawn(dpy, s->cmd);
        	return;
		}
    }
    XBell(dpy, 0);
}

static void
handle_shortcut(Window w, unsigned long mod, KeyCode key)
{
	size_t i;
	for(i = 0; i < nshortcut; i++) {
		Shortcut *s = shortcut[i];
		if((s->mod == mod) && (s->key == key)) {
			if(s->next) {
        		XGrabKeyboard(dpy, w, True, GrabModeAsync, GrabModeAsync, CurrentTime);
        		XMapRaised(dpy, win);
        		XSync(dpy, False);

				handle_shortcut_chain(w, s, s->name);

        		XUngrabKeyboard(dpy, CurrentTime);
        		XUnmapWindow(dpy, win);
        		XSync(dpy, False);
			}
			else if(s->cmd)
        		wmii_spawn(dpy, s->cmd);
        	return;
		}
    }
}

static void
draw_shortcut_box(char *prefix)
{
	if(!strlen(box.data))
		return;
	box.rect.x = box.rect.y = 0;
    box.rect.height = box.font->ascent + box.font->descent + 4;
    box.rect.width = XTextWidth(box.font, prefix, strlen(prefix)) + box.rect.height;
	box.data = prefix;
    center();
    blitz_drawlabel(dpy, &box);
}

static void
check_event(IXPConn *c)
{
    XEvent ev;
    while(XPending(dpy)) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
        case KeyPress:
            ev.xkey.state &= valid_mask;
            if(grabkb)
                handle_shortcut_gkb(root, ev.xkey.state, (KeyCode) ev.xkey.keycode);
            else
                handle_shortcut(root, ev.xkey.state, (KeyCode) ev.xkey.keycode);
            break;
        case KeymapNotify:
			{
				size_t i;
				for(i = 0; i < nshortcut; i++) {
					ungrab_shortcut(shortcut[i]);
					grab_shortcut(shortcut[i]);
				}
			}
            break;
        default:
            break;
        }
    }
}

static int
dummy_error_handler(Display * dpy, XErrorEvent * err)
{
    return 0;
}

static Shortcut *
shortcut_of_name(char *name)
{
	size_t i;
	for(i = 0; i < nshortcut; i++)
		if(!strncmp(shortcut[i]->name, name, sizeof(shortcut[i]->name)))
			return shortcut[i];
	return nil;
}

static int
index_of_id(unsigned short id)
{
	int i;
	for(i = 0; i < nshortcut; i++)
		if(shortcut[i]->id == id)
			return i;
	return -1;
}

/* IXP stuff */

static unsigned long long
mkqpath(unsigned char type, unsigned short id)
{
    return ((unsigned long long) id << 8) | (unsigned long long) type;
}

static unsigned char
qpath_type(unsigned long long path)
{
    return path & 0xff;
}

static unsigned short
qpath_id(unsigned long long path)
{
    return (path >> 8) & 0xffff;
}

static char *
qid_to_name(Qid *qid)
{
	unsigned char type = qpath_type(qid->path);
	unsigned short id = qpath_id(qid->path);
	int i;

	if(id && ((i = index_of_id(id)) == -1))
		return nil;
	switch(type) {
		case Droot: return "/"; break;
		case Dshortcut: return "shortcut"; break;
		case Fctl: return "ctl"; break;
		case Ffont: return "font"; break;
		case Fcolor: return "color"; break;
		case Freset: return "reset"; break;
		case Fshortcut: return shortcut[i]->name; break;
		default: return nil; break;
	}
}

static int
name_to_type(char *name)
{
	if(!name || !name[0] || !strncmp(name, "/", 2) || !strncmp(name, "..", 3))
		return Droot;
	if(!strncmp(name, "shortcut", 4))
		return Dshortcut;
	if(!strncmp(name, "ctl", 4))
		return Fctl;
	if(!strncmp(name, "font", 5))
		return Ffont;
	if(!strncmp(name, "color", 6))
		return Fcolor;
	if(!strncmp(name, "reset", 6))
		return Freset;
	if(shortcut_of_name(name))
		return Fshortcut;
	return -1;
}

static int
mkqid(Qid *dir, char *wname, Qid *new, Bool iswalk)
{
	unsigned short id = qpath_id(dir->path);
	int i, type = name_to_type(wname);
   
    if(id && ((i = index_of_id(id)) == -1))
		return -1;

    if((dir->type != IXP_QTDIR) || (type == -1))
        return -1;
	
	new->dtype = qpath_type(dir->path);
    new->version = 0;
	switch(type) {
	case Droot:
		*new = root_qid;
		break;
	case Dshortcut:
		new->type = IXP_QTDIR;
		new->path = mkqpath(Dshortcut, 0);
		break;
	case Fshortcut:
		if(i >= nshortcut)
			return -1;
	default:
		new->type = IXP_QTFILE;
    	new->path = mkqpath(type, id);
		break;
	}
    return 0;
}

static char * 
xwalk(IXPConn *c, Fcall *fcall)
{
    unsigned short nwqid = 0;
    Qid dir = root_qid;
    IXPMap *m;

    if(!(m = ixp_server_fid2map(c, fcall->fid)))
        return Enofid;
    if(fcall->fid != fcall->newfid && (ixp_server_fid2map(c, fcall->newfid)))
        return Enofid;
    if(fcall->nwname) {
        dir = m->qid;
        for(nwqid = 0; (nwqid < fcall->nwname)
            && !mkqid(&dir, fcall->wname[nwqid], &fcall->wqid[nwqid], True); nwqid++) {
			/*fprintf(stderr, "wname=%s nwqid=%d\n", fcall->wname[nwqid], nwqid);*/
            dir = fcall->wqid[nwqid];
		}
        if(!nwqid)
			return Enofile;
    }
    /* a fid will only be valid, if the walk was complete */
    if(nwqid == fcall->nwname) {
        if(fcall->fid != fcall->newfid) {
			m = cext_emallocz(sizeof(IXPMap));
			c->map = (IXPMap **)cext_array_attach((void **)c->map,
						m, sizeof(IXPMap *), &c->mapsz);
        }
        m->qid = dir;
        m->fid = fcall->newfid;
    }
    fcall->id = RWALK;
    fcall->nwqid = nwqid;
	ixp_server_respond_fcall(c, fcall);
    return nil;
}

static char *
xopen(IXPConn *c, Fcall *fcall)
{
    IXPMap *m = ixp_server_fid2map(c, fcall->fid);

    if(!m)
        return Enofid;
    if(!(fcall->mode | IXP_OREAD) && !(fcall->mode | IXP_OWRITE))
        return Enomode;
    fcall->id = ROPEN;
    fcall->qid = m->qid;
    fcall->iounit = 256;
	ixp_server_respond_fcall(c, fcall);
    return nil;
}

static unsigned int
mkstat(Stat *stat, Qid *dir, char *name, unsigned long long length, unsigned int mode)
{
    stat->mode = mode;
    stat->atime = stat->mtime = time(0);
    cext_strlcpy(stat->uid, getenv("USER"), sizeof(stat->uid));
    cext_strlcpy(stat->gid, getenv("USER"), sizeof(stat->gid));
    cext_strlcpy(stat->muid, getenv("USER"), sizeof(stat->muid));

    cext_strlcpy(stat->name, name, sizeof(stat->name));
    stat->length = length;
    mkqid(dir, name, &stat->qid, False);
	return ixp_sizeof_stat(stat);
}

static unsigned int
type_to_stat(Stat *stat, char *name, Qid *dir)
{
	int i, type = name_to_type(name);
	unsigned short id = qpath_id(dir->path);

	if(id && ((i = index_of_id(id)) == -1))
		return 0;
    switch (type) {
    case Droot:
    case Dshortcut:
		return mkstat(stat, dir, name, 0, DMDIR | DMREAD | DMEXEC);
        break;
	case Fctl:
	case Freset:
		return mkstat(stat, dir, name, 0, DMWRITE);
		break;
    case Ffont:
		return mkstat(stat, dir, name, strlen(font), DMREAD | DMWRITE);
        break;
    case Fcolor:
		return mkstat(stat, dir, name, 23, DMREAD | DMWRITE);
        break;
    case Fshortcut:
		return mkstat(stat, dir, name, strlen(shortcut[i]->cmd), DMREAD | DMWRITE);
		break;
    }
	return 0;
}

static char *
xremove(IXPConn *c, Fcall *fcall)
{
    IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	unsigned short id = qpath_id(m->qid.path);
	int i;

    if(!m)
        return Enofid;
	if(id && ((i = qpath_id(id)) == -1))
		return Enofile;
	if((qpath_type(m->qid.path) == Ditem) && i && (i < nitem)) {
		Item *it = item[i];
		/* clunk */
		cext_array_detach((void **)c->map, m, &c->mapsz);
    	free(m);
		/* now detach the item */
		detach_item(it);
		free(it);
		if(iexpand >= nitem)
			iexpand = 0;
		draw();
    	fcall->id = RREMOVE;
		ixp_server_respond_fcall(c, fcall);
		return nil;
	}
	return Enoperm;
}

static char *
xread(IXPConn *c, Fcall *fcall)
{
	Stat stat;
    IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	unsigned short id;
    unsigned char *p = fcall->data;
	unsigned int len;
	int i;
	char buf[32];

    if(!m)
        return Enofid;
	id = qpath_id(m->qid.path);
	if(id && ((i = index_of_id(id)) == -1))
		return Enofile;

	fcall->count = 0;
	if(fcall->offset) {
		switch (qpath_type(m->qid.path)) {
		case Droot:
			/* jump to offset */
			len = type_to_stat(&stat, "ctl", &m->qid);
			len += type_to_stat(&stat, "font", &m->qid);
			len += type_to_stat(&stat, "defcolor", &m->qid);
			len += type_to_stat(&stat, "expand", &m->qid);
			len += type_to_stat(&stat, "new", &m->qid);
			len += type_to_stat(&stat, "event", &m->qid);
			for(i = 0; i < nitem; i++) {
				snprintf(buf, sizeof(buf), "%u", i + 1);
				len += type_to_stat(&stat, buf, &m->qid);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < nitem; i++) {
				snprintf(buf, sizeof(buf), "%u", i + 1);
				len = type_to_stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Fevent:
			ixp_server_enqueue_fcall(c, fcall);
			return nil;
			break;
		default:
			break;
		}
	}
	else {
		switch (qpath_type(m->qid.path)) {
		case Droot:
			fcall->count = type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "font", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "defcolor", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "expand", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "new", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "event", &m->qid);
			p = ixp_enc_stat(p, &stat);
			for(i = 0; i < nitem; i++) {
				snprintf(buf, sizeof(buf), "%u", i + 1);
				len = type_to_stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Ditem:
			if(i >= nitem)
				return Enofile;
			fcall->count = type_to_stat(&stat, "color", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "data", &m->qid);
			p = ixp_enc_stat(p, &stat);
			break;
		case Fctl:
			return Enoperm;
			break;
		case Ffont:
			if((fcall->count = strlen(font)))
				memcpy(p, font, fcall->count);
			break;
		case Fdefcolor:
			if((fcall->count = strlen(defcolstr)))
				memcpy(p, defcolstr, fcall->count);
			break;
		case Fexpand:
			snprintf(buf, sizeof(buf), "%u", iexpand + 1);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case Fdata:
			if(i >= nitem)
				return Enofile;
			if((fcall->count = strlen(item[i]->data)))
				memcpy(p, item[i]->data, fcall->count);
			break;
		case Fcolor:
			if(i >= nitem)
				return Enofile;
			if((fcall->count = strlen(item[i]->colstr)))
				memcpy(p, item[i]->colstr, fcall->count);
			break;
		case Fevent:
			ixp_server_enqueue_fcall(c, fcall);
			return nil;
			break;
		default:
			return "invalid read";
			break;
		}
	}
	fcall->id = RREAD;
	ixp_server_respond_fcall(c, fcall);
    return nil;
}

static char *
xstat(IXPConn *c, Fcall *fcall)
{
    IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	char *name;

    if(!m)
        return Enofid;
	name = qid_to_name(&m->qid);
	/*fprintf(stderr, "xstat: name=%s\n", name);*/
	if(!type_to_stat(&fcall->stat, name, &m->qid))
		return Enofile;
    fcall->id = RSTAT;
	ixp_server_respond_fcall(c, fcall);
    return nil;
}

static char *
xwrite(IXPConn *c, Fcall *fcall)
{
	char buf[256];
    IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	unsigned short id;
	int i;

    if(!m)
        return Enofid;
	id = qpath_id(m->qid.path);
	if(id && ((i = index_of_id(id)) == -1))
		return Enofile;
	switch (qpath_type(m->qid.path)) {
	case Fctl:
		if(fcall->count == 4) {
			memcpy(buf, fcall->data, 4);
			buf[4] = 0;
			if(!strncmp(buf, "quit", 5)) {
				srv.running = 0;
				break;
			}
		}
		return Enocommand;
		break;
	case Ffont:
		if(font)
			free(font);
		font = cext_emallocz(fcall->count + 1);
		memcpy(font, fcall->data, fcall->count);
		XFreeFont(dpy, xfont);
    	xfont = blitz_getfont(dpy, font);
		update_geometry();
		break;
	case Fdefcolor:
		if((fcall->count != 23)
			|| (fcall->data[0] != '#') || (fcall->data[8] != '#')
		    || (fcall->data[16] != '#')
		  )
			return "wrong color format";
		memcpy(defcolstr, fcall->data, fcall->count);
		defcolstr[fcall->count] = 0;
		blitz_loadcolor(dpy, screen, defcolstr, &defcolor);
		break;
    case Fexpand:
		{
			const char *err;
			if(fcall->count && fcall->count < 16) {
				memcpy(buf, fcall->data, fcall->count);
				buf[fcall->count] = 0;
				i = (unsigned short) cext_strtonum(buf, 1, 0xffff, &err);
				if(!err && (i - 1 < nitem)) {
					iexpand = i - 1;
					draw();
					break;
				}
			}
		}
		return Enofile;
		break;
	case Fdata:
		{
			unsigned int len = fcall->count;
			if(i >= nitem)
				return Enofile;
			if(len >= sizeof(item[i]->data))
				len = sizeof(item[i]->data) - 1;
			memcpy(item[i]->data, fcall->data, len);
			item[i]->data[len] = 0;
			draw();
		}
		break;
	case Fcolor:
		if((i >= nitem) || (fcall->count != 23)
			|| (fcall->data[0] != '#') || (fcall->data[8] != '#')
		    || (fcall->data[16] != '#')
		  )
			return "wrong color format";
		memcpy(item[i]->colstr, fcall->data, fcall->count);
		item[i]->colstr[fcall->count] = 0;
		blitz_loadcolor(dpy, screen, item[i]->colstr, &item[i]->color);
		draw();
		break;
	default:
		return "invalid write";
		break;
	}
    fcall->id = RWRITE;
	ixp_server_respond_fcall(c, fcall);
	return nil;
}

static void
do_fcall(IXPConn *c)
{
	static Fcall fcall;
    unsigned int msize;
	char *errstr;

	if((msize = ixp_server_receive_fcall(c, &fcall))) {
		/*fprintf(stderr, "fcall=%d\n", fcall.id);*/
		switch(fcall.id) {
		case TVERSION: errstr = xversion(c, &fcall); break;
		case TATTACH: errstr = xattach(c, &fcall); break;
		case TWALK: errstr = xwalk(c, &fcall); break;
		case TREMOVE: errstr = xremove(c, &fcall); break;
		case TOPEN: errstr = xopen(c, &fcall); break;
		case TREAD: errstr = xread(c, &fcall); break;
		case TWRITE: errstr = xwrite(c, &fcall); break;
		case TCLUNK: errstr = xclunk(c, &fcall); break;
		case TSTAT: errstr = xstat(c, &fcall); break;
		default: errstr = Enofunc; break;
		}
		if(errstr)
			ixp_server_respond_error(c, &fcall, errstr);
	}
}

static void
new_ixp_conn(IXPConn *c)
{
	int fd = ixp_accept_sock(c->fd);
	
	if(fd >= 0)
		ixp_server_open_conn(c->srv, fd, do_fcall, ixp_server_close_conn);
}

/* main */

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
    screen = DefaultScreen(dpy);

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

    root = RootWindow(dpy, screen);
    rect.x = rect.y = 0;
    rect.width = DisplayWidth(dpy, screen);
    rect.height = DisplayHeight(dpy, screen);
    krect.x = krect.y = 0;
    krect.width = krect.height = 1;

    wmii_init_lock_modifiers(dpy, &valid_mask, &num_lock_mask);

    win = XCreateWindow(dpy, RootWindow(dpy, screen), krect.x, krect.y,
                        krect.width, krect.height, 0, DefaultDepth(dpy,
                                                                   screen),
                        CopyFromParent, DefaultVisual(dpy, screen),
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
