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

#include "wm.h"

/*
 * filesystem specification
 * / 					Droot
 * /default/			Ddefault
 * /default/font		Ffont		<xlib font name>
 * /default/selcolor	Fselcolor	<#RRGGBB> <#RRGGBB> <#RRGGBB>
 * /default/normcolor	Fnormcolor	<#RRGGBB> <#RRGGBB> <#RRGGBB>
 * /default/border		Fborder		0..n
 * /default/title 		Fbool  		0, 1
 * /default/snap 		Fsnap  		0..n
 * /event				Fevent
 * /ctl					Fctl 		command interface (root)
 * /new/				Dpage		returns new page
 * /sel/				Dpage		sel page
 * /1/					Dpage		page
 * /1/ctl				Fctl		command interface (page)
 * /1/sel/				Dclient
 * /1/1/				Dclient
 * /1/1/border			Fborder		0..n
 * /1/1/title			Ftitle		0, 1
 * /1/1/name			Fname		name of client
 * /1/1/ctl 			Fctl 		command interface (client)
 * /1/col/				Dcolroot
 * /1/col/new/			Dcol
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
	Dcolroot,
	Dcol,
	Dclient,
    Ffont,
	Fselcolor,
	Fnormcolor,
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
mkqpath(unsigned char type, unsigned short pg, unsigned short col, unsigned short cl)
{
    return ((unsigned long long) type << 48) | ((unsigned long long) pg << 32)
		| ((unsigned long long) col << 16) | (unsigned long long) cl;
}

static unsigned char
qpath_type(unsigned long long path)
{
    return (path >> 48) & 0xff;
}

static unsigned short
qpath_page(unsigned long long path)
{
    return (path >> 32) & 0xffff;
}

static unsigned short
qpath_col(unsigned long long path)
{
    return (path >> 16) & 0xffff;
}

static unsigned short
qpath_client(unsigned long long path)
{
    return path & 0xffff;
}

static char *
qid_to_name(Qid *qid)
{
	unsigned char typ = qpath_type(qid->path);
	unsigned short pg = qpath_page(qid->path);
	unsigned short col = qpath_col(qid->path);
	unsigned short cl = qpath_client(qid->path);
	static char buf[32];

	switch(typ) {
		case Droot: return "/"; break;
		case Ddefault: return "default"; break;
		case Dcolroot: return "col"; break;
		case Dpage:
			if(pg == sel_page)
				return "sel";
			if(pg == npage)
				return "new";
			snprintf(buf, sizeof(buf), "%u", pg);
			return buf;
			break;
		case Dcol:
			if(page[pg]->ncol == col)
				return "new";
			if(page[pg]->sel_col == col)
				return "sel";
			snprintf(buf, sizeof(buf), "%u", col);
			return buf;
			break;
		case Dclient:
			if(!col && page[pg]->sel_float == cl)
				return "sel";
			snprintf(buf, sizeof(buf), "%u", cl);
			return buf;
			break;
		case Fctl: return "ctl"; break;
		case Ffont: return "font"; break;
		case Fselcolor: return "selcolor"; break;
		case Fnormcolor: return "normcolor"; break;
		case Fborder: return "border"; break;
		case Fsnap: return "border"; break;
		case Ftitle: return "title"; break;
		case Fname: return "name"; break;
		case Fevent: return "event"; break;
		default: return nil; break;
	}
}

static int
name_to_type(char *name, unsigned char dtyp)
{
	const char *err;
    unsigned int i;
	if(!name || !name[0] || !strncmp(name, "/", 2) || !strncmp(name, "..", 3))
		return Droot;
	if(!strncmp(name, "new", 4)) {
		if(dtyp == Droot)
			return Dpage;
		else if(dtyp == Dpage)
			return Dcol;
	}
	if(!strncmp(name, "ctl", 4))
		return Fctl;
	if(!strncmp(name, "font", 5))
		return Ffont;
	if(!strncmp(name, "event", 6))
		return Fevent;
	if(!strncmp(name, "selcolor", 6))
		return Fselcolor;
	if(!strncmp(name, "normcolor", 6))
		return Fnormcolor;
	if(!strncmp(name, "snap", 5))
		return Fsnap;
	if(!strncmp(name, "name", 5))
		return Fname;
	if(!strncmp(name, "border", 7))
		return Fborder;
	if(!strncmp(name, "title", 6))
		return Ftitle;
	if(!strncmp(name, "col", 4))
		return Dcolroot;
	if(!strncmp(name, "sel", 6))
		goto dyndir;
   	i = (unsigned short) cext_strtonum(name, 1, 0xffff, &err);
    if(err)
		return -1;
dyndir:
	switch(dtyp) {
	case Droot: return Dpage; break;
	case Dcol:
	case Dpage: return Dclient; break;
	case Dcolroot: return Dcol; break;
	}
	return -1;
}

static int
mkqid(Qid *dir, char *wname, Qid *new)
{
	const char *err;
	unsigned char dtyp = qpath_type(dir->path);
	unsigned short dpg = qpath_page(dir->path);
	unsigned short dcol = qpath_col(dir->path);
	unsigned short dcl = qpath_client(dir->path);
	unsigned short i;
	int type = name_to_type(wname, dtyp);

    if((dir->type != IXP_QTDIR) || (type == -1))
        return -1;
	
    new->version = 0;
	switch(type) {
	case Droot:
		*new = root_qid;
		break;
	case Ddefault:
		new->type = IXP_QTDIR;
		new->path = mkqpath(Ddefault, 0, 0, 0);
		break;
	case Dcolroot:
		new->type = IXP_QTDIR;
		new->path = mkqpath(Dcolroot, dpg, 0, 0);
		break;
	case Dpage:
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "new", 4))
			new->path = mkqpath(Dpage, npage, 0, 0);
		else if(!strncmp(wname, "sel", 4))
			new->path = mkqpath(Dpage, sel_page, 0, 0);
		else {
			i = cext_strtonum(wname, 1, 0xffff, &err);
			if(err || (i >= npage))
				return -1;
			new->path = mkqpath(Dpage, i, 0, 0);
		}
		break;
	case Dcol:
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "new", 4))
			new->path = mkqpath(Dcol, dpg, page[dpg]->ncol, 0);
		else if(!strncmp(wname, "sel", 4))
			new->path = mkqpath(Dcol, dpg, page[dpg]->sel_col, 0);
		else {
			i = cext_strtonum(wname, 1, 0xffff, &err);
			if(err)
				return -1;
			new->path = mkqpath(Dcol, dpg, i, 0);
		}
		break;
	case Dclient:
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "new", 4)) {
			if(dcol)
				new->path = mkqpath(Dclient, dpg, dcol, page[dpg]->col[dcol]->nclient);
			else
				new->path = mkqpath(Dclient, dpg, dcol, page[dpg]->nfloat);
		}
		else if(!strncmp(wname, "sel", 4)) {
			if(dcol)
				new->path = mkqpath(Dclient, dpg, dcol, page[dpg]->col[dcol]->sel);
			else
				new->path = mkqpath(Dclient, dpg, dcol, page[dpg]->sel_float);
		}
		else {
			i = cext_strtonum(wname, 1, 0xffff, &err);
			if(err)
				return -1;
			new->path = mkqpath(Dclient, dpg, dcol, i);
		}
		break;
	default:
		new->type = IXP_QTFILE;
    	new->path = mkqpath(type, dpg, dcol, dcl);
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
type_to_stat(Stat *stat, char *name, Qid *dir, unsigned int idx)
{
	unsigned char dtyp = qpath_type(dir->path);
	unsigned short dpg = qpath_page(dir->path);
	unsigned short dcol = qpath_col(dir->path);
	int type = name_to_type(name, dtyp);
	char buf[16];

    switch (type) {
    case Droot:
    case Ddefault:
    case Dpage:
    case Dcolroot:
    case Dcol:
    case Dclient:
		return mkstat(stat, dir, name, 0, DMDIR | DMREAD | DMEXEC);
        break;
	case Fctl:
		return mkstat(stat, dir, name, 0, DMWRITE);
		break;
    case Fevent:
		return mkstat(stat, dir, name, 0, DMREAD);
		break;
    case Ffont:
		return mkstat(stat, dir, name, strlen(def.font), DMREAD | DMWRITE);
        break;
    case Fselcolor:
		return mkstat(stat, dir, name, 24, DMREAD | DMWRITE);
		break;
    case Fnormcolor:
		return mkstat(stat, dir, name, 24, DMREAD | DMWRITE);
		break;
    case Fborder:
		if(dtyp == Ddefault)
			snprintf(buf, sizeof(buf), "%d", def.border);
		else {
			if(dcol) {
				Column *col = page[dpg]->col[dcol];
				snprintf(buf, sizeof(buf), "%d", col->client[idx]->frame.border);
			}
			else
				snprintf(buf, sizeof(buf), "%d", page[dpg]->floatc[idx]->frame.border);
		}
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
        break;
    case Ftitle:
		if(dtyp == Ddefault)
			snprintf(buf, sizeof(buf), "%d", def.title);
		else {
			if(dcol) {
				Column *col = page[dpg]->col[dcol];
				snprintf(buf, sizeof(buf), "%d", col->client[idx]->frame.title);
			}
			else
				snprintf(buf, sizeof(buf), "%d", page[dpg]->floatc[idx]->frame.title);
		}
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
        break;
    case Fsnap:
		snprintf(buf, sizeof(buf), "%d", def.snap);
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
		break;
    case Fname:
		if(dcol) {
			Column *col = page[dpg]->col[dcol];
			return mkstat(stat, dir, name, strlen(col->client[idx]->name), DMREAD);
		}
		else
			return mkstat(stat, dir, name, strlen(page[dpg]->floatc[idx]->name), DMREAD | DMWRITE);
        break;
    default:
		errstr = "invalid stat";
		break;
    }
	return 0;
}

static int
xread(IXPConn *c)
{
	Stat stat;
    IXPMap *m = ixp_server_fid2map(c, c->fcall->fid);
    unsigned char typ, *p = c->fcall->data;
	unsigned short pg, col, cl;
	unsigned int i, len;
	Qid dir = root_qid;
	char buf[32];

    if(!m) {
        errstr = Enofid;
        return -1;
    }
	typ = qpath_type(m->qid.path);
	pg = qpath_page(m->qid.path);
	col = qpath_col(m->qid.path);
	cl = qpath_client(m->qid.path);

	c->fcall->count = 0;
	if(c->fcall->offset) {
		switch (qpath_type(m->qid.path)) {
		case Droot:
			/* jump to offset */
			len = type_to_stat(&stat, "ctl", &dir, 0);
			len += type_to_stat(&stat, "event", &dir, 0);
			len += type_to_stat(&stat, "default", &dir, 0);
			len += type_to_stat(&stat, "sel", &dir, 0);
			len += type_to_stat(&stat, "new", &dir, 0);
			for(i = 1; i < npage; i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len += type_to_stat(&stat, buf, &dir, 0);
				if(len <= c->fcall->offset)
					continue;
				else 
					break;
			}
			/* offset found, proceeding */
			for(; i < npage; i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, &dir, 0);
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
			c->fcall->count = type_to_stat(&stat, "ctl", &dir, 0);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "event", &dir, 0);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "default", &dir, 0);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "sel", &dir, 0);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "new", &dir, 0);
			p = ixp_enc_stat(p, &stat);
			for(i = 1; i < npage; i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, &dir, 0);
				if(c->fcall->count + len > c->fcall->iounit)
					break;
				c->fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Ddefault:
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
