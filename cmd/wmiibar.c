/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <sys/socket.h>

#include "wmii.h"

/*
 * filesystem specification
 * / 					Droot
 * /font				Ffont		<xlib font name>
 * /defcolor			Fdefcolor	<#RRGGBB> <#RRGGBB> <#RRGGBB>
 * /event				Fevent
 * /expand				Fexpand 	id of expandable label
 * /ctl					Fctl 		command interface
 * /new/				Ditem
 * /1/					Ditem
 * /1/data 				Fdata		<arbitrary data which gets displayed>
 * /1/color				Fcolor		<#RRGGBB> <#RRGGBB> <#RRGGBB>
 */

/* 8-bit qid.path.type */
enum {                          
    Droot,
    Ditem,
	Fctl,
    Ffont,
    Fdefcolor,
	Fexpand,
    Fevent,
    Fdata,                      /* data to display */
    Fcolor
};

typedef struct {
	unsigned short id;
    char data[256];
	char colstr[24];
	Color color;
	XRectangle rect;
} Item;

static size_t nitem = 0;
static size_t itemsz = 0;
static size_t iexpand = 0;
static Item **item = 0;
static IXPServer srv = { 0 };
static Display *dpy;
static int screen;
static char *font = nil;
static XFontStruct *xfont;
static GC gc;
static Window win;
static XRectangle brect, rect;
static Pixmap pmap;
static char defcolstr[24];
static Color defcolor;
Qid root_qid;

static void do_pend_fcall(char *event);

static char version[] = "wmiibar - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";

static void
usage()
{
    fprintf(stderr, "%s", "usage: wmiibar -a <address> [-v]\n");
    exit(1);
}

static int
dummy_error_handler(Display * dpy, XErrorEvent * err)
{
    return 0;
}

static Item *
new_item()
{
	static unsigned int id = 1;
	Item *it = cext_emallocz(sizeof(Item));
	it->id = id++;
	cext_strlcpy(it->colstr, defcolstr, sizeof(it->colstr));
	it->color = defcolor;
	item = (Item **)cext_array_attach((void **)item, it, sizeof(Item *), &itemsz);
	nitem++;
	return it;
}

static void
detach_item(Item *it)
{
	cext_array_detach((void **)item, it, &itemsz);
	nitem--;
}

static void
draw()
{
	size_t i;
	unsigned int w = 0;
	Draw d = { 0 };

	d.align = WEST;
    d.gc = gc;
    d.drawable = pmap;
    d.rect = brect;
    d.rect.y = 0;
	d.font = xfont;

	if(!nitem) { /* /default only */
		d.color = defcolor;
		blitz_drawlabel(dpy, &d);
	}
	else {
		for(i = 0; i < nitem; i++) {
			Item *it = item[i];
			it->rect.x = it->rect.y = 0;
			it->rect.height = brect.height;
			if(i == iexpand)
		   		continue;
			it->rect.width = brect.height;
			if(strlen(it->data)) {
				if(!strncmp(it->data, "%m:", 3))
					it->rect.width = brect.height / 2;
				else
					it->rect.width += XTextWidth(xfont, it->data, strlen(it->data));
			}
			w += it->rect.width;
		}

		if(w >= brect.width) {
			/* failsafe mode, give all labels same width */
			w = brect.width / nitem;
			for(i = 0; i < nitem; i++) {
				item[i]->rect.x = i * w;
				item[i]->rect.width = w;
			}
			i--;
			item[i]->rect.width = brect.width - item[i]->rect.x;
		}
		else {
			item[iexpand]->rect.width = brect.width - w;
			for(i = 1; i < nitem; i++)
				item[i]->rect.x = item[i - 1]->rect.x + item[i - 1]->rect.width;
		}

		for(i = 0; i < nitem; i++) {
			d.color = item[i]->color;
			d.rect = item[i]->rect;
			d.data = item[i]->data;
			if(d.data && !strncmp(d.data, "%m:", 3))
				blitz_drawmeter(dpy, &d);
			else
				blitz_drawlabel(dpy, &d);
		}
	}
    XCopyArea(dpy, pmap, win, gc, 0, 0, brect.width, brect.height, 0, 0);
    XSync(dpy, False);
}

static void
update_geometry()
{
	char buf[64];
    brect = rect;
    brect.height = xfont->ascent + xfont->descent + 4;
    brect.y = rect.height - brect.height;
    XMoveResizeWindow(dpy, win, brect.x, brect.y, brect.width, brect.height);
    XSync(dpy, False);
    XFreePixmap(dpy, pmap);
    pmap = XCreatePixmap(dpy, win, brect.width, brect.height,
						 DefaultDepth(dpy, screen));
    XSync(dpy, False);
	snprintf(buf, sizeof(buf), "NewGeometry %d %d %d %d\n", brect.x, brect.y, brect.width, brect.height);
	do_pend_fcall(buf);
	draw();
}

static void
handle_buttonpress(XButtonPressedEvent * e)
{
	size_t i;
	char buf[32];
    for(i = 0; i < nitem; i++)
        if(blitz_ispointinrect(e->x, e->y, &item[i]->rect)) {
			snprintf(buf, sizeof(buf), "Button%dPress %d\n", e->button, i);
			do_pend_fcall(buf);
        }
}

static void
check_x_event(IXPConn *c)
{
    XEvent e;

    while(XPending(dpy)) {
        XNextEvent(dpy, &e);
        switch (e.type) {
        case ButtonPress:
            handle_buttonpress(&e.xbutton);
            break;
        case Expose:
            if(e.xexpose.count == 0)
                draw();
            break;
        default:
            break;
        }
    }
}

static int
index_of_id(unsigned short id)
{
	int i;
	for(i = 0; i < nitem; i++)
		if(item[i]->id == id)
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
	static char buf[32];

	if(id && ((i = index_of_id(id)) == -1))
		return nil;
	switch(type) {
		case Droot: return "/"; break;
		case Ditem:
			snprintf(buf, sizeof(buf), "%u", i + 1);
			return buf;
			break;
		case Fctl: return "ctl"; break;
		case Ffont: return "font"; break;
		case Fdefcolor: return "defcolor"; break;
		case Fexpand: return "expand"; break;
		case Fdata: return "data"; break;
		case Fevent: return "event"; break;
		case Fcolor: return "color"; break;
		default: return nil; break;
	}
}

static int
name_to_type(char *name)
{
	const char *err;
    unsigned int i;
	if(!name || !name[0] || !strncmp(name, "/", 2) || !strncmp(name, "..", 3))
		return Droot;
	if(!strncmp(name, "new", 4))
		return Ditem;
	if(!strncmp(name, "ctl", 4))
		return Fctl;
	if(!strncmp(name, "font", 5))
		return Ffont;
	if(!strncmp(name, "defcolor", 9))
		return Fdefcolor;
	if(!strncmp(name, "expand", 7))
		return Fexpand;
	if(!strncmp(name, "data", 5))
		return Fdata;
	if(!strncmp(name, "event", 6))
		return Fevent;
	if(!strncmp(name, "color", 6))
		return Fcolor;
   	i = (unsigned short) cext_strtonum(name, 1, 0xffff, &err);
    if(!err && (i - 1 <= nitem))
		return Ditem;
	return -1;
}

static int
mkqid(Qid *dir, char *wname, Qid *new, Bool iswalk)
{
	const char *err;
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
	case Ditem:
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "new", 4)) {
			/*fprintf(stderr, "mkqid iswalk=%d, wname=%s\n", iswalk, wname);*/
			if(iswalk)
				new->path = mkqpath(Ditem, new_item()->id);
			else
				new->path = mkqpath(Ditem, 0);
		}
		else {
			i = cext_strtonum(wname, 1, 0xffff, &err);
			if(err || (i - 1 >= nitem))
				return -1;
			new->path = mkqpath(Ditem, item[i - 1]->id);
		}
		break;
	case Fdata:
	case Fcolor:
		if(i >= nitem)
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
	char buf[16];

	if(id && ((i = index_of_id(id)) == -1))
		return 0;
    switch (type) {
    case Droot:
    case Ditem:
		return mkstat(stat, dir, name, 0, DMDIR | DMREAD | DMEXEC);
        break;
	case Fctl:
		return mkstat(stat, dir, name, 0, DMWRITE);
		break;
    case Fevent:
		return mkstat(stat, dir, name, 0, DMREAD);
		break;
    case Ffont:
		return mkstat(stat, dir, name, strlen(font), DMREAD | DMWRITE);
        break;
    case Fdefcolor:
		return mkstat(stat, dir, name, 23, DMREAD | DMWRITE);
        break;
    case Fexpand:
		snprintf(buf, sizeof(buf), "%u", iexpand + 1);
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
		break;
    case Fdata:
		return mkstat(stat, dir, name, (i == nitem) ? 0 : strlen(item[i]->data), DMREAD | DMWRITE);
		break;	
    case Fcolor:
		return mkstat(stat, dir, name, 23, DMREAD | DMWRITE);
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
	if((qpath_type(m->qid.path) == Ditem) && (i < nitem)) {
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
    XSetWindowAttributes wa;

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
        fprintf(stderr, "%s", "wmiibar: cannot open display\n");
        exit(1);
    }
    XSetErrorHandler(dummy_error_handler);
    screen = DefaultScreen(dpy);

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

    /* default settings */
	cext_strlcpy(defcolstr, BLITZ_SEL_COLOR, sizeof(defcolstr));
	blitz_loadcolor(dpy, screen, defcolstr, &defcolor);

	/* X stuff */
    font = strdup(BLITZ_FONT);
    xfont = blitz_getfont(dpy, font);
    wa.override_redirect = 1;
    wa.background_pixmap = ParentRelative;
    wa.event_mask = ExposureMask | ButtonPressMask
					| SubstructureRedirectMask | SubstructureNotifyMask;

    rect.x = rect.y = 0;
    rect.width = DisplayWidth(dpy, screen);
    rect.height = DisplayHeight(dpy, screen);
    brect = rect;
    brect.height = xfont->ascent + xfont->descent + 4;
    brect.y = rect.height - brect.height;

    win = XCreateWindow(dpy, RootWindow(dpy, screen), brect.x, brect.y,
                        brect.width, brect.height, 0, DefaultDepth(dpy, screen),
                        CopyFromParent, DefaultVisual(dpy, screen),
                        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    XDefineCursor(dpy, win, XCreateFontCursor(dpy, XC_left_ptr));
    XSync(dpy, False);

    gc = XCreateGC(dpy, win, 0, 0);

    pmap = XCreatePixmap(dpy, win, brect.width, brect.height,
                      	 DefaultDepth(dpy, screen));

	/* main loop */
	XMapRaised(dpy, win);
	draw();

	errstr = ixp_server_loop(&srv);
	if(errstr)
    	fprintf(stderr, "wmiibar: fatal: %s\n", errstr);

	/* cleanup */
	ixp_server_close(&srv);
	XCloseDisplay(dpy);

	return errstr ? 1 : 0;
}
