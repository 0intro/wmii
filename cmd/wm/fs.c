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

#define WMII_IOUNIT 	2048

/*
 * filesystem specification
 * / 					Droot
 * /def/				Ddef
 * /def/border			Fborder		0..n
 * /def/bar 			Fbar 		0, 1
 * /def/snap 			Fsnap  		0..n
 * /def/inc 			Finc   		0..n
 * /def/font			Ffont  		xlib font name
 * /def/selcolors		Fselcolors	sel color
 * /def/normcolors		Fnormcolors normal colors
 * /keys/				Dkeys
 * /keys/foo			Fkey
 * /bar/				Dbar
 * /bar/expand			Fexpand 	id of expandable label
 * /bar/new/			Dlabel
 * /bar/1/				Dlabel
 * /bar/1/data 			Fdata		<arbitrary data which gets displayed>
 * /bar/1/colors		Fcolors		<#RRGGBB> <#RRGGBB> <#RRGGBB>
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

/**
 * Qid->path is calculated related to the index of the associated structure.
 * i1 is associated to page, key or label
 * i2 is associated to area
 * i3 is associated to client
 * ie /sel/sel/ctl is i1id = sel page id, i2id = sel area id , i3id = 0 (no id)
 */
unsigned long long
mkqpath(unsigned char type, unsigned short i1id, unsigned short i2id, unsigned short i3id)
{
    return ((unsigned long long) type << 48) | ((unsigned long long) i1id << 32)
		| ((unsigned long long) i2id << 16) | (unsigned long long) i3id;
}

static unsigned char
qpath_type(unsigned long long path)
{
    return (path >> 48) & 0xff;
}

static unsigned short
qpath_i1id(unsigned long long path)
{
    return (path >> 32) & 0xffff;
}

static unsigned short
qpath_i2id(unsigned long long path)
{
    return (path >> 16) & 0xffff;
}

static unsigned short
qpath_i3id(unsigned long long path)
{
    return path & 0xffff;
}

static void
decode_qpath(Qid *qid, unsigned char *type, int *i1, int *i2, int *i3)
{
	unsigned short i1id = qpath_i1id(qid->path);
	unsigned short i2id = qpath_i2id(qid->path);
	unsigned short i3id = qpath_i3id(qid->path);
	*type = qpath_type(qid->path);

	if(i1id) {
		switch(*type) {
			case Fkey: *i1 = kid_to_index(i1id); break;
			case Fdata:
			case Fcolors:
			case Dlabel: *i1 = lid_to_index(i1id); break;
			default: *i1 = pid_to_index(i1id); break;
		}
	   if(i2id && (*i1 != -1)) {
			*i2 = aid_to_index(page[*i1], i2id);
		   if(i3id && (*i2 != -1))
			   *i3 = cid_to_index(page[*i1]->area[*i2], i3id);
	   }
	}
}

static char *
qid_to_name(Qid *qid)
{
	unsigned char type;
	int i1 = 0, i2 = 0, i3 = 0;
	static char buf[32];

	decode_qpath(qid, &type, &i1, &i2, &i3);
	if((i1 == -1) || (i2 == -1) || (i3 == -1))
		return 0;

	switch(type) {
		case Droot: return "/"; break;
		case Ddef: return "def"; break;
		case Dkeys: return "keys"; break;
		case Fbar: 
		case Dbar: return "bar"; break;
		case Dpage:
			if(i1 == sel)
				return "sel";
			snprintf(buf, sizeof(buf), "%u", i1 + 1);
			return buf;
			break;
		case Dlabel:
			snprintf(buf, sizeof(buf), "%u", i1 + 1);
			return buf;
			break;
		case Darea:
			if(!i2) {
				if(page[i1]->sel)
					return "float";
				else
					return "sel";
			}
			if(page[i1]->sel == i2)
				return "sel";
			snprintf(buf, sizeof(buf), "%u", i2 + 1);
			return buf;
			break;
		case Dclient:
			if(page[i1]->area[i2]->sel == i3)
				return "sel";
			snprintf(buf, sizeof(buf), "%u", i3 + 1);
			return buf;
			break;
		case Fselcolors: return "selcolors"; break;
		case Fnormcolors: return "normcolors"; break;
		case Ffont: return "font"; break;
		case Fcolors: return "colors"; break;
		case Fdata: return "data"; break;
		case Fexpand: return "expand"; break;
		case Fctl: return "ctl"; break;
		case Fborder: return "border"; break;
		case Fsnap: return "border"; break;
		case Finc: return "inc"; break;
		case Fgeom: return "geometry"; break;
		case Fname: return "name"; break;
		case Fevent: return "event"; break;
		case Fkey: return key[i1]->name; break; 
		default: return nil; break;
	}
}

static int
name_to_type(char *name, unsigned char dir_type)
{
    unsigned int i;
	if(!name || !name[0] || !strncmp(name, "/", 2) || !strncmp(name, "..", 3))
		return Droot;
	if(!strncmp(name, "new", 4)) {
		switch(dir_type) {
		case Droot: return Dpage; break;
		case Dbar: 	return Dlabel; break;
		case Dpage: return Darea; break;
		}
	}
	if(!strncmp(name, "bar", 4)) {
		if(dir_type == Droot)
			return Dbar;
		else
			return Fbar;
	}
	if(!strncmp(name, "def", 4))
		return Ddef;
	if(!strncmp(name, "keys", 5))
		return Dkeys;
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
	if(!strncmp(name, "inc", 4))
		return Finc;
	if(!strncmp(name, "geometry", 9))
		return Fgeom;
	if(!strncmp(name, "expand", 7))
		return Fexpand;
	if(!strncmp(name, "colors", 7))
		return Fcolors;
	if(!strncmp(name, "selcolors", 10))
		return Fselcolors;
	if(!strncmp(name, "normcolors", 11))
		return Fnormcolors;
	if(!strncmp(name, "font", 5))
		return Ffont;
	if(!strncmp(name, "data", 5))
		return Fdata;
	if(name_to_key(name))
		return Fkey;
	if(!strncmp(name, "sel", 4))
		goto dyndir;
   	i = (unsigned short) cext_strtonum(name, 1, 0xffff, &err);
    if(err)
		return -1;
dyndir:
	/*fprintf(stderr, "nametotype: dir_type = %d\n", dir_type);*/
	switch(dir_type) {
	case Droot: return Dpage; break;
	case Dbar: return Dlabel; break;
	case Dpage: return Darea; break;
	case Darea: return Dclient; break;
	}
	return -1;
}

static int
mkqid(Qid *dir, char *wname, Qid *new, Bool iswalk)
{
	unsigned char dir_type;
	int dir_i1 = 0, dir_i2 = 0, dir_i3 = 0;
	int type, i;

	decode_qpath(dir, &dir_type, &dir_i1, &dir_i2, &dir_i3);
	if((dir_i1 == -1) || (dir_i2 == -1) || (dir_i3 == -1))
		return -1;
	type = name_to_type(wname, dir_type);

	new->dir_type = dir_type;
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
	case Dlabel:
		new->type = IXP_QTDIR;
		if(!strncmp(wname, "new", 4)) {
			if(iswalk)
				new->path = mkqpath(Dlabel, new_label()->id, 0, 0);
			else
				new->path = mkqpath(Dlabel, 0,0 ,0);
		}
		else {
			i = cext_strtonum(wname, 1, 0xffff, &err);
			if(err || (i - 1 >= nlabel))
				return -1;
			new->path = mkqpath(Dlabel, label[i - 1]->id, 0, 0);
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
		{
			Page *p = page[dir_i1];
			new->type = IXP_QTDIR;
			if(!strncmp(wname, "new", 4)) {
				if(iswalk) {
					Area *a = new_column(p->area[p->sel]);
					if(!a)
						return -1;
					new->path = mkqpath(Darea, p->id, a->id, 0);
				}
				else
					new->path = mkqpath(Darea, p->id, 0, 0); /* simple stat */
			}
			else if(!strncmp(wname, "sel", 4)) {
				new->path = mkqpath(Darea, p->id, p->area[p->sel]->id, 0);
			}
			else {
				i = cext_strtonum(wname, 1, 0xffff, &err);
				if(err || (i - 1 >= p->narea))
					return -1;
				new->path = mkqpath(Darea, p->id, p->area[i - 1]->id, 0);
			}
		}
		break;
	case Dclient:
		{
			Page *p = page[dir_i1];
			Area *a = p->area[dir_i2];
			new->type = IXP_QTDIR;
			if(!strncmp(wname, "sel", 4)) {
				if(!a->nclient)
					return -1;
				new->path = mkqpath(Dclient, p->id, a->id, a->client[a->sel]->id);
			}
			else {
				i = cext_strtonum(wname, 1, 0xffff, &err);
				if(err || (i - 1 >= a->nclient))
					return -1;
				new->path = mkqpath(Dclient, p->id, a->id, a->client[i - 1]->id);
			}
		}
		break;
	case Fkey:
		{
			Key *k;
			if(!(k = name_to_key(wname)))
				return -1;
			new->type = IXP_QTFILE;
			new->path = mkqpath(Fkey, k->id, 0, 0);
		}
		break;
	case Fdata:
	case Fcolors:
		if((dir_type == Dlabel) && (dir_i1 >= nlabel))
			return -1;
	default:
		new->type = IXP_QTFILE;
		new->path = mkqpath(type, qpath_i1id(dir->path), qpath_i2id(dir->path), qpath_i3id(dir->path));
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
    if(fcall->nwname) {
        dir = m->qid;
        for(nwqid = 0; (nwqid < fcall->nwname)
            && !mkqid(&dir, fcall->wname[nwqid], &fcall->wqid[nwqid], True); nwqid++)
		{
            dir = fcall->wqid[nwqid];
		}
        if(!nwqid) {
			fprintf(stderr, "%s", "xwalk: no such file\n");
			return Enofile;
		}
    }
    /* a fid will only be valid, if the walk was complete */
    if(nwqid == fcall->nwname) {
		/*fprintf(stderr, "wm: xwalk4: newfid=%d\n", fcall->newfid);*/
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
xcreate(IXPConn *c, Fcall *fcall)
{
    IXPMap *m = ixp_server_fid2map(c, fcall->fid);

    if(!(fcall->mode | IXP_OWRITE))
        return Enomode;
    if(!m)
        return Enofid;
	if(!strncmp(fcall->name, ".", 2) || !strncmp(fcall->name, "..", 3))
		return "illegal file name";
	if(qpath_type(m->qid.path) != Dkeys)
		return Enoperm;
	grab_key(create_key(fcall->name));
	mkqid(&m->qid, fcall->name, &m->qid, False);
    fcall->id = RCREATE;
    fcall->qid = m->qid;
    fcall->iounit = WMII_IOUNIT;
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
    fcall->iounit = WMII_IOUNIT;
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
type_to_stat(Stat *stat, char *wname, Qid *dir)
{
	unsigned char dir_type;
	int dir_i1 = 0, dir_i2 = 0, dir_i3 = 0;
	int type;
	char buf[32];
	Client *c;

	decode_qpath(dir, &dir_type, &dir_i1, &dir_i2, &dir_i3);
	if((dir_i1 == -1) || (dir_i2 == -1) || (dir_i3 == -1))
		return -1;
	type = name_to_type(wname, dir_type);

    switch (type) {
    case Dclient:
    case Darea:
    case Dpage:
    case Ddef:
	case Dkeys:
	case Dbar:
	case Dlabel:
    case Droot:
		return mkstat(stat, dir, wname, 0, DMDIR | DMREAD | DMEXEC);
        break;
	case Fctl:
    case Fevent:
		return mkstat(stat, dir, wname, 0, DMREAD);
		break;
    case Fborder:
		if(dir_type == Ddef)
			snprintf(buf, sizeof(buf), "%d", def.border);
		else
			snprintf(buf, sizeof(buf), "%d", page[dir_i1]->area[dir_i2]->client[dir_i3]->frame.border);
		return mkstat(stat, dir, wname, strlen(buf), DMREAD | DMWRITE);
        break;
    case Fbar:
		if(dir_type == Ddef)
			snprintf(buf, sizeof(buf), "%d", def.bar);
		else
			snprintf(buf, sizeof(buf), "%d", page[dir_i1]->area[dir_i2]->client[dir_i3]->frame.bar);
		return mkstat(stat, dir, wname, strlen(buf), DMREAD | DMWRITE);
        break;
    case Finc:
		snprintf(buf, sizeof(buf), "%d", def.inc);
		return mkstat(stat, dir, wname, strlen(buf), DMREAD | DMWRITE);
        break;
    case Fgeom:
		c = page[dir_i1]->area[dir_i2]->client[dir_i3];
		snprintf(buf, sizeof(buf), "%d %d %d %d", c->frame.rect.x, c->frame.rect.y,
				c->frame.rect.width, c->frame.rect.height);
		return mkstat(stat, dir, wname, strlen(buf), DMREAD | DMWRITE);
        break;
    case Fsnap:
		snprintf(buf, sizeof(buf), "%d", def.snap);
		return mkstat(stat, dir, wname, strlen(buf), DMREAD | DMWRITE);
		break;
    case Fname:
		c = page[dir_i1]->area[dir_i2]->client[dir_i3];
		return mkstat(stat, dir, wname, strlen(c->name), DMREAD);
        break;
    case Fkey:
		return mkstat(stat, dir, wname, 0, DMWRITE);
		break;
    case Fexpand:
		snprintf(buf, sizeof(buf), "%u", iexpand + 1);
		return mkstat(stat, dir, wname, strlen(buf), DMREAD | DMWRITE);
		break;
    case Fdata:
		return mkstat(stat, dir, wname, (dir_i1 == nlabel) ? 0 : strlen(label[dir_i1]->data), DMREAD | DMWRITE);
		break;	
    case Fcolors:
    case Fselcolors:
    case Fnormcolors:
		return mkstat(stat, dir, wname, 23, DMREAD | DMWRITE);
    case Ffont:
		return mkstat(stat, dir, wname, strlen(def.font), DMREAD | DMWRITE);
		break;
    }
	return 0;
}

static char *
xremove(IXPConn *c, Fcall *fcall)
{
    IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	unsigned char type;
	int i1 = 0, i2 = 0, i3 = 0;

    if(!m)
        return Enofid;
	decode_qpath(&m->qid, &type, &i1, &i2, &i3);
	if((i1 == -1) || (i2 == -1) || (i3 == -1))
		return Enofile;
	switch(type) {
	case Dlabel:
		{
			Label *l = label[i1];
			/* clunk */
			cext_array_detach((void **)c->map, m, &c->mapsz);
			free(m);
			/* now detach the label */
			detach_label(l);
			free(l);
			if(iexpand >= nlabel)
				iexpand = 0;
			draw_bar();
		}
		break;
	case Fkey:
		{
			Key *k = key[i1];
			ungrab_key(k);
			destroy_key(k);
		}
		break;
	default:
		return Enoperm;
		break;
	}
    fcall->id = RREMOVE;
	ixp_server_respond_fcall(c, fcall);
	return nil;
}

static char *
xread(IXPConn *c, Fcall *fcall)
{
	Stat stat;
    IXPMap *m = ixp_server_fid2map(c, fcall->fid);
    unsigned char *p = fcall->data;
	unsigned int i, len;
	char buf[32];
	unsigned char type;
	int i1 = 0, i2 = 0, i3 = 0;
	Client *client;

    if(!m)
        return Enofid;
	decode_qpath(&m->qid, &type, &i1, &i2, &i3);
	if((i1 == -1) || (i2 == -1) || (i3 == -1))
		return Enofile;

	fcall->count = 0;
	if(fcall->offset) {
		switch (type) {
		case Droot:
			/* jump to offset */
			len = type_to_stat(&stat, "ctl", &m->qid);
			len += type_to_stat(&stat, "event", &m->qid);
			len += type_to_stat(&stat, "def", &m->qid);
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
			len = 0;
			for(i = 0; i < nkey; i++) {
				len += type_to_stat(&stat, key[i]->name, &m->qid);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < nkey; i++) {
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
			for(i = 0; i < nlabel; i++) {
				snprintf(buf, sizeof(buf), "%u", i + 1);
				len += type_to_stat(&stat, buf, &m->qid);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < nlabel; i++) {
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
			for(i = 0; i < page[i1]->narea; i++) {
				if(i == page[i1]->sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i + 1);
				len += type_to_stat(&stat, buf, &m->qid);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < page[i1]->narea; i++) {
				if(i == page[i1]->sel)
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
			for(i = 0; i < page[i1]->area[i2]->nclient; i++) {
				if(i == page[i1]->area[i2]->sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i + 1);
				len += type_to_stat(&stat, buf, &m->qid);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < page[i1]->area[i2]->nclient; i++) {
				if(i == page[i1]->area[i2]->sel)
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
			memcpy(&c->pending, fcall, sizeof(Fcall));
			c->is_pending = 1;
			return nil;
			break;
		default:
			break;
		}
	}
	else {
		switch (type) {
		case Droot:
			fcall->count = type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "event", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "def", &m->qid);
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
			for(i = 0; i < nkey; i++) {
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
			for(i = 0; i < nlabel; i++) {
				snprintf(buf, sizeof(buf), "%u", i + 1);
				len = type_to_stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Dlabel:
			if(i1 >= nlabel)
				return Enofile;
			fcall->count = type_to_stat(&stat, "colors", &m->qid);
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
			fcall->count += type_to_stat(&stat, "selcolors", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "normcolors", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "font", &m->qid);
			p = ixp_enc_stat(p, &stat);
			break;
		case Dpage:
			fcall->count = type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "new", &m->qid);
			p = ixp_enc_stat(p, &stat);
			for(i = 0; i < page[i1]->narea; i++) {
				if(i == page[i1]->sel)
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
			for(i = 0; i < page[i1]->area[i2]->nclient; i++) {
				if(i == page[i1]->area[i2]->sel)
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
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "bar", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "name", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "geometry", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type_to_stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			break;
		case Fctl:
			return Enoperm;
			break;
		case Fevent:
			memcpy(&c->pending, fcall, sizeof(Fcall));
			c->is_pending = 1;
			return nil;
			break;
		case Fborder:
			if(m->qid.dir_type == Ddef)
				snprintf(buf, sizeof(buf), "%u", def.border);
			else
				snprintf(buf, sizeof(buf), "%u", page[i1]->area[i2]->client[i3]->frame.border);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case Fbar:
			if(m->qid.dir_type == Ddef)
				snprintf(buf, sizeof(buf), "%u", def.bar);
			else
				snprintf(buf, sizeof(buf), "%u", page[i1]->area[i2]->client[i3]->frame.bar);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case Finc:
			snprintf(buf, sizeof(buf), "%u", def.inc);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case Fgeom:
			client = page[i1]->area[i2]->client[i3];
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
			if((fcall->count = strlen(page[i1]->area[i2]->client[i3]->name)))
				memcpy(p, page[i1]->area[i2]->client[i3]->name, fcall->count);
			break;
		case Fexpand:
			snprintf(buf, sizeof(buf), "%u", iexpand + 1);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case Fdata:
			if(i1 >= nlabel)
				return Enofile;
			if((fcall->count = strlen(label[i1]->data)))
				memcpy(p, label[i1]->data, fcall->count);
			break;
		case Fcolors:
			if(i1 >= nlabel)
				return Enofile;
			if((fcall->count = strlen(label[i1]->colstr)))
				memcpy(p, label[i1]->colstr, fcall->count);
			break;
		case Fselcolors:
			if((fcall->count = strlen(def.selcolor)))
				memcpy(p, def.selcolor, fcall->count);
			break;
		case Fnormcolors:
			if((fcall->count = strlen(def.normcolor)))
				memcpy(p, def.normcolor, fcall->count);
			break;
		case Ffont:
			if((fcall->count = strlen(def.font)))
				memcpy(p, def.font, fcall->count);
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
	unsigned short i;
	unsigned char type;
	int i1 = 0, i2 = 0, i3 = 0;
	Client *cl;

    if(!m)
        return Enofid;
	decode_qpath(&m->qid, &type, &i1, &i2, &i3);
	if((i1 == -1) || (i2 == -1) || (i3 == -1))
		return Enofile;

	switch (qpath_type(m->qid.path)) {
	case Fctl:
		if(fcall->count > sizeof(buf) - 1)
			return Enocommand;
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		switch(m->qid.dir_type) {
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
			else if(!strncmp(buf, "warp ", 5)) {
				char *err;
				if((err = warp_mouse(&buf[5])))
					return err;
			}
			else
				return Enocommand;
			break;
		case Dpage:
			break;
		case Darea:
			if(!strncmp(buf, "select ", 7)) {
			   Area *a = page[i1]->area[i2];
			   if(a->nclient)
				   select_client(a->client[a->sel], &buf[7]);
			}
			break;
		case Dclient:
			cl = page[i1]->area[i2]->client[i3];
			if(!strncmp(buf, "kill", 5))
				kill_client(cl);
			if(!strncmp(buf, "detach", 7))
				detach_client(cl, False);
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
		if(m->qid.dir_type == Ddef)
			def.border = i;
		else {
			Client *c = page[i1]->area[i2]->client[i3];
			c->frame.border = i;
			resize_client(c, &c->frame.rect, 0);
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
		if(m->qid.dir_type == Ddef)
			def.border = i;
		else {
			Client *c = page[i1]->area[i2]->client[i3];
			c->frame.bar = i;
			resize_client(c, &c->frame.rect, 0);
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
		for(i = 0; i < nclient; i++)
			if(client[i]->area)
				resize_client(client[i], &client[i]->frame.rect, 0);
		break;
	case Fgeom:
		cl = page[i1]->area[i2]->client[i3];
		if(fcall->count > sizeof(buf))
			return "geometry values out of range";
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		blitz_strtorect(&rect, &cl->frame.rect, buf);
		resize_client(cl, &cl->frame.rect, 0);
		break;
    case Fexpand:
		{
			const char *err;
			if(fcall->count && fcall->count < 16) {
				memcpy(buf, fcall->data, fcall->count);
				buf[fcall->count] = 0;
				i = (unsigned short) cext_strtonum(buf, 1, 0xffff, &err);
				if(!err && (i - 1 < nlabel)) {
					iexpand = i - 1;
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
			if(len >= sizeof(label[i1]->data))
				len = sizeof(label[i1]->data) - 1;
			memcpy(label[i1]->data, fcall->data, len);
			label[i1]->data[len] = 0;
			draw_bar();
		}
		break;
	case Fcolors:
		if((i1 >= nlabel) || (fcall->count != 23)
			|| (fcall->data[0] != '#') || (fcall->data[8] != '#')
		    || (fcall->data[16] != '#')
		  )
			return "wrong color format";
		memcpy(label[i1]->colstr, fcall->data, fcall->count);
		label[i1]->colstr[fcall->count] = 0;
		blitz_loadcolor(dpy, screen, label[i1]->colstr, &label[i1]->color);
		draw_bar();
		break;
	case Fselcolors:
		if((fcall->count != 23)
			|| (fcall->data[0] != '#') || (fcall->data[8] != '#')
		    || (fcall->data[16] != '#')
		  )
			return "wrong color format";
		memcpy(def.selcolor, fcall->data, fcall->count);
		def.selcolor[fcall->count] = 0;
		blitz_loadcolor(dpy, screen, def.selcolor, &def.sel);
		for(i = 0; i < nclient; i++)
			if(client[i]->area->page == page[sel])
				draw_client(client[i]);
		break;
	case Fnormcolors:
		if((fcall->count != 23)
			|| (fcall->data[0] != '#') || (fcall->data[8] != '#')
		    || (fcall->data[16] != '#')
		  )
			return "wrong color format";
		memcpy(def.normcolor, fcall->data, fcall->count);
		def.normcolor[fcall->count] = 0;
		blitz_loadcolor(dpy, screen, def.normcolor, &def.norm);
		for(i = 0; i < nclient; i++)
			if(client[i]->area->page == page[sel])
				draw_client(client[i]);
		break;
	case Ffont:
		if(def.font)
			free(def.font);
		def.font = cext_emallocz(fcall->count + 1);
		memcpy(def.font, fcall->data, fcall->count);
		XFreeFont(dpy, xfont);
    	xfont = blitz_getfont(dpy, def.font);
		for(i = 0; i < nclient; i++) {
			if(!client[i]->area->page)
				continue;
			resize_client(client[i], &client[i]->frame.rect, 0);
		}
		update_bar_geometry();
		break;
	case Fkey:
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
		case TCREATE: errstr = xcreate(c, &fcall); break;
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
write_event(char *event)
{
	size_t i;
	for(i = 0; (i < srv.connsz) && srv.conn[i]; i++) {
		IXPConn *c = srv.conn[i];
		if(c->is_pending) {
			/* pending reads on /event only, no qid checking */
			IXPMap *m = ixp_server_fid2map(c, c->pending.fid);
			unsigned char *p = c->pending.data;
			if(!m) {
				if(ixp_server_respond_error(c, &c->pending, Enofid))
					break;
			}
			else if(qpath_type(m->qid.path) == Fevent) {
				c->pending.count = strlen(event);
				memcpy(p, event, c->pending.count);
				c->pending.id = RREAD;
				if(ixp_server_respond_fcall(c, &c->pending))
					break;
			}
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
