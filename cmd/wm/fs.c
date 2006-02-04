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
 * /default/bar 		Fbar 		0, 1
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
 * /1/float/1/bar		Fbar		0, 1
 * /1/float/1/name		Fname		name of client
 * /1/float/1/geom		Fgeom		geometry of client
 * /1/float/1/ctl 		Fctl 		command interface (client)
 * /1/1/				Darea
 * /1/1/ctl 			Fctl 		command interface (area)
 * /1/1/1/sel/			Dclient
 * /1/1/1/1/			Dclient
 * /1/1/1/border		Fborder		0..n
 * /1/1/1/bar			Fbar		0, 1
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
static char Enocommand[] = "command not supported";

const char *err;

/* IXP stuff */

unsigned long long
mkqpath(unsigned char type, unsigned short pgid, unsigned short aid, unsigned short cid)
{
    return ((unsigned long long) type << 48) | ((unsigned long long) pgid << 32)
		| ((unsigned long long) aid << 16) | (unsigned long long) cid;
}

static unsigned char
qpath_type(unsigned long long path)
{
    return (path >> 48) & 0xff;
}

static unsigned short
qpath_pgid(unsigned long long path)
{
    return (path >> 32) & 0xffff;
}

static unsigned short
qpath_aid(unsigned long long path)
{
    return (path >> 16) & 0xffff;
}

static unsigned short
qpath_cid(unsigned long long path)
{
    return path & 0xffff;
}

static char *
qid_to_name(Qid *qid)
{
	unsigned char typ = qpath_type(qid->path);
	unsigned short pgid = qpath_pgid(qid->path);
	unsigned short aid = qpath_aid(qid->path);
	unsigned short cid = qpath_cid(qid->path);
	int pg = 0, area = 0, cl = 0;
	static char buf[32];

	if(pgid && ((pg = index_of_page_id(pgid)) == -1))
		return nil;
	if(aid && ((area = index_of_area_id(page[pg], aid)) == -1))
		return nil;
	if(cid && ((cl = index_of_client_id(page[pg]->area[area], cid)) == -1))
		return nil;

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
		case Fbar: return "bar"; break;
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
	if(!strncmp(name, "bar", 4))
		return Fbar;
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
	fprintf(stderr, "nametotype: dtyp = %d\n", dtyp);
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
	unsigned short dpgid = qpath_pgid(dir->path);
	unsigned short daid = qpath_aid(dir->path);
	unsigned short dcid = qpath_cid(dir->path);
	int dpg = 0, darea = 0, dcl = 0;
	unsigned short i;
	int type = name_to_type(wname, dtyp);

	if(dpgid && ((dpg = index_of_page_id(dpgid)) == -1))
		return -1;
	if(daid && ((darea = index_of_area_id(page[dpg], daid)) == -1))
		return -1;
	if(dcid && ((dcl = index_of_client_id(page[dpg]->area[darea], dcid)) == -1))
		return -1;
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
			new->path = mkqpath(Dpage, NEW_OBJ, 0, 0);
		else if(!strncmp(wname, "sel", 4)) {
			if(!npage)
				return -1;
			new->path = mkqpath(Dpage, page[sel]->id, 0, 0);
		}
		else {
			i = cext_strtonum(wname, 0, 0xffff, &err);
			if(err || (i >= npage))
				return -1;
			new->path = mkqpath(Dpage, page[i]->id, 0, 0);
		}
		break;
	case Darea:
		fprintf(stderr, "mkqid(): %s\n", "Darea");
		if(!npage)
			return -1;
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "new", 4))
			new->path = mkqpath(Darea, dpgid, NEW_OBJ, 0);
		else if(!strncmp(wname, "sel", 4)) {
			Page *p = page[dpg];
			new->path = mkqpath(Darea, dpgid, p->area[p->sel]->id, 0);
		}
		else {
			i = cext_strtonum(wname, 0, 0xffff, &err);
			if(err || (i >= page[dpg]->narea))
				return -1;
			new->path = mkqpath(Darea, dpgid, page[dpg]->area[i]->id, 0);
		}
		break;
	case Dclient:
		if(!npage)
			return -1;
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "sel", 4)) {
			Area *a = page[dpg]->area[darea];
			new->path = mkqpath(Dclient, dpgid, daid, a->client[a->sel]->id);
		}
		else {
			Area *a = page[dpg]->area[darea];
			i = cext_strtonum(wname, 0, 0xffff, &err);
			if(err || (i >= page[dpg]->area[darea]->nclient))
				return -1;
			new->path = mkqpath(Dclient, dpgid, daid, a->client[i]->id);
		}
		break;
	default:
		new->type = IXP_QTFILE;
    	new->path = mkqpath(type, dpgid, daid, dcid);
		break;
	}
    return 0;
}

static char *
xversion(IXPConn *c, Fcall *fcall)
{
    if(strncmp(fcall->version, IXP_VERSION, strlen(IXP_VERSION)))
        return E9pversion;
    else if(fcall->maxmsg > IXP_MAX_MSG)
        fcall->maxmsg = IXP_MAX_MSG;
    fcall->id = RVERSION;
	ixp_server_respond_fcall(c, fcall);
    return nil;
}

static char *
xattach(IXPConn *c, Fcall *fcall)
{
    IXPMap *new = cext_emallocz(sizeof(IXPMap));
    new->qid = root_qid;
    new->fid = fcall->fid;
	c->map = (IXPMap **)cext_array_attach((void **)c->map, new,
					sizeof(IXPMap *), &c->mapsz);
    fcall->id = RATTACH;
    fcall->qid = root_qid;
	ixp_server_respond_fcall(c, fcall);
    return nil;
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
            && !mkqid(&dir, fcall->wname[nwqid], &fcall->wqid[nwqid]); nwqid++)
		{
			fprintf(stderr, "dir.dtype=%d\n", dir.dtype);
            dir = fcall->wqid[nwqid];
			fprintf(stderr, "walk: qid_to_name()=%s qpath_type(dir.path)=%d dir.dtype=%d\n",
							qid_to_name(&dir), qpath_type(dir.path), dir.dtype);
		}
        if(!nwqid) {
			fprintf(stderr, "%s", "xwalk: no sucj file\n");
			return Enofile;
		}
    }
    /* a fid will only be valid, if the walk was complete */
    if(nwqid == fcall->nwname) {
        if(fcall->fid != fcall->newfid) {
			m = cext_emallocz(sizeof(IXPMap));
			c->map = (IXPMap **)cext_array_attach((void **)c->map, m,
							sizeof(IXPMap *), &c->mapsz);
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
    mkqid(dir, name, &stat->qid);
	return ixp_sizeof_stat(stat);
}

static unsigned int
type_to_stat(Stat *stat, char *name, Qid *dir)
{
	unsigned char dtyp = qpath_type(dir->path);
	unsigned short dpgid = qpath_pgid(dir->path);
	unsigned short daid = qpath_aid(dir->path);
	unsigned short dcid = qpath_cid(dir->path);
	int dpg = 0, darea = 0, dcl = 0;
	int type = name_to_type(name, dir->dtype);
	char buf[32];
	Client *c;

	fprintf(stderr, "typetostat(0): name=%s dpgid=%d daid=%d dcid=%d\n", name, dpgid, daid, dcid);
	fprintf(stderr, "typetostat (0) Dtype=%d\n", type);

	if(dpgid && ((dpg = index_of_page_id(dpgid)) == -1))
		return 0;
	if(daid && ((darea = index_of_area_id(page[dpg], daid)) == -1))
		return 0;
	if(dcid && ((dcl = index_of_client_id(page[dpg]->area[darea], dcid)) == -1))
		return 0;

	fprintf(stderr, "typetostat(1): dpg=%d darea=%d dcl=%d\n", dpg, darea, dcl);
	fprintf(stderr, "typetostat (3) Dtype=%d\n", type);

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
		else
			snprintf(buf, sizeof(buf), "%d", page[dpg]->area[darea]->client[dcl]->frame.border);
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
        break;
    case Fbar:
		if(dtyp == Ddefault)
			snprintf(buf, sizeof(buf), "%d", def.bar);
		else
			snprintf(buf, sizeof(buf), "%d", page[dpg]->area[darea]->client[dcl]->frame.bar);
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
        break;
    case Finc:
		snprintf(buf, sizeof(buf), "%d", def.inc);
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
        break;
    case Fgeom:
		c = page[dpg]->area[darea]->client[dcl];
		snprintf(buf, sizeof(buf), "%d %d %d %d", c->frame.rect.x, c->frame.rect.y,
				c->frame.rect.width, c->frame.rect.height);
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
        break;
    case Fsnap:
		snprintf(buf, sizeof(buf), "%d", def.snap);
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
		break;
    case Fname:
		c = page[dpg]->area[darea]->client[dcl];
		return mkstat(stat, dir, name, strlen(c->name), DMREAD);
        break;
    }
	return 0;
}

static char *
xread(IXPConn *c, Fcall *fcall)
{
	Stat stat;
    IXPMap *m = ixp_server_fid2map(c, fcall->fid);
    unsigned char *p = fcall->data;
	unsigned short pgid, aid, cid;
	unsigned int i, len;
	char buf[32];
	Client *client;
	int pg = 0, area = 0, cl = 0;

    if(!m)
        return Enofid;

	pgid = qpath_pgid(m->qid.path);
	aid = qpath_aid(m->qid.path);
	cid = qpath_cid(m->qid.path);

	if(pgid && ((pg = index_of_page_id(pgid)) == -1))
		return Enofile;
	if(aid && ((area = index_of_area_id(page[pg], aid)) == -1))
		return Enofile;
	if(cid && ((cl = index_of_client_id(page[pg]->area[area], cid)) == -1))
		return Enofile;

	fcall->count = 0;
	if(fcall->offset) {
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
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < npage; i++) {
				if(i == sel)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
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
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < page[pg]->narea; i++) {
				if(i == page[pg]->sel)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
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
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < page[pg]->area[area]->nclient; i++) {
				if(i == page[pg]->area[area]->sel)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
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
			fprintf(stderr, "%s", "Droot dir creation\n");
			fcall->count = type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "event", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "default", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "new", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "sel", &m->qid);
			p = ixp_enc_stat(p, &stat);
			for(i = 0; i < npage; i++) {
				if(i == npage)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Ddefault:
			fcall->count = type_to_stat(&stat, "font", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "selcolor", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "normcolor", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "border", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "bar", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "inc", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "snap", &m->qid);
			p = ixp_enc_stat(p, &stat);
			break;
		case Dpage:
			if(pg == npage)
				focus_page(alloc_page());
			fcall->count = type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "new", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "sel", &m->qid);
			p = ixp_enc_stat(p, &stat);
			for(i = 0; i < page[pg]->narea; i++) {
				if(i == page[pg]->area[area]->sel)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Darea:
			fcall->count = type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "sel", &m->qid);
			p = ixp_enc_stat(p, &stat);
			for(i = 0; i < page[pg]->area[area]->nclient; i++) {
				if(i == page[pg]->area[area]->sel)
					continue;
				snprintf(buf, sizeof(buf), "%u", i);
				len = type_to_stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Dclient:
			fcall->count = type_to_stat(&stat, "border", &m->qid);
			fprintf(stderr, "msgl=%d\n", fcall->count);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "bar", &m->qid);
			fprintf(stderr, "msgl=%d\n", fcall->count);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "name", &m->qid);
			fprintf(stderr, "msgl=%d\n", fcall->count);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "geometry", &m->qid);
			fprintf(stderr, "msgl=%d\n", fcall->count);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "ctl", &m->qid);
			fprintf(stderr, "msgl=%d\n", fcall->count);
			p = ixp_enc_stat(p, &stat);
			break;
		case Fctl:
			return Enoperm;
			break;
		case Ffont:
			if((fcall->count = strlen(def.font)))
				memcpy(p, def.font, fcall->count);
			break;
		case Fevent:
			ixp_server_enqueue_fcall(c, fcall);
			return nil;
			break;
		case Fselcolor:
			if((fcall->count = strlen(def.selcolor)))
				memcpy(p, def.selcolor, fcall->count);
			break;
		case Fnormcolor:
			if((fcall->count = strlen(def.normcolor)))
				memcpy(p, def.normcolor, fcall->count);
			break;
		case Fborder:
			fprintf(stderr, "Fborder: qpath_type(m->qid.path)=%d, m->qid.dtype=%d\n",
							qpath_type(m->qid.path), m->qid.dtype);

			if(m->qid.dtype == Ddefault)
				snprintf(buf, sizeof(buf), "%u", def.border);
			else
				snprintf(buf, sizeof(buf), "%u", page[pg]->area[area]->client[cl]->frame.border);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case Fbar:
			if(m->qid.dtype == Ddefault)
				snprintf(buf, sizeof(buf), "%u", def.bar);
			else
				snprintf(buf, sizeof(buf), "%u", page[pg]->area[area]->client[cl]->frame.bar);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case Finc:
			snprintf(buf, sizeof(buf), "%u", def.inc);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case Fgeom:
			client = page[pg]->area[area]->client[cl];
			snprintf(buf, sizeof(buf), "%d %d %d %d", client->frame.rect.x, client->frame.rect.y,
					client->frame.rect.width, client->frame.rect.height);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case Fsnap:
			snprintf(buf, sizeof(buf), "%u", def.snap);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case Fname:
			if((fcall->count = strlen(page[pg]->area[area]->client[cl]->name)))
				memcpy(p, page[pg]->area[area]->client[cl]->name, fcall->count);
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
	unsigned short pgid, aid, cid;
	unsigned short i;
	int pg = 0, area = 0, cl = 0;

    if(!m)
        return Enofid;

	pgid = qpath_pgid(m->qid.path);
	aid = qpath_aid(m->qid.path);
	cid = qpath_cid(m->qid.path);

	if(pgid && ((pg = index_of_page_id(pgid)) == -1))
		return Enofile;
	if(aid && ((area = index_of_area_id(page[pg], aid)) == -1))
		return Enofile;
	if(cid && ((cl = index_of_client_id(page[pg]->area[area], cid)) == -1))
		return Enofile;

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
			return Enocommand;
		}
		break;
	case Ffont:
		if(def.font)
			free(def.font);
		def.font = cext_emallocz(fcall->count + 1);
		memcpy(def.font, fcall->data, fcall->count);
		XFreeFont(dpy, xfont);
    	xfont = blitz_getfont(dpy, def.font);
		/* TODO: update geometry */
		break;
	case Fselcolor:
		if((fcall->count != 24)
			|| (fcall->data[0] != '#') || (fcall->data[8] != '#')
		    || (fcall->data[16] != '#')
		)
			return "wrong color format";
		memcpy(def.selcolor, fcall->data, fcall->count);
		def.selcolor[fcall->count] = 0;
		blitz_loadcolor(dpy, screen, def.selcolor, &def.sel);
    	XSetForeground(dpy, gc_xor, def.sel.bg);
		/* TODO: update color */
		break;
	case Fnormcolor:
		if((fcall->count != 24)
			|| (fcall->data[0] != '#') || (fcall->data[8] != '#')
		    || (fcall->data[16] != '#')
		)
			return "wrong color format";
		memcpy(def.normcolor, fcall->data, fcall->count);
		def.normcolor[fcall->count] = 0;
		blitz_loadcolor(dpy, screen, def.normcolor, &def.norm);
		/* TODO: update color */
		break;
	case Fsnap:
		if(fcall->count > sizeof(buf))
			return "snap value out of range 0..0xffff";
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		i = cext_strtonum(buf, 0, 0xffff, &err);
		if(err)
			return "snap value out of range 0..0xffff";
		def.snap = i;
		break;
	case Fborder:
		if(fcall->count > sizeof(buf))
			return "border value out of range 0..0xffff";
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		i = cext_strtonum(buf, 0, 0xffff, &err);
		if(err)
			return "border value out of range 0..0xffff";
		if(m->qid.dtype == Ddefault)
			def.border = i;
		else {
			page[pg]->area[area]->client[cl]->frame.border = i;
			/* TODO: resize client */
		}
		break;
	case Fbar:
		if(fcall->count > sizeof(buf))
			return "bar value out of range 0, 1";
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		i = cext_strtonum(buf, 0, 1, &err);
		if(err)
			return "bar value out of range 0, 1";
		if(m->qid.dtype == Ddefault)
			def.border = i;
		else {
			page[pg]->area[area]->client[cl]->frame.border = i;
			/* TODO: resize client */
		}
		break;
	case Finc:
		if(fcall->count > sizeof(buf))
			return "increment value out of range 0, 1";
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		i = cext_strtonum(buf, 0, 1, &err);
		if(err)
			return "increment value out of range 0, 1";
		def.inc = i;
		/* TODO: resize all clients */
		break;
	case Fgeom:
		if(fcall->count > sizeof(buf))
			return "geometry values out of range";
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		blitz_strtorect(&rect, &page[pg]->area[area]->client[cl]->frame.rect, buf);
		/* TODO: resize client */
		break;
	default:
		return "invalid write";
		break;
	}
    fcall->id = RWRITE;
	ixp_server_respond_fcall(c, fcall);
	return nil;
}

static char *
xclunk(IXPConn *c, Fcall *fcall)
{
    IXPMap *m = ixp_server_fid2map(c, fcall->fid);

    if(!m)
        return Enofid;
	cext_array_detach((void **)c->map, m, &c->mapsz);
    free(m);
    fcall->id = RCLUNK;
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
			fprintf(stderr, "do_fcall=%d\n", fcall.id);
		switch(fcall.id) {
		case TVERSION: errstr = xversion(c, &fcall); break;
		case TATTACH: errstr = xattach(c, &fcall); break;
		case TWALK: errstr = xwalk(c, &fcall); break;
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

void
do_pend_fcall(char *event)
{
	size_t i;
	Fcall *fcall;
	for(i = 0; (i < srv.connsz) && srv.conn[i]; i++) {
		IXPConn *c = srv.conn[i];
		/* all pending TREADs are on /event, so no qid checking necessary */
		while((fcall = ixp_server_dequeue_fcall(c, TREAD))) {
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

void
new_ixp_conn(IXPConn *c)
{
	int fd = ixp_accept_sock(c->fd);
	
	if(fd >= 0)
		ixp_server_open_conn(c->srv, fd, do_fcall, ixp_server_close_conn);
}
