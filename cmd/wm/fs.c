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
 * /default/title 		Ftitle 		0, 1
 * /default/snap 		Fsnap  		0..n
 * /default/inc 		Finc   		0..n
 * /event				Fevent
 * /ctl					Fctl 		command interface (root)
 * /new/				Dpage		returns new page
 * /sel/				Dpage		sel page
 * /1/					Dpage		page
 * /1/ctl				Fctl		command interface (page)
 * /1/sel/				Darea
 * /1/new/				Darea
 * /1/float/			Darea		floating clients in area 0
 * /1/float/ctl 		Fctl		command interface (area)
 * /1/float/sel/		Dclient
 * /1/float/1/			Dclient
 * /1/float/1/border	Fborder		0..n
 * /1/float/1/title		Ftitle		0, 1
 * /1/float/1/inc  		Finc  		0, 1
 * /1/float/1/name		Fname		name of client
 * /1/float/1/geom		Fgeom		geometry of client
 * /1/float/1/ctl 		Fctl 		command interface (client)
 * /1/1/				Darea
 * /1/1/ctl 			Fctl 		command interface (area)
 * /1/1/1/sel/			Dclient
 * /1/1/1/1/			Dclient
 * /1/1/1/border		Fborder		0..n
 * /1/1/1/title			Ftitle		0, 1
 * /1/1/1/inc  			Finc  		0, 1
 * /1/1/1/name			Fname		name of client
 * /1/1/1/geom			Fgeom		geometry of client
 * /1/1/1/ctl 			Fctl 		command interface (client)
 */

static char E9pversion[] = "9P version not supported";
static char Enoperm[] = "permission denied";
static char Enofid[] = "fid not assigned";
static char Enofile[] = "file not found";
static char Enomode[] = "mode not supported";
static char Enofunc[] = "function not supported";
/*static char Enocommand[] = "command not supported";*/

const char *err;
static unsigned char *msg[IXP_MAX_MSG];

/* IXP stuff */

unsigned long long
mkqpath(unsigned char type, unsigned short pg, unsigned short area, unsigned short cl)
{
    return ((unsigned long long) type << 48) | ((unsigned long long) pg << 32)
		| ((unsigned long long) area << 16) | (unsigned long long) cl;
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
qpath_area(unsigned long long path)
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
	unsigned short area = qpath_area(qid->path);
	unsigned short cl = qpath_client(qid->path);
	static char buf[32];

	switch(typ) {
		case Droot: return "/"; break;
		case Ddefault: return "default"; break;
		case Dpage:
			if(pg == sel)
				return "sel";
			if(pg == npage)
				return "new";
			snprintf(buf, sizeof(buf), "%u", pg);
			return buf;
			break;
		case Darea:
			if(!area) {
				if(page[pg]->sel)
					return "float";
				else
					return "sel";
			}
			if(page[pg]->narea == area)
				return "new";
			if(page[pg]->sel == area)
				return "sel";
			snprintf(buf, sizeof(buf), "%u", area);
			return buf;
			break;
		case Dclient:
			if(page[pg]->area[area]->sel == cl)
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
		case Finc: return "inc"; break;
		case Fgeom: return "geometry"; break;
		case Fname: return "name"; break;
		case Fevent: return "event"; break;
		default: return nil; break;
	}
}

static int
name_to_type(char *name, unsigned char dtyp)
{
    unsigned int i;
	if(!name || !name[0] || !strncmp(name, "/", 2) || !strncmp(name, "..", 3))
		return Droot;
	if(!strncmp(name, "new", 4)) {
		if(dtyp == Droot)
			return Dpage;
		else if(dtyp == Dpage)
			return Darea;
	}
	if(!strncmp(name, "default", 8))
		return Ddefault;
	if(!strncmp(name, "ctl", 4))
		return Fctl;
	if(!strncmp(name, "font", 5))
		return Ffont;
	if(!strncmp(name, "event", 6))
		return Fevent;
	if(!strncmp(name, "selcolor", 9))
		return Fselcolor;
	if(!strncmp(name, "normcolor", 10))
		return Fnormcolor;
	if(!strncmp(name, "snap", 5))
		return Fsnap;
	if(!strncmp(name, "name", 5))
		return Fname;
	if(!strncmp(name, "border", 7))
		return Fborder;
	if(!strncmp(name, "title", 6))
		return Ftitle;
	if(!strncmp(name, "inc", 4))
		return Finc;
	if(!strncmp(name, "geometry", 9))
		return Fgeom;
	if(!strncmp(name, "sel", 4))
		goto dyndir;
   	i = (unsigned short) cext_strtonum(name, 0, 0xffff, &err);
    if(err)
		return -1;
dyndir:
	switch(dtyp) {
	case Droot: return Dpage; break;
	case Dpage: return Darea; break;
	case Darea: return Dclient; break;
	}
	return -1;
}

static int
mkqid(Qid *dir, char *wname, Qid *new)
{
	unsigned char dtyp = qpath_type(dir->path);
	unsigned short dpg = qpath_page(dir->path);
	unsigned short darea = qpath_area(dir->path);
	unsigned short dcl = qpath_client(dir->path);
	unsigned short i;
	int type = name_to_type(wname, dtyp);

    if((dir->type != IXP_QTDIR) || (type == -1))
        return -1;
	
	new->dtype = dtyp;
    new->version = 0;
	switch(type) {
	case Droot:
		*new = root_qid;
		break;
	case Ddefault:
		new->type = IXP_QTDIR;
		new->path = mkqpath(Ddefault, 0, 0, 0);
		break;
	case Dpage:
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "new", 4))
			new->path = mkqpath(Dpage, npage, 0, 0);
		else if(!strncmp(wname, "sel", 4)) {
			if(!npage)
				return -1;
			new->path = mkqpath(Dpage, sel, 0, 0);
		}
		else {
			i = cext_strtonum(wname, 0, 0xffff, &err);
			if(err || (i >= npage))
				return -1;
			new->path = mkqpath(Dpage, i, 0, 0);
		}
		break;
	case Darea:
		fprintf(stderr, "mkqid(): %s\n", "Darea");
		if(!npage)
			return -1;
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "new", 4))
			new->path = mkqpath(Darea, dpg, page[dpg]->narea, 0);
		else if(!strncmp(wname, "sel", 4))
			new->path = mkqpath(Darea, dpg, page[dpg]->sel, 0);
		else {
			i = cext_strtonum(wname, 0, 0xffff, &err);
			if(err || (i >= page[dpg]->narea))
				return -1;
			new->path = mkqpath(Darea, dpg, i, 0);
		}
		break;
	case Dclient:
		if(!npage)
			return -1;
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "sel", 4))
			new->path = mkqpath(Dclient, dpg, darea, page[dpg]->area[darea]->sel);
		else {
			i = cext_strtonum(wname, 0, 0xffff, &err);
			if(err || (i >= page[dpg]->area[darea]->nclient))
				return -1;
			new->path = mkqpath(Dclient, dpg, darea, i);
		}
		break;
	default:
		new->type = IXP_QTFILE;
    	new->path = mkqpath(type, dpg, darea, dcl);
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
		{
			fprintf(stderr, "dir.dtype=%d\n", dir.dtype);
            dir = c->fcall->wqid[nwqid];
			fprintf(stderr, "walk: qid_to_name()=%s qpath_type(dir.path)=%d dir.dtype=%d\n",
							qid_to_name(&dir), qpath_type(dir.path), dir.dtype);
		}
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
    c->fcall->iounit = 2048;
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
type_to_stat(Stat *stat, char *name, Qid *dir)
{
	unsigned char dtyp = qpath_type(dir->path);
	unsigned short dpg = qpath_page(dir->path);
	unsigned short darea = qpath_area(dir->path);
	unsigned int idx;
	int type = name_to_type(name, dtyp);
	char buf[32];
	Client *c;

    switch (type) {
    case Dclient:
    case Darea:
    case Dpage:
    case Ddefault:
    case Droot:
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
			idx = cext_strtonum(name, 0, 0xffff, &err);
			if(err)
				return 0;
			snprintf(buf, sizeof(buf), "%d", page[dpg]->area[darea]->client[idx]->frame.border);
		}
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
        break;
    case Ftitle:
		if(dtyp == Ddefault)
			snprintf(buf, sizeof(buf), "%d", def.title);
		else {
			idx = cext_strtonum(name, 0, 0xffff, &err);
			if(err)
				return 0;
			snprintf(buf, sizeof(buf), "%d", page[dpg]->area[darea]->client[idx]->frame.title);
		}
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
        break;
    case Finc:
		if(dtyp == Ddefault)
			snprintf(buf, sizeof(buf), "%d", def.inc);
		else {
			idx = cext_strtonum(name, 0, 0xffff, &err);
			if(err)
				return 0;
			snprintf(buf, sizeof(buf), "%d", page[dpg]->area[darea]->client[idx]->inc);
		}
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
        break;
    case Fgeom:
		idx = cext_strtonum(name, 0, 0xffff, &err);
		if(err)
			return 0;
		c = page[dpg]->area[darea]->client[idx];
		snprintf(buf, sizeof(buf), "%d %d %d %d", c->frame.rect.x, c->frame.rect.y,
				c->frame.rect.width, c->frame.rect.height);
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
        break;
    case Fsnap:
		snprintf(buf, sizeof(buf), "%d", def.snap);
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
		break;
    case Fname:
		return mkstat(stat, dir, name, strlen(page[dpg]->area[darea]->client[idx]->name), DMREAD);
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
	unsigned short pg, area, cl;
	unsigned int i, len;
	char buf[32];
	Client *client;

    if(!m) {
        errstr = Enofid;
        return -1;
    }
	typ = qpath_type(m->qid.path);
	pg = qpath_page(m->qid.path);
	area = qpath_area(m->qid.path);
	cl = qpath_client(m->qid.path);

	c->fcall->count = 0;
	if(c->fcall->offset) {
		switch (qpath_type(m->qid.path)) {
		case Droot:
			/* jump to offset */
			len = type_to_stat(&stat, "ctl", &m->qid);
			len += type_to_stat(&stat, "event", &m->qid);
			len += type_to_stat(&stat, "default", &m->qid);
			len += type_to_stat(&stat, "new", &m->qid);
			len += type_to_stat(&stat, "sel", &m->qid);
			for(i = 0; i < npage; i++) {
				if(i == sel)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
				len += type_to_stat(&stat, buf, &m->qid);
				if(len <= c->fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < npage; i++) {
				if(i == sel)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, &m->qid);
				if(c->fcall->count + len > c->fcall->iounit)
					break;
				c->fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Dpage:
			/* jump to offset */
			len = type_to_stat(&stat, "ctl", &m->qid);
			len += type_to_stat(&stat, "new", &m->qid);
			len += type_to_stat(&stat, "sel", &m->qid);
			for(i = 0; i < page[pg]->narea; i++) {
				if(i == page[pg]->sel)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
				len += type_to_stat(&stat, buf, &m->qid);
				if(len <= c->fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < page[pg]->narea; i++) {
				if(i == page[pg]->sel)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, &m->qid);
				if(c->fcall->count + len > c->fcall->iounit)
					break;
				c->fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Darea:
			/* jump to offset */
			len = type_to_stat(&stat, "ctl", &m->qid);
			len += type_to_stat(&stat, "sel", &m->qid);
			for(i = 0; i < page[pg]->area[area]->nclient; i++) {
				if(i == page[pg]->area[area]->sel)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
				len += type_to_stat(&stat, buf, &m->qid);
				if(len <= c->fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < page[pg]->area[area]->nclient; i++) {
				if(i == page[pg]->area[area]->sel)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, &m->qid);
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
			fprintf(stderr, "%s", "Droot dir creation\n");
			c->fcall->count = type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "event", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "default", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "new", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "sel", &m->qid);
			p = ixp_enc_stat(p, &stat);
			for(i = 0; i < npage; i++) {
				if(i == npage)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, &m->qid);
				if(c->fcall->count + len > c->fcall->iounit)
					break;
				c->fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Ddefault:
			c->fcall->count = type_to_stat(&stat, "font", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "selcolor", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "normcolor", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "border", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "title", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "inc", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "snap", &m->qid);
			p = ixp_enc_stat(p, &stat);
			break;
		case Dpage:
			if(pg == npage)
				alloc_page();
			c->fcall->count = type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "new", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "sel", &m->qid);
			p = ixp_enc_stat(p, &stat);
			for(i = 0; i < page[pg]->narea; i++) {
				if(i == page[pg]->area[area]->sel)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, &m->qid);
				if(c->fcall->count + len > c->fcall->iounit)
					break;
				c->fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Darea:
			c->fcall->count = type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "sel", &m->qid);
			p = ixp_enc_stat(p, &stat);
			for(i = 0; i < page[pg]->area[area]->nclient; i++) {
				if(i == page[pg]->area[area]->sel)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, &m->qid);
				if(c->fcall->count + len > c->fcall->iounit)
					break;
				c->fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Dclient:
			c->fcall->count = type_to_stat(&stat, "border", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "title", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "inc", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "name", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "geometry", &m->qid);
			p = ixp_enc_stat(p, &stat);
			c->fcall->count += type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			break;
		case Fctl:
			errstr = Enoperm;
			return -1;
			break;
		case Ffont:
			if((c->fcall->count = strlen(def.font)))
				memcpy(p, def.font, c->fcall->count);
			break;
		case Fevent:
			return 1;
			break;
		case Fselcolor:
			if((c->fcall->count = strlen(def.selcolor)))
				memcpy(p, def.selcolor, c->fcall->count);
			break;
		case Fnormcolor:
			if((c->fcall->count = strlen(def.normcolor)))
				memcpy(p, def.normcolor, c->fcall->count);
			break;
		case Fborder:
			fprintf(stderr, "Fborder: qpath_type(m->qid.path)=%d, m->qid.dtype=%d\n",
							qpath_type(m->qid.path), m->qid.dtype);

			if(m->qid.dtype == Ddefault)
				snprintf(buf, sizeof(buf), "%u", def.border);
			else
				snprintf(buf, sizeof(buf), "%u", page[pg]->area[area]->client[cl]->frame.border);
			c->fcall->count = strlen(buf);
			memcpy(p, buf, c->fcall->count);
			break;
		case Ftitle:
			if(m->qid.dtype == Ddefault)
				snprintf(buf, sizeof(buf), "%u", def.title);
			else
				snprintf(buf, sizeof(buf), "%u", page[pg]->area[area]->client[cl]->frame.title);
			c->fcall->count = strlen(buf);
			memcpy(p, buf, c->fcall->count);
			break;
		case Finc:
			if(m->qid.dtype == Ddefault)
				snprintf(buf, sizeof(buf), "%u", def.inc);
			else
				snprintf(buf, sizeof(buf), "%u", page[pg]->area[area]->client[cl]->inc);
			c->fcall->count = strlen(buf);
			memcpy(p, buf, c->fcall->count);
			break;
		case Fgeom:
			client = page[pg]->area[area]->client[cl];
			snprintf(buf, sizeof(buf), "%d %d %d %d", client->frame.rect.x, client->frame.rect.y,
					client->frame.rect.width, client->frame.rect.height);
			c->fcall->count = strlen(buf);
			memcpy(p, buf, c->fcall->count);
			break;
		case Fsnap:
			snprintf(buf, sizeof(buf), "%u", def.snap);
			c->fcall->count = strlen(buf);
			memcpy(p, buf, c->fcall->count);
			break;
		case Fname:
			if((c->fcall->count = strlen(page[pg]->area[area]->client[cl]->name)))
				memcpy(p, page[pg]->area[area]->client[cl]->name, c->fcall->count);
			break;
		default:
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
	if(!type_to_stat(&c->fcall->stat, name, &m->qid))
		return -1;
    c->fcall->id = RSTAT;
    return 0;
}

static int
xwrite(IXPConn *c)
{
	char buf[256];
    IXPMap *m = ixp_server_fid2map(c, c->fcall->fid);
    unsigned char typ;
	unsigned short pg, area, cl;
	unsigned short i;

    if(!m) {
        errstr = Enofid;
        return -1;
    }
	typ = qpath_type(m->qid.path);
	pg = qpath_page(m->qid.path);
	area = qpath_area(m->qid.path);
	cl = qpath_client(m->qid.path);

	switch (qpath_type(m->qid.path)) {
	case Fctl:
		switch(m->qid.dtype) {
		case Droot:
			/* TODO: /ctl commands */
			break;
		case Dpage:
			/* /TODO: /n/ctl commands */
			break;
		case Darea:
			/* /TODO: /n/{float,n}/ctl commands */
			break;
		case Dclient:
			/* /TODO: /n/{float,n}/n/ctl commands */
			break;
		default:
			errstr = "command not supported";
			return -1;
		}
		break;
	case Ffont:
		if(def.font)
			free(def.font);
		def.font = cext_emallocz(c->fcall->count + 1);
		memcpy(def.font, c->fcall->data, c->fcall->count);
		XFreeFont(dpy, xfont);
    	xfont = blitz_getfont(dpy, def.font);
		/* TODO: update geometry */
		break;
	case Fselcolor:
		if((c->fcall->count != 24)
			|| (c->fcall->data[0] != '#') || (c->fcall->data[8] != '#')
		    || (c->fcall->data[16] != '#'))
		{
			errstr = "wrong color format";
			goto error_xwrite;
		}
		memcpy(def.selcolor, c->fcall->data, c->fcall->count);
		def.selcolor[c->fcall->count] = 0;
		/* TODO: update color */
		break;
	case Fnormcolor:
		if((c->fcall->count != 24)
			|| (c->fcall->data[0] != '#') || (c->fcall->data[8] != '#')
		    || (c->fcall->data[16] != '#'))
		{
			errstr = "wrong color format";
			goto error_xwrite;
		}
		memcpy(def.normcolor, c->fcall->data, c->fcall->count);
		def.normcolor[c->fcall->count] = 0;
		/* TODO: update color */
		break;
	case Fsnap:
		if(c->fcall->count > sizeof(buf))
			goto error_xwrite;
		memcpy(buf, c->fcall->data, c->fcall->count);
		buf[c->fcall->count] = 0;
		i = cext_strtonum(buf, 0, 0xffff, &err);
		if(err) {
			errstr = "snap value out of range 0..0xffff";
			return -1;
		}
		def.snap = i;
		break;
	case Fborder:
		if(c->fcall->count > sizeof(buf))
			goto error_xwrite;
		memcpy(buf, c->fcall->data, c->fcall->count);
		buf[c->fcall->count] = 0;
		i = cext_strtonum(buf, 0, 0xffff, &err);
		if(err) {
			errstr = "border value out of range 0..0xffff";
			return -1;
		}
		if(m->qid.dtype == Ddefault)
			def.border = i;
		else {
			page[pg]->area[area]->client[cl]->frame.border = i;
			/* TODO: resize client */
		}
		break;
	case Ftitle:
		if(c->fcall->count > sizeof(buf))
			goto error_xwrite;
		memcpy(buf, c->fcall->data, c->fcall->count);
		buf[c->fcall->count] = 0;
		i = cext_strtonum(buf, 0, 1, &err);
		if(err) {
			errstr = "title value out of range 0, 1";
			return -1;
		}
		if(m->qid.dtype == Ddefault)
			def.border = i;
		else {
			page[pg]->area[area]->client[cl]->frame.border = i;
			/* TODO: resize client */
		}
		break;
	case Finc:
		if(c->fcall->count > sizeof(buf))
			goto error_xwrite;
		memcpy(buf, c->fcall->data, c->fcall->count);
		buf[c->fcall->count] = 0;
		i = cext_strtonum(buf, 0, 1, &err);
		if(err) {
			errstr = "increment value out of range 0, 1";
			return -1;
		}
		if(m->qid.dtype == Ddefault)
			def.inc = i;
		else {
			page[pg]->area[area]->client[cl]->inc = i;
			/* TODO: resize client */
		}
		break;
	case Fgeom:
		if(c->fcall->count > sizeof(buf))
			goto error_xwrite;
		memcpy(buf, c->fcall->data, c->fcall->count);
		buf[c->fcall->count] = 0;
		blitz_strtorect(&rect, &page[pg]->area[area]->client[cl]->frame.rect, buf);
		/* TODO: resize client */
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

void
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

void
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

void
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
