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
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <sys/socket.h>

#include "wm.h"

static char E9pversion[] = "9P version not supported";
static char Enoperm[] = "permission denied";
static char Enofid[] = "fid not found";
static char Enofile[] = "file not found";
static char Enomode[] = "mode not supported";
static char Enofunc[] = "function not supported";
static char Enocommand[] = "command not supported";

/*
 * filesystem specification
 * / 					Droot
 * /def/				Ddef
 * /def/border			Fborder		0..n
 * /def/bar 			Fbar 		0, 1
 * /def/snap 			Fsnap  		0..n
 * /def/inc 			Finc   		0..n
 * /keys/				Dkeys
 * /keys/grab			Fgrab		interface to grab shortcuts
 * /keys/foo			Fkey
 * /bar/				Dbar
 * /bar/expand			Fexpand 	id of expandable label
 * /bar/new/			Ditem
 * /bar/1/				Ditem
 * /bar/1/data 			Fdata		<arbitrary data which gets displayed>
 * /bar/1/color			Fcolor		<#RRGGBB> <#RRGGBB> <#RRGGBB>
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

Qid root_qid;
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
		case Ddef: return "def"; break;
		case Dkeys: return "keys"; break;
		case Dbar: return "bar"; break;
		case Dpage:
			if(pg == sel)
				return "sel";
			snprintf(buf, sizeof(buf), "%u", pg + 1);
			return buf;
			break;
		case Ditem:
			snprintf(buf, sizeof(buf), "%u", pg + 1);
			return buf;
			break;
		case Darea:
			if(!area) {
				if(page[pg]->sel)
					return "float";
				else
					return "sel";
			}
			if(page[pg]->sel == area)
				return "sel";
			snprintf(buf, sizeof(buf), "%u", area + 1);
			return buf;
			break;
		case Dclient:
			if(page[pg]->area[area]->sel == cl)
				return "sel";
			snprintf(buf, sizeof(buf), "%u", cl + 1);
			return buf;
			break;
		case Fcolor: return "color"; break;
		case Fdata: return "data"; break;
		case Fexpand: return "expand"; break;
		case Fctl: return "ctl"; break;
		case Fborder: return "border"; break;
		case Fsnap: return "border"; break;
		case Fbar: return "bar"; break;
		case Finc: return "inc"; break;
		case Fgeom: return "geometry"; break;
		case Fname: return "name"; break;
		case Fevent: return "event"; break;
		case Fkey: return key[pg]->name; break; 
		case Fgrab: return "grab"; break; 
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
		if(dtyp == Dbar)
			return Ditem;
		if(dtyp == Dpage)
			return Darea;
	}
	if(!strncmp(name, "bar", 4))
		return Dbar;
	if(!strncmp(name, "def", 4))
		return Ddef;
	if(!strncmp(name, "keys", 5))
		return Dkeys;
	if(!strncmp(name, "grab", 5))
		return Fgrab;
	if(!strncmp(name, "ctl", 4))
		return Fctl;
	if(!strncmp(name, "event", 6))
		return Fevent;
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
	if(!strncmp(name, "expand", 7))
		return Fexpand;
	if(!strncmp(name, "color", 6))
		return Fcolor;
	if(!strncmp(name, "data", 5))
		return Fdata;
	if(key_of_name(name))
		return Fkey;
	if(!strncmp(name, "sel", 4))
		goto dyndir;
   	i = (unsigned short) cext_strtonum(name, 1, 0xffff, &err);
    if(err)
		return -1;
dyndir:
	/*fprintf(stderr, "nametotype: dtyp = %d\n", dtyp);*/
	switch(dtyp) {
	case Droot: return Dpage; break;
	case Dbar: return Ditem; break;
	case Dpage: return Darea; break;
	case Darea: return Dclient; break;
	}
	return -1;
}

static int
mkqid(Qid *dir, char *wname, Qid *new, Bool iswalk)
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
	case Ddef:
		new->type = IXP_QTDIR;
		new->path = mkqpath(Ddef, 0, 0, 0);
		break;
	case Dkeys:
		new->type = IXP_QTDIR;
		new->path = mkqpath(Dkeys, 0, 0, 0);
		break;
	case Dbar:
		new->type = IXP_QTDIR;
		new->path = mkqpath(Dbar, 0, 0, 0);
		break;
	case Ditem:
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "new", 4)) {
			/*fprintf(stderr, "mkqid iswalk=%d, wname=%s\n", iswalk, wname);*/
			if(iswalk)
				new->path = mkqpath(Ditem, new_item()->id, 0, 0);
			else
				new->path = mkqpath(Ditem, 0,0 ,0);
		}
		else {
			i = cext_strtonum(wname, 1, 0xffff, &err);
			if(err || (i - 1 >= nitem))
				return -1;
			new->path = mkqpath(Ditem, item[i - 1]->id, 0, 0);
		}
		break;

	case Dpage:
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "new", 4)) {
			if(iswalk) {
				Page *p = alloc_page();
				new->path = mkqpath(Dpage, p->id, 0, 0);
				focus_page(p);
			}
			else
				new->path = mkqpath(Dpage, 0, 0, 0);
		}
		else if(!strncmp(wname, "sel", 4)) {
			if(!npage)
				return -1;
			new->path = mkqpath(Dpage, page[sel]->id, 0, 0);
		}
		else {
			i = cext_strtonum(wname, 1, 0xffff, &err);
			if(err || (i - 1 >= npage))
				return -1;
			new->path = mkqpath(Dpage, page[i - 1]->id, 0, 0);
		}
		break;
	case Darea:
		/*fprintf(stderr, "mkqid(): %s\n", "Darea");*/
		if(!npage || dpg >= npage)
			return -1;
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "new", 4)) {
			if(iswalk) {
				Area *a = alloc_area();
				Page *p = page[dpg];
				p->area = (Area **)cext_array_attach((void **)p->area, a, sizeof(Area *), &p->areasz);
				p->narea++;
				new->path = mkqpath(Darea, dpgid, a->id, 0);
			}
			else
				new->path = mkqpath(Darea, dpgid, 0, 0);
		}
		else if(!strncmp(wname, "sel", 4)) {
			Page *p = page[dpg];
			if(!p->narea)
				return -1;
			new->path = mkqpath(Darea, dpgid, p->area[p->sel]->id, 0);
		}
		else {
			i = cext_strtonum(wname, 1, 0xffff, &err);
			if(err || (i - 1 >= page[dpg]->narea))
				return -1;
			new->path = mkqpath(Darea, dpgid, page[dpg]->area[i - 1]->id, 0);
		}
		break;
	case Dclient:
		if(!npage)
			return -1;
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "sel", 4)) {
			Area *a = page[dpg]->area[darea];
			if(!a->nclient)
				return -1;
			new->path = mkqpath(Dclient, dpgid, daid, a->client[a->sel]->id);
		}
		else {
			Area *a = page[dpg]->area[darea];
			i = cext_strtonum(wname, 1, 0xffff, &err);
			if(err || (i - 1 >= page[dpg]->area[darea]->nclient))
				return -1;
			new->path = mkqpath(Dclient, dpgid, daid, a->client[i - 1]->id);
		}
		break;
	case Fkey:
		{
			Key *k;
			if(!(k = key_of_name(wname)))
				return -1;
			new->type = IXP_QTFILE;
			new->path = mkqpath(Fkey, k->id, 0, 0);
		}
		break;
	case Fdata:
	case Fcolor:
		if(dpgid >= nitem)
			return -1;
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

	/*fprintf(stderr, "wm: xwalk: fid=%d\n", fcall->fid);*/
    if(!(m = ixp_server_fid2map(c, fcall->fid)))
        return Enofid;
	/*fprintf(stderr, "wm: xwalk1: fid=%d\n", fcall->fid);*/
    if(fcall->fid != fcall->newfid && (ixp_server_fid2map(c, fcall->newfid)))
        return Enofid;
	/*fprintf(stderr, "wm: xwalk2: fid=%d\n", fcall->fid);*/
    if(fcall->nwname) {
        dir = m->qid;
        for(nwqid = 0; (nwqid < fcall->nwname)
            && !mkqid(&dir, fcall->wname[nwqid], &fcall->wqid[nwqid], True); nwqid++)
		{
			/*fprintf(stderr, "dir.dtype=%d\n", dir.dtype);*/
            dir = fcall->wqid[nwqid];
			/*fprintf(stderr, "walk: qid_to_name()=%s qpath_type(dir.path)=%d dir.dtype=%d\n",
							qid_to_name(&dir), qpath_type(dir.path), dir.dtype);*/
		}
        if(!nwqid) {
			/*fprintf(stderr, "%s", "xwalk: no such file\n");*/
			return Enofile;
		}
    }
	/*fprintf(stderr, "wm: xwalk3: fid=%d\n", fcall->fid);*/
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
    mkqid(dir, name, &stat->qid, False);
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

	/*fprintf(stderr, "typetostat(0): name=%s dpgid=%d daid=%d dcid=%d\n", name, dpgid, daid, dcid);
	fprintf(stderr, "typetostat (0) Dtype=%d\n", type);
	*/

	if(dpgid && ((dpg = index_of_page_id(dpgid)) == -1))
		return 0;
	if(daid && ((darea = index_of_area_id(page[dpg], daid)) == -1))
		return 0;
	if(dcid && ((dcl = index_of_client_id(page[dpg]->area[darea], dcid)) == -1))
		return 0;

	/*
	fprintf(stderr, "typetostat(1): dpg=%d darea=%d dcl=%d\n", dpg, darea, dcl);
	fprintf(stderr, "typetostat (3) Dtype=%d\n", type);
	*/

    switch (type) {
    case Dclient:
    case Darea:
    case Dpage:
    case Ddef:
	case Dkeys:
	case Dbar:
    case Droot:
		return mkstat(stat, dir, name, 0, DMDIR | DMREAD | DMEXEC);
        break;
	case Fctl:
	case Fgrab:
		return mkstat(stat, dir, name, 0, DMWRITE);
		break;
    case Fevent:
		return mkstat(stat, dir, name, 0, DMREAD);
		break;
    case Fborder:
		if(dtyp == Ddef)
			snprintf(buf, sizeof(buf), "%d", def.border);
		else
			snprintf(buf, sizeof(buf), "%d", page[dpg]->area[darea]->client[dcl]->frame.border);
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
        break;
    case Fbar:
		if(dtyp == Ddef)
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
    case Fkey:
		return mkstat(stat, dir, name, 0, 0);
		break;
    case Fexpand:
		snprintf(buf, sizeof(buf), "%u", iexpand + 1);
		return mkstat(stat, dir, name, strlen(buf), DMREAD | DMWRITE);
		break;
    case Fdata:
		return mkstat(stat, dir, name, (dpg == nitem) ? 0 : strlen(item[dpg]->data), DMREAD | DMWRITE);
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
	unsigned short id = qpath_pgid(m->qid.path);
	int i;

    if(!m)
        return Enofid;
	if(id && ((i = index_of_page_id(id)) == -1))
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
		draw_bar();
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
			len += type_to_stat(&stat, "keys", &m->qid);
			len += type_to_stat(&stat, "bar", &m->qid);
			len += type_to_stat(&stat, "new", &m->qid);
			for(i = 0; i < npage; i++) {
				if(i == sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i + 1);
				len += type_to_stat(&stat, buf, &m->qid);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < npage; i++) {
				if(i == sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i + 1);
				len = type_to_stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Dkeys:
			/* jump to offset */
			len = type_to_stat(&stat, "grab", &m->qid);
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
		case Dbar:
			/* jump to offset */
			len = type_to_stat(&stat, "expand", &m->qid);
			len += type_to_stat(&stat, "new", &m->qid);
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
		case Dpage:
			/* jump to offset */
			len = type_to_stat(&stat, "ctl", &m->qid);
			len += type_to_stat(&stat, "new", &m->qid);
			for(i = 0; i < page[pg]->narea; i++) {
				if(i == page[pg]->sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i + 1);
				len += type_to_stat(&stat, buf, &m->qid);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < page[pg]->narea; i++) {
				if(i == page[pg]->sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i + 1);
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
			for(i = 0; i < page[pg]->area[area]->nclient; i++) {
				if(i == page[pg]->area[area]->sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i + 1);
				len += type_to_stat(&stat, buf, &m->qid);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < page[pg]->area[area]->nclient; i++) {
				if(i == page[pg]->area[area]->sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
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
			/*fprintf(stderr, "%s", "Droot dir creation\n");*/
			fcall->count = type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "event", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "default", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "keys", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "bar", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "new", &m->qid);
			p = ixp_enc_stat(p, &stat);
			for(i = 0; i < npage; i++) {
				if(i == sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i + 1);
				len = type_to_stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Dkeys:
			fcall->count = type_to_stat(&stat, "grab", &m->qid);
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
		case Dbar:
			fcall->count = type_to_stat(&stat, "expand", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "new", &m->qid);
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
		case Ddef:
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
			fcall->count = type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "new", &m->qid);
			p = ixp_enc_stat(p, &stat);
			for(i = 0; i < page[pg]->narea; i++) {
				if(i == page[pg]->sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i + 1);
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
			for(i = 0; i < page[pg]->area[area]->nclient; i++) {
				if(i == page[pg]->area[area]->sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i + 1);
				len = type_to_stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Dclient:
			fcall->count = type_to_stat(&stat, "border", &m->qid);
			/*fprintf(stderr, "msgl=%d\n", fcall->count);*/
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "bar", &m->qid);
			/*fprintf(stderr, "msgl=%d\n", fcall->count);*/
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "name", &m->qid);
			/*fprintf(stderr, "msgl=%d\n", fcall->count);*/
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "geometry", &m->qid);
			/*fprintf(stderr, "msgl=%d\n", fcall->count);*/
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "ctl", &m->qid);
			/*fprintf(stderr, "msgl=%d\n", fcall->count);*/
			p = ixp_enc_stat(p, &stat);
			break;
		case Fctl:
			return Enoperm;
			break;
		case Fevent:
			ixp_server_enqueue_fcall(c, fcall);
			return nil;
			break;
		case Fborder:
			/*fprintf(stderr, "Fborder: qpath_type(m->qid.path)=%d, m->qid.dtype=%d\n",
							qpath_type(m->qid.path), m->qid.dtype);*/

			if(m->qid.dtype == Ddef)
				snprintf(buf, sizeof(buf), "%u", def.border);
			else
				snprintf(buf, sizeof(buf), "%u", page[pg]->area[area]->client[cl]->frame.border);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case Fbar:
			if(m->qid.dtype == Ddef)
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
		case Fexpand:
			snprintf(buf, sizeof(buf), "%u", iexpand + 1);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case Fdata:
			if(pg >= nitem)
				return Enofile;
			if((fcall->count = strlen(item[pg]->data)))
				memcpy(p, item[pg]->data, fcall->count);
			break;
		case Fcolor:
			if(pg >= nitem)
				return Enofile;
			if((fcall->count = strlen(item[pg]->colstr)))
				memcpy(p, item[pg]->colstr, fcall->count);
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
		if(fcall->count > sizeof(buf) - 1)
			return Enocommand;
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		switch(m->qid.dtype) {
		case Droot:
			if(!strncmp(buf, "quit", 5))
				srv.running = 0;
			else if(!strncmp(buf, "pager", 6))
				pager();
			else if(!strncmp(buf, "detached", 9))
				detached_clients();
			else if(!strncmp(buf, "attach", 7))
				attach_detached_client();
			else if(!strncmp(buf, "select", 6))
				select_page(&buf[7]);
			else
				return Enocommand;
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
			break;
		}
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
		if(m->qid.dtype == Ddef)
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
		if(m->qid.dtype == Ddef)
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
    case Fgrab:
		if(fcall->count > 2048)
			return Enoperm;
		{
			static size_t lastcount;
			static char last[2048]; /* iounit */
			char fcallbuf[2048], tmp[2048]; /* iounit */
			char *p1, *p2;
			if(!fcall->offset) {
				while(nkey) {
					Key *k = key[0];
					ungrab_key(k);
					cext_array_detach((void **)key, k, &keysz);
					nkey--;
					destroy_key(k);
				}
			}
			if(!fcall->count)
				break;
		    memcpy(fcallbuf, fcall->data, fcall->count);
		    fcallbuf[fcall->count] = 0;
			if(fcall->offset) {
				p1 = strrchr(last, '\n');
				p2 = strchr(fcallbuf, '\n');
				memcpy(tmp, p1, lastcount - (p1 - last));
				memcpy(tmp + (lastcount - (p1 - last)), p2, p2 - fcallbuf);
				tmp[(lastcount - (p1 - last)) + (p2 - fcallbuf)] = 0;
				grab_key(create_key(tmp));

			}
			else p2 = fcallbuf;
			lastcount = fcall->count;
			memcpy(last, fcall->data, fcall->count);
			while(p2 - fcallbuf < fcall->count) {
				p1 = strchr(p2, '\n');
				if(!p1)
					return "cannot grab, no \n supplied";
				*p1 = 0;
				grab_key(create_key(p2));
				*p1 = '\n';
				p2 = ++p1;
			}
			lastcount = fcall->count;
			memcpy(last, fcall->data, fcall->count);
		}
		break;
    case Fexpand:
		{
			const char *err;
			if(fcall->count && fcall->count < 16) {
				memcpy(buf, fcall->data, fcall->count);
				buf[fcall->count] = 0;
				i = (unsigned short) cext_strtonum(buf, 1, 0xffff, &err);
				if(!err && (pg - 1 < nitem)) {
					iexpand = pg - 1;
					draw_bar();
					break;
				}
			}
		}
		return Enofile;
		break;
	case Fdata:
		{
			unsigned int len = fcall->count;
			if(pg >= nitem)
				return Enofile;
			if(len >= sizeof(item[pg]->data))
				len = sizeof(item[pg]->data) - 1;
			memcpy(item[pg]->data, fcall->data, len);
			item[pg]->data[len] = 0;
			draw_bar();
		}
		break;
	case Fcolor:
		if((pg >= nitem) || (fcall->count != 23)
			|| (fcall->data[0] != '#') || (fcall->data[8] != '#')
		    || (fcall->data[16] != '#')
		  )
			return "wrong color format";
		memcpy(item[pg]->colstr, fcall->data, fcall->count);
		item[pg]->colstr[fcall->count] = 0;
		blitz_loadcolor(dpy, screen, item[pg]->colstr, &item[pg]->color);
		draw_bar();
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
			/*fprintf(stderr, "do_fcall=%d\n", fcall.id);*/
		switch(fcall.id) {
		case TVERSION: errstr = xversion(c, &fcall); break;
		case TATTACH: errstr = xattach(c, &fcall); break;
		case TWALK: errstr = xwalk(c, &fcall); break;
		case TOPEN: errstr = xopen(c, &fcall); break;
		case TREMOVE: errstr = xremove(c, &fcall); break;
		case TREAD: errstr = xread(c, &fcall); break;
		case TWRITE: errstr = xwrite(c, &fcall); break;
		case TCLUNK: errstr = xclunk(c, &fcall); break;
		case TSTAT: errstr = xstat(c, &fcall); break;
		default: errstr = Enofunc; break;
		}
		if(errstr)
			ixp_server_respond_error(c, &fcall, errstr);
	}
	check_x_event(nil);
}

void
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

void
new_ixp_conn(IXPConn *c)
{
	int fd = ixp_accept_sock(c->fd);
	
	if(fd >= 0)
		ixp_server_open_conn(c->srv, fd, do_fcall, ixp_server_close_conn);
}
