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
 * /ctl					Fctl		command interface
 * /grab				Fgrab		setup interface
 * /event				Fevent		read for receiving key presses
 * /foo					Fkey        key file
 */

/* 8-bit qid.path.type */
enum {                          
    Droot,
	Fctl,
	Fgrab,
	Fevent,
    Fkey
};

typedef struct Key Key;
struct Key {
	unsigned short id;
    char name[128];
    unsigned long mod;
    KeyCode key;
    Key *next;
};

static IXPServer srv;
static Display *dpy;
static Window root;
static Key **key = nil;
static size_t keysz = 0;
static size_t nkey = 0;
static unsigned int num_lock_mask, valid_mask;
Qid root_qid;

static char version[] = "wmiikeys - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";

static void do_pend_fcall(char *event);

static void
usage()
{
    fprintf(stderr, "%s", "usage: wmiikeys [-a <address>] [-v]\n");
    exit(1);
}

/* X stuff */

static void
grab_key(Key *k)
{
    XGrabKey(dpy, k->key, k->mod, root,
             True, GrabModeAsync, GrabModeAsync);
    if(num_lock_mask) {
        XGrabKey(dpy, k->key, k->mod | num_lock_mask, root,
                 True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, k->key, k->mod | num_lock_mask | LockMask, root,
                 True, GrabModeAsync, GrabModeAsync);
    }
    XSync(dpy, False);
}

static void
ungrab_key(Key *k)
{
    XUngrabKey(dpy, k->key, k->mod, root);
    if(num_lock_mask) {
        XUngrabKey(dpy, k->key, k->mod | num_lock_mask, root);
        XUngrabKey(dpy, k->key, k->mod | num_lock_mask | LockMask, root);
    }
    XSync(dpy, False);
}

static Key *
create_key(char *name)
{
	char buf[128];
    char *seq[8];
    char *key;
    size_t i, toks;
	static unsigned short id = 1;
    Key *k = 0, *r = 0;

    cext_strlcpy(buf, name, sizeof(buf));
    toks = cext_tokenize(seq, 8, buf, ',');

    for(i = 0; i < toks; i++) {
        if(!k)
            r = k = cext_emallocz(sizeof(Key));
        else {
            k->next = cext_emallocz(sizeof(Key));
            k = k->next;
        }
        cext_strlcpy(k->name, name, sizeof(k->name));
        key = strrchr(seq[i], '-');
        if(key)
            key++;
        else
            key = seq[i];
        k->key = XKeysymToKeycode(dpy, XStringToKeysym(key));
        k->mod = blitz_strtomod(seq[i]);
    }
	r->id = id++;
	return r;

}

static void
destroy_key(Key *k)
{
    if(k->next)
        destroy_key(k->next);
    free(k);
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

static Key **
match_keys(Key **t, size_t n, unsigned long mod, KeyCode keycode, Bool next, size_t *nres)
{
	Key **result = nil;
	size_t ressz = 0;
	size_t i = 0;
	*nres = 0;
	for(i = 0; i < n; i++) {
		Key *k = next ? t[i]->next : t[i];
		if(k && (k->mod == mod) && (k->key == keycode)) {
			result = (Key **)cext_array_attach((void **)result, k, sizeof(Key *), &ressz);
			(*nres)++;
		}
	}
	return result;
}

static void
handle_key_seq(Window w, Key **done, size_t ndone)
{
    unsigned long mod;
    KeyCode key;
	Key **found = nil;
	size_t nfound = 0; 
	char buf[128];

    next_keystroke(&mod, &key);

	found = match_keys(done, ndone, mod, key, True, &nfound);
	if((done[0]->mod == mod) && (done[0]->key == key))
		emulate_key_press(mod, key); /* double key */
	else {
		switch(nfound) {
		case 0:
			XBell(dpy, 0);
			return; /* grabbed but not found */
		case 1: 
			if(!found[0]->next) {
				snprintf(buf, sizeof(buf), "%s\n", found[0]->name);
				do_pend_fcall(buf);
				break;
			}
		default:
			handle_key_seq(w, found, nfound);
			break;
		}
	}
	free(found);
}

static void
handle_key(Window w, unsigned long mod, KeyCode keycode)
{
	size_t nfound;
	char buf[128];
	Key **found = match_keys(key, nkey, mod, keycode, False, &nfound);
	switch(nfound) {
	case 0:
		XBell(dpy, 0);
		return; /* grabbed but not found */
	case 1: 
		if(!found[0]->next) {
			snprintf(buf, sizeof(buf), "%s\n", found[0]->name);
			do_pend_fcall(buf);
			break;
		}
	default:
		XGrabKeyboard(dpy, w, True, GrabModeAsync, GrabModeAsync, CurrentTime);
		handle_key_seq(w, found, nfound);
		XUngrabKeyboard(dpy, CurrentTime);
		XSync(dpy, False);
		break;
    }
	free(found);
}

static void
check_x_event(IXPConn *c)
{
    XEvent ev;
    while(XPending(dpy)) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
        case KeyPress:
            ev.xkey.state &= valid_mask;
            handle_key(root, ev.xkey.state, (KeyCode) ev.xkey.keycode);
            break;
        case KeymapNotify:
			{
				size_t i;
				for(i = 0; i < nkey; i++) {
					ungrab_key(key[i]);
					grab_key(key[i]);
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

static Key *
key_of_name(char *name)
{
	size_t i;
	for(i = 0; i < nkey; i++)
		if(!strncmp(key[i]->name, name, sizeof(key[i]->name)))
			return key[i];
	return nil;
}

static int
index_of_id(unsigned short id)
{
	int i;
	for(i = 0; i < nkey; i++)
		if(key[i]->id == id)
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
		case Fctl: return "ctl"; break;
		case Fgrab: return "grab"; break;
		case Fevent: return "event"; break;
		case Fkey: return key[i]->name; break;
		default: return nil; break;
	}
}

static int
name_to_type(char *name)
{
	if(!name || !name[0] || !strncmp(name, "/", 2) || !strncmp(name, "..", 3))
		return Droot;
	if(!strncmp(name, "ctl", 4))
		return Fctl;
	if(!strncmp(name, "grab", 5))
		return Fgrab;
	if(!strncmp(name, "event", 6))
		return Fevent;
	if(key_of_name(name))
		return Fkey;
	return -1;
}

static int
mkqid(Qid *dir, char *wname, Qid *new, Bool iswalk)
{
	Key *k;
	int type = name_to_type(wname);
   
    if((dir->type != IXP_QTDIR) || (type == -1))
        return -1;
	
	new->dtype = qpath_type(dir->path);
    new->version = 0;
	switch(type) {
	case Droot:
		*new = root_qid;
		break;
	case Fkey:
		if(!(k = key_of_name(wname)))
			return -1;
		new->type = IXP_QTFILE;
		new->path = mkqpath(type, k->id);
		break;
	default:
		new->type = IXP_QTFILE;
    	new->path = mkqpath(type, 0);
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
    fcall->iounit = 2048;
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
	int type = name_to_type(name);

    switch (type) {
    case Droot:
	case Fctl:
	case Fgrab:
		return mkstat(stat, dir, name, 0, DMWRITE);
		break;
	case Fevent:
		return mkstat(stat, dir, name, 0, DMREAD);
		break;
    case Fkey:
		return mkstat(stat, dir, name, 0, 0);
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
	if((qpath_type(m->qid.path) == Fkey) && (i < nkey)) {
		Key *k = key[i];
		/* clunk */
		cext_array_detach((void **)c->map, m, &c->mapsz);
    	free(m);
		/* now detach the item */
		cext_array_detach((void **)key, k, &keysz);
		nkey--;
		destroy_key(k);
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
	unsigned int len = 0;
	int i;

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
			len += type_to_stat(&stat, "grab", &m->qid);
			len += type_to_stat(&stat, "event", &m->qid);
			for(i = 0; i < nkey; i++) {
				len += type_to_stat(&stat, key[i]->name, &m->qid);
				fprintf(stderr, "len=%d <= fcall->offset=%lld\n", len, fcall->offset);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < nkey; i++) {
				fprintf(stderr, "offset xread %s\n", key[i]->name);
				len = type_to_stat(&stat, key[i]->name, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Fevent:
			ixp_server_enqueue_fcall(c, fcall);
			return nil;
		default:
			break;
		}
	}
	else {
		switch (qpath_type(m->qid.path)) {
		case Droot:
			fcall->count = type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "grab", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "event", &m->qid);
			p = ixp_enc_stat(p, &stat);
			for(i = 0; i < nkey; i++) {
				fprintf(stderr, "normal xread %s\n", key[i]->name);
				len = type_to_stat(&stat, key[i]->name, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Fctl:
		case Fevent:
			ixp_server_enqueue_fcall(c, fcall);
			return nil;
		default:
			return Enoperm;
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
	char buf[128];
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
    case Fgrab:
		if(fcall->count > 2048)
			return Enoperm;
		{
			static size_t lastcount;
			static char last[2048]; /* iounit */
			char fcallbuf[2048], tmp[2048]; /* iounit */
			char *p1, *p2;
			Key *k;
			if(!fcall->offset) {
				while(nkey) {
					Key *k = key[0];
					ungrab_key(k);
					cext_array_detach((void **)key, k, &keysz);
					nkey--;
					destroy_key(k);
				}
			}
		    memcpy(fcallbuf, fcall->data, fcall->count);
		    fcallbuf[fcall->count] = 0;
			if(fcall->offset) {
				p1 = strrchr(last, '\n');
				p2 = strchr(fcallbuf, '\n');
				memcpy(tmp, p1, lastcount - (p1 - last));
				memcpy(tmp + (lastcount - (p1 - last)), p2, p2 - fcallbuf);
				tmp[(lastcount - (p1 - last)) + (p2 - fcallbuf)] = 0;
				k = create_key(tmp);
				key = (Key **)cext_array_attach((void **)key, k, sizeof(Key *), &keysz);
				nkey++;
				grab_key(k);

			}
			else p2 = fcallbuf;
			lastcount = fcall->count;
			memcpy(last, fcall->data, fcall->count);
			while(p2 - fcallbuf < fcall->count) {
				p1 = strchr(p2, '\n');
				*p1 = 0;
				k = create_key(p2);
				key = (Key **)cext_array_attach((void **)key, k, sizeof(Key *), &keysz);
				nkey++;
				grab_key(k);
				*p1 = '\n';
				p2 = ++p1;
			}
			lastcount = fcall->count;
			memcpy(last, fcall->data, fcall->count);
		}
		break;
	default:
		return Enoperm;
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
		case TVERSION: errstr = wmii_ixp_version(c, &fcall); break;
		case TATTACH: errstr = wmii_ixp_attach(c, &fcall); break;
		case TWALK: errstr = xwalk(c, &fcall); break;
		case TREMOVE: errstr = xremove(c, &fcall); break;
		case TOPEN: errstr = xopen(c, &fcall); break;
		case TREAD: errstr = xread(c, &fcall); break;
		case TWRITE: errstr = xwrite(c, &fcall); break;
		case TCLUNK: errstr = wmii_ixp_clunk(c, &fcall); break;
		case TSTAT: errstr = xstat(c, &fcall); break;
		default: errstr = Enofunc; break;
		}
		if(errstr)
			ixp_server_respond_error(c, &fcall, errstr);
	}
	check_x_event(nil);
}

static void
do_pend_fcall(char *event)
{
	size_t i;
	Fcall *fcall;
	for(i = 0; (i < srv.connsz) && srv.conn[i]; i++) {
		IXPConn *c = srv.conn[i];
		/* all pending TREADs are on /event, so no qid checking necessary */
		while((fcall = ixp_server_dequeue_fcall_id(c, TREAD))) {
			IXPMap *m = ixp_server_fid2map(c, fcall->fid);
			unsigned char *p = fcall->data;

			if(!m) {
				if(ixp_server_respond_error(c, fcall, Enofid))
					break;
			}
			else if(qpath_type(m->qid.path) == Fevent) {
				fcall->count = strlen(event);
				memcpy(p, event, fcall->count);
				fcall->id = RREAD;
				if(ixp_server_respond_fcall(c, fcall))
					break;
			}
			free(fcall);
		}
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
	char *errstr;
	char *address = nil;

    /* command line args */
    for(i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
        switch (argv[i][1]) {
        case 'v':
            fprintf(stdout, "%s", version);
            exit(0);
            break;
        case 'a':
            if(i + 1 < argc)
                address = argv[++i];
            else
                usage();
            break;
        default:
            usage();
            break;
        }
    }

    if(!address)
		usage();

    dpy = XOpenDisplay(0);
    if(!dpy) {
        fprintf(stderr, "%s", "wmiikeys: cannot open display\n");
        exit(1);
    }
    XSetErrorHandler(dummy_error_handler);

	i = ixp_create_sock(address, &errstr);
	if(i < 0) {
        fprintf(stderr, "wmiibar: fatal: %s\n", errstr);
		exit(1);
	}

	/* IXP server */
	ixp_server_open_conn(&srv, i, new_ixp_conn, ixp_server_close_conn);
    root_qid.type = IXP_QTDIR;
    root_qid.version = 0;
    root_qid.path = mkqpath(Droot, 0);

	/* X server */
	ixp_server_open_conn(&srv, ConnectionNumber(dpy), check_x_event, nil);

    root = RootWindow(dpy, DefaultScreen(dpy));
    wmii_init_lock_modifiers(dpy, &valid_mask, &num_lock_mask);

    /* main loop */
	errstr = ixp_server_loop(&srv);
	if(errstr)
    	fprintf(stderr, "wmiibar: fatal: %s\n", errstr);

	/* cleanup */
	ixp_server_close(&srv);
	XCloseDisplay(dpy);

	return errstr ? 1 : 0;
}
