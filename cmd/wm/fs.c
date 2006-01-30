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

#include "ixp.h"
#include "blitz.h"

/*
 * filesystem specification
 * / 					Droot
 * /default/			Ddefault
 * /default/font		Ffont		<xlib font name>
 * /default/selcolor	Fcolor3		<#RRGGBB> <#RRGGBB> <#RRGGBB>
 * /default/normcolor	Fcolor3		<#RRGGBB> <#RRGGBB> <#RRGGBB>
 * /default/transcolor	Fcolor		<#RRGGBB>
 * /default/border		Fborder		0..n
 * /default/title 		Fbool  		0, 1
 * /default/snap 		Fsnap  		0..n
 * /event				Fevent
 * /ctl					Fctl 		command interface (root)
 * /new/				Dpage		returns new page
 * /sel/				Dpage		sel page
 * /1/					Dpage		page
 * /1/ctl				Fctl		command interface (page)
 * /1/float/			Dfloat
 * /1/float/sel/		Dclient
 * /1/float/1/			Dclient
 * /1/float/1/border	Fborder		0..n
 * /1/float/1/title		Ftitle		0, 1
 * /1/float/1/name		Fname		name of client
 * /1/float/1/ctl 		Fctl 		command interface (client)
 * /1/col/				Dcolroot
 * /1/col/sel/			Dcol
 * /1/col/1/			Dcol
 * /1/col/1/sel/		Dclient
 * /1/col/1/1/			Dclient
 * /1/col/1/1/border	Fborder		0..n
 * /1/col/1/1/title		Ftitle		0, 1
 * /1/col/1/1/name		Fname		name of client
 * /1/col/1/1/ctl 		Fctl 		command interface (client)
 * /1/col/1/ctl 		Fctl 		command interface (col)
 */

/* 8-bit qid.path.type */
enum {                          
    Droot,
	Ddefault,
	Dpage,
	Dfloat,
	Dcolroot,
	Dcol,
	Dclient,
    Ffont,
	Fcolor3,
	Fcolor,
	Fborder,
	Fsnap,
	Ftitle,
	Fevent,
	Fctl,
	Fname
};

static char E9pversion[] = "9P version not supported";
static char Enoperm[] = "permission denied";
static char Enofid[] = "fid not assigned";
static char Enofile[] = "file not found";
static char Enomode[] = "mode not supported";
static char Enofunc[] = "function not supported";

static unsigned char *msg[IXP_MAX_MSG];
char *errstr = 0;
static Qid root_qid;

/* IXP stuff */

static unsigned long long
mkqpath(unsigned char type, unsigned char level, unsigned short item)
{
    return ((unsigned long long) item << 16) | ((unsigned long long) level << 8) | (unsigned long long) type;
}

static unsigned char
qpath_type(unsigned long long path)
{
    return path & 0xff;
}

static unsigned char
qpath_level(unsigned long long path)
{
    return (path >> 8) & 0xffff;
}

static unsigned short
qpath_item(unsigned long long path)
{
    return (path >> 16) & 0xffffff;
}

static char *
qid_to_name(Qid *qid)
{
	unsigned char type = qpath_type(qid->path);
	unsigned char level = qpath_level(qid->path);
	unsigned short i = qpath_item(qid->path);
	static char buf[32];

	switch(type) {
		case Droot: return "/"; break;
		case Ddefault: return "default"; break;
		case Dpage:
		case Dcol:
		case Dclient:
			if(!level && (i == nitem)) /* only for page */
				return "new";
			snprintf(buf, sizeof(buf), "%u", i);
			return buf;
			break;
		case Fctl: return "ctl"; break;
		case Ffont: return "font"; break;
		case Fcolor3: return "color"; break;
		case Fcolor: return "color"; break;
		case Fborder: return "border"; break;
		case Fsnap: return "border"; break;
		case Ftitle: return "title"; break;
		case Fname: return "name"; break;
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
	if(!strncmp(name, "expand", 7))
		return Fexpand;
	if(!strncmp(name, "data", 5))
		return Fdata;
	if(!strncmp(name, "event", 6))
		return Fevent;
	if(!strncmp(name, "color", 6))
		return Fcolor;
   	i = (unsigned short) cext_strtonum(name, 1, 0xffff, &err);
    if(!err && (i <= nitem))
		return Ditem;
	return -1;
}

static int
mkqid(Qid *dir, char *wname, Qid *new)
{
	const char *err;
	int type = name_to_type(wname);
	unsigned short i = qpath_item(dir->path);

    if((dir->type != IXP_QTDIR) || (type == -1))
        return -1;
	
    new->version = 0;
	switch(type) {
	case Droot:
		new->type = IXP_QTDIR;
		*new = root_qid;
		break;
	case Ditem:
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "new", 4))
			new->path = mkqpath(Ditem, nitem);
		else {
			unsigned short i = cext_strtonum(wname, 1, 0xffff, &err);
			if(err || (i >= nitem))
				return -1;
			new->path = mkqpath(Ditem, i);
		}
		break;
	case Fdata: /* note, Fdata needs to be before Fcolor, fallthrough */
		if(!i)
			return -1;
	case Fcolor:
		if(i > nitem)
			return -1;
	default:
		new->type = IXP_QTFILE;
    	new->path = mkqpath(type, i);
		break;
	}
    return 0;
}

static int
xversion(IXPConn *c)
{
    if(strncmp(c->fcall->version, IXP_VERSION, strlen(IXP_VERSION))) {
        errstr = E9pversion;
        return -1;
    } else if(c->fcall->maxmsg > IXP_MAX_MSG)
        c->fcall->maxmsg = IXP_MAX_MSG;
    c->fcall->id = RVERSION;
    return 0;
}

static int
xattach(IXPConn *c)
{
    IXPMap *new = cext_emallocz(sizeof(IXPMap));
    new->qid = root_qid;
    new->fid = c->fcall->fid;
	c->map = (IXPMap **)cext_array_attach((void **)c->map, new,
					sizeof(IXPMap *), &c->mapsz);
    c->fcall->id = RATTACH;
    c->fcall->qid = root_qid;
    return 0;
}

static int
xwalk(IXPConn *c)
{
    unsigned short nwqid = 0;
    Qid dir = root_qid;
    IXPMap *m;

    if(!(m = ixp_server_fid2map(c, c->fcall->fid))) {
        errstr = Enofid;
        return -1;
    }
    if(c->fcall->fid != c->fcall->newfid
       && (ixp_server_fid2map(c, c->fcall->newfid))) {
        errstr = Enofid;
        return -1;
    }
    if(c->fcall->nwname) {
        dir = m->qid;
        for(nwqid = 0; (nwqid < c->fcall->nwname)
            && !mkqid(&dir, c->fcall->wname[nwqid], &c->fcall->wqid[nwqid]); nwqid++)
            dir = c->fcall->wqid[nwqid];
        if(!nwqid) {
            errstr = Enofile;
            return -1;
        }
    }
    /* a fid will only be valid, if the walk was complete */
    if(nwqid == c->fcall->nwname) {
        if(c->fcall->fid != c->fcall->newfid) {
			m = cext_emallocz(sizeof(IXPMap));
			c->map = (IXPMap **)cext_array_attach((void **)c->map, m,
							sizeof(IXPMap *), &c->mapsz);
        }
        m->qid = dir;
        m->fid = c->fcall->newfid;
    }
    c->fcall->id = RWALK;
    c->fcall->nwqid = nwqid;
    return 0;
}

static int
xopen(IXPConn *c)
{
    IXPMap *m = ixp_server_fid2map(c, c->fcall->fid);

    if(!m) {
        errstr = Enofid;
        return -1;
    }
    if(!(c->fcall->mode | IXP_OREAD) && !(c->fcall->mode | IXP_OWRITE)) {
        errstr = Enomode;
        return -1;
    }
    c->fcall->id = ROPEN;
    c->fcall->qid = m->qid;
    c->fcall->iounit = 256;
    return 0;
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
    mkqid(dir, name, &stat->qid);
	return ixp_sizeof_stat(stat);
}

static unsigned int
type_to_stat(Stat *stat, char *name, unsigned short i)
{
	int type = name_to_type(name);
	Qid dir = {IXP_QTDIR, 0, mkqpath(Ditem, i)};
	char buf[16];

    switch (type) {
    case Droot:
    case Ditem:
		return mkstat(stat, &root_qid, name, 0, DMDIR | DMREAD | DMEXEC);
        break;
	case Fctl:
		return mkstat(stat, &root_qid, name, 0, DMWRITE);
		break;
    case Fevent:
		return mkstat(stat, &root_qid, name, 0, DMREAD);
		break;
    case Ffont:
		return mkstat(stat, &root_qid, name, strlen(font),
						DMREAD | DMWRITE);
        break;
    case Fexpand:
		snprintf(buf, sizeof(buf), "%u", iexpand);
		return mkstat(stat, &root_qid, name, strlen(buf), DMREAD | DMWRITE);
		break;
    case Fdata:
		if(i == nitem)
			i = 0;
		return mkstat(stat, &dir, name, strlen(item[i]->data),
						DMREAD | DMWRITE);
		break;	
    case Fcolor:
		if(i == nitem)
			i = 0;
		return mkstat(stat, &dir, name, strlen(item[i]->color),
						DMREAD | DMWRITE);
		break;
    default:
		errstr = "invalid stat";
		break;
    }
	return 0;
}

static int
xremove(IXPConn *c)
{
    IXPMap *m = ixp_server_fid2map(c, c->fcall->fid);
	unsigned short i;

    if(!m) {
        errstr = Enofid;
        return -1;
    }
	i = qpath_item(m->qid.path);
	if((qpath_type(m->qid.path) == Ditem) && i && (i < nitem)) {
		Item *it = item[i];
		detach_item(it);
		free(it);
    	c->fcall->id = RREMOVE;
		if(iexpand >= nitem)
			iexpand = 0;
		draw();
		return 0;
	}
	errstr = Enoperm;
	return -1;
}

static int
xread(IXPConn *c)
{
	Stat stat;
    IXPMap *m = ixp_server_fid2map(c, c->fcall->fid);
    unsigned char *p = c->fcall->data;
	unsigned short i;
	unsigned int len;
	char buf[32];

    if(!m) {
        errstr = Enofid;
        return -1;
    }
	i = qpath_item(m->qid.path);

	c->fcall->count = 0;
	if(c->fcall->offset) {
		switch (qpath_type(m->qid.path)) {
		case Droot:
			/* jump to offset */
			len = type_to_stat(&stat, "ctl", 0);
			len += type_to_stat(&stat, "font", 0);
			len += type_to_stat(&stat, "color", 0);
			len += type_to_stat(&stat, "new", 0);
			len += type_to_stat(&stat, "event", 0);
			for(i = 1; i < nitem; i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len += type_to_stat(&stat, buf, i);
				if(len <= c->fcall->offset)
					continue;
				else 
					break;
			}
			/* offset found, proceeding */
			for(; i < nitem; i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, i);
				if(c->fcall->count + len > c->fcall->iounit)
					break;
				c->fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Fevent:
			return 1;
			break;
		default:
			break;
		}
	}
	else {
		switch (qpath_type(m->qid.path)) {
		case Droot:
			c->fcall->count = type_to_stat(&stat, "ctl", 0);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "font", 0);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "color", 0);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "new", 0);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "event", 0);
			p = ixp_enc_stat(p, &stat);
			for(i = 1; i < nitem; i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, i);
				if(c->fcall->count + len > c->fcall->iounit)
					break;
				c->fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Ditem:
			if(i > nitem)
				goto error_xread;
			if(i == nitem)
				new_item();
			c->fcall->count = type_to_stat(&stat, "color", i);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "data", i);
			p = ixp_enc_stat(p, &stat);
			break;
		case Fctl:
			errstr = Enoperm;
			return -1;
			break;
		case Ffont:
			if((c->fcall->count = strlen(font)))
				memcpy(p, font, c->fcall->count);
			break;
		case Fexpand:
			snprintf(buf, sizeof(buf), "%u", iexpand);
			c->fcall->count = strlen(buf);
			memcpy(p, buf, c->fcall->count);
			break;
		case Fevent:
			return 1;
			break;
		case Fdata:
			if(i == nitem)
				new_item();
			if(i >= nitem)
				goto error_xread;
			if((c->fcall->count = strlen(item[i]->data)))
				memcpy(p, item[i]->data, c->fcall->count);
			break;
		case Fcolor:
			if(i == nitem)
				new_item();
			if(i >= nitem)
				goto error_xread;
			if((c->fcall->count = strlen(item[i]->color)))
				memcpy(p, item[i]->color, c->fcall->count);
			break;
		default:
	error_xread:
			if(!errstr)
				errstr = "invalid read";
			return -1;
			break;
		}
	}
	c->fcall->id = RREAD;
    return 0;
}

static int
xstat(IXPConn *c)
{
    IXPMap *m = ixp_server_fid2map(c, c->fcall->fid);
	char *name;

    if(!m) {
        errstr = Enofid;
        return -1;
    }

	name = qid_to_name(&m->qid);
	if(!type_to_stat(&c->fcall->stat, name, qpath_item(m->qid.path)))
		return -1;
    c->fcall->id = RSTAT;
    return 0;
}

static int
xwrite(IXPConn *c)
{
	char buf[256];
    IXPMap *m = ixp_server_fid2map(c, c->fcall->fid);
	unsigned short i;

    if(!m) {
        errstr = Enofid;
        return -1;
    }

	i = qpath_item(m->qid.path);
	switch (qpath_type(m->qid.path)) {
	case Fctl:
		if(c->fcall->count == 4) {
			memcpy(buf, c->fcall->data, 4);
			buf[4] = 0;
			if(!strncmp(buf, "quit", 5)) {
				srv.running = 0;
				break;
			}
		}
		errstr = "command not supported";
		return -1;
		break;
	case Ffont:
		if(font)
			free(font);
		font = cext_emallocz(c->fcall->count + 1);
		memcpy(font, c->fcall->data, c->fcall->count);
    	xfont = blitz_getfont(dpy, font);
		update_geometry();
		break;
    case Fexpand:
		{
			const char *err;
			if(c->fcall->count && c->fcall->count < 16) {
				memcpy(buf, c->fcall->data, c->fcall->count);
				buf[c->fcall->count] = 0;
				i = (unsigned short) cext_strtonum(buf, 1, 0xffff, &err);
				if(i < nitem) {
					iexpand = i;
					draw();
					break;
				}
			}
		}
		errstr = "item not found";
		return -1;
		break;
	case Fdata:
		{
			unsigned int len = c->fcall->count;
			if(i == nitem)
				new_item();
			if(!i || (i >= nitem))
				goto error_xwrite;
			if(len >= sizeof(item[i]->data))
				len = sizeof(item[i]->data) - 1;
			memcpy(item[i]->data, c->fcall->data, len);
			item[i]->data[len] = 0;
			draw();
		}
		break;
	case Fcolor:
		if(i == nitem)
			new_item();
		if((i >= nitem) || (c->fcall->count != 24)
			|| (c->fcall->data[0] != '#') || (c->fcall->data[8] != '#')
		    || (c->fcall->data[16] != '#'))
		{
			errstr = "wrong color format";
			goto error_xwrite;
		}
		memcpy(item[i]->color, c->fcall->data, c->fcall->count);
		item[i]->color[c->fcall->count] = 0;
		update_color(item[i]);
		draw();
		break;
	default:
error_xwrite:
		if(!errstr)
			errstr = "invalid write";
		return -1;
		break;
	}
    c->fcall->id = RWRITE;
	return 0;
}

static int
xclunk(IXPConn *c)
{
    IXPMap *m = ixp_server_fid2map(c, c->fcall->fid);

    if(!m) {
        errstr = Enofid;
        return -1;
    }
	cext_array_detach((void **)c->map, m, &c->mapsz);
    free(m);
    c->fcall->id = RCLUNK;
    return 0;
}

static void
close_ixp_conn(IXPServer *s, IXPConn *c)
{
	size_t i;
	cext_array_detach((void **)s->conn, c, &s->connsz);
	if(c->map) {
		for(i = 0; (i < c->mapsz) && c->map[i]; i++)
			free(c->map[i]);
		free(c->map);
	}
	if(c->pend) {
		for(i = 0; (i < c->pendsz) && c->pend[i]; i++)
			free(c->pend[i]);
		free(c->pend);
	}
	shutdown(c->fd, SHUT_RDWR);
	close(c->fd);
	free(c);
}

static void
do_fcall(IXPServer *s, IXPConn *c)
{
    unsigned int msize;
	int ret = -1;
    errstr = 0;
	if(!(msize = ixp_recv_message(c->fd, msg, IXP_MAX_MSG, &errstr))) {
		close_ixp_conn(s, c);
		return;
	}
	if(!c->fcall)
		c->fcall = cext_emallocz(sizeof(Fcall));
    if((msize = ixp_msg_to_fcall(msg, IXP_MAX_MSG, c->fcall))) {
		switch(c->fcall->id) {
		case TVERSION: ret = xversion(c); break;
		case TATTACH: ret = xattach(c); break;
		case TWALK: ret = xwalk(c); break;
		case TREMOVE: ret = xremove(c); break;
		case TOPEN: ret = xopen(c); break;
		case TREAD: ret = xread(c); break;
		case TWRITE: ret = xwrite(c); break;
		case TCLUNK: ret = xclunk(c); break;
		case TSTAT: ret = xstat(c); break;
		default:
			break;
		}
	}
	if(ret == -1) {
		if(!errstr)
			errstr = Enofunc;
		c->fcall->id = RERROR;
		cext_strlcpy(c->fcall->errstr, errstr, sizeof(c->fcall->errstr));
	}
	else if(ret == 1) {
		c->pend = (Fcall **)cext_array_attach((void **)c->pend, c->fcall, sizeof(Fcall *), &c->pendsz);
		c->fcall = nil;
		return;	 /* response asynchroneously */
	}
	msize = ixp_fcall_to_msg(c->fcall, msg, IXP_MAX_MSG);
	if(ixp_send_message(c->fd, msg, msize, &errstr) != msize)
		close_ixp_conn(s, c);
}

static Fcall *
pending(IXPConn *c, unsigned char id)
{
	size_t i;
	for(i = 0; (i < c->pendsz) && c->pend[i]; i++)
		if(c->pend[i]->id == id)
			return c->pend[i];
	return nil;
}

static void
do_pend_fcall(char *event)
{
	size_t i;
	Fcall *fcall;
	for(i = 0; (i < srv.connsz) && srv.conn[i]; i++) {
		IXPConn *c = srv.conn[i];
		/* all pending TREADs are on /event, so no qid checking necessary */
		while((fcall = pending(c, TREAD))) {
			IXPMap *m = ixp_server_fid2map(c, fcall->fid);
			unsigned char *p = fcall->data;
			unsigned int msize;

			if(!m) {
				errstr = Enofid;
				fcall->id = RERROR;
			}
			else if(qpath_type(m->qid.path) == Fevent) {
				fcall->count = strlen(event);
				memcpy(p, event, fcall->count);
				fcall->id = RREAD;
			}
			msize = ixp_fcall_to_msg(fcall, msg, IXP_MAX_MSG);
			/* remove pending request */
			cext_array_detach((void **)c->pend, fcall, &c->pendsz);
			free(fcall);
			if(ixp_send_message(c->fd, msg, msize, &errstr) != msize) {
				close_ixp_conn(&srv, c);
				break;
			}
		}
	}
}

static void
new_ixp_conn(IXPServer *s, IXPConn *c)
{
    IXPConn *new;
	int fd = ixp_accept_sock(c->fd);
	
	if(fd >= 0) {
		new = cext_emallocz(sizeof(IXPConn));
		new->fd = fd;
		new->read = do_fcall;
		new->close = close_ixp_conn;
		s->conn = (IXPConn **)cext_array_attach((void **)s->conn, new,
					sizeof(IXPConn *), &s->connsz);
	}
}
