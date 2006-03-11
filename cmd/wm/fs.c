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
static char Ebadvalue[] = "bad value";

#define WMII_IOUNIT 	2048

/*
 * filesystem specification
 * / 					FsDroot
 * /def/				FsDdef
 * /def/border			FsFborder		0..n
 * /def/snap 			FsFsnap  		0..n
 * /def/font			FsFfont  		xlib font name
 * /def/selcolors		FsFselcolors	sel color
 * /def/normcolors		FsFnormcolors 	normal colors
 * /def/rules      		FsFrules 		rules
 * /def/keys       		FsFkeys  		keys
 * /tags				FsFtags
 * /bar/				FsDbar
 * /bar/expand			FsFexpand 		id of expandable label
 * /bar/lab/			FsDlabel
 * /bar/lab/data 		FsFdata			<arbitrary data which gets displayed>
 * /bar/lab/colors		FsFcolors		<#RRGGBB> <#RRGGBB> <#RRGGBB>
 * /clients/			FsDclients
 * /clients/1/			FsDGclient		see /ws/X/X/ namespace below
 * /event				FsFevent
 * /ctl					FsFctl 			command interface (root)
 * /ws/				    FsDws			ws
 * /ws/tag				FsFtag			current tag
 * /ws/ctl				FsFctl			command interface (tag)
 * /ws/sel/				FsDarea
 * /ws/1/				FsDarea
 * /ws/1/ctl 			FsFctl 			command interface (area)
 * /ws/1/mode			FsFmode			col mode
 * /ws/1/sel/			FsDclient
 * /ws/1/1/class		FsFclass		class:instance of client
 * /ws/1/1/name			FsFname			name of client
 * /ws/1/1/tags			FsFtags			tag of client
 * /ws/1/1/geom			FsFgeom			geometry of client
 * /ws/1/1/ctl 			FsFctl 			command interface (client)
 */

Qid root_qid;
const char *err;

/* IXP stuff */

/**
 * Qid->path is calculated related to the index of the associated structure.
 * i1 is associated to tag, key, global client, or label
 * i2 is associated to area
 * i3 is associated to client
 * ie /ws/sel/ctl is i1id = sel tag id, i2id = sel area id , i3id = 0 (no id)
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
		if(qid->dir_type == FsDGclient || qid->dir_type == FsDclients)
			*i1 = cid2index(i1id);
		else {
			switch(*type) {
				case FsFdata:
				case FsFcolors:
				case FsDlabel: *i1 = lid2index(i1id); break;
				default: *i1 = tid2index(i1id); break;
			}
		}
		if(i2id && (*i1 != -1)) {
			*i2 = aid2index(tag[*i1], i2id);
			if(i3id && (*i2 != -1))
				*i3 = frid2index(tag[*i1]->area[*i2], i3id);
		}
	}
}

static char *
qid2name(Qid *qid)
{
	unsigned char type;
	int i1 = -1, i2 = -1, i3 = -1;
	static char buf[256];

	decode_qpath(qid, &type, &i1, &i2, &i3);

	switch(type) {
		case FsDroot: return "/"; break;
		case FsDdef: return "def"; break;
		case FsDclients: return "clients"; break;
		case FsDbar: return "bar"; break;
		case FsDws:
			if(qid->dir_type == FsDroot)
				return "ws";
			else {
				if(i1 == -1)
					return nil;
				return tag[i1]->name;
			}
			break;
		case FsDlabel:
			if(i1 == -1)
				return nil;
			return label[i1]->name;
			break;
		case FsDarea:
			if(i1 == -1 || i2 == -1)
				return nil;
			if(tag[i1]->sel == i2)
				return "sel";
			snprintf(buf, sizeof(buf), "%u", i2);
			return buf;
			break;
		case FsDGclient:
			if(i1 == -1)
				return nil;
			snprintf(buf, sizeof(buf), "%u", i1);
			return buf;
			break;
		case FsDclient:
			if(i1 == -1 || i2 == -1 || i3 == -1)
				return nil;
			if(tag[i1]->area[i2]->sel == i3)
				return "sel";
			snprintf(buf, sizeof(buf), "%u", i3);
			return buf;
			break;
		case FsFselcolors: return "selcolors"; break;
		case FsFnormcolors: return "normcolors"; break;
		case FsFfont: return "font"; break;
		case FsFrules: return "rules"; break;
		case FsFkeys: return "keys"; break;
		case FsFcolors: return "colors"; break;
		case FsFdata:
			if(i1 == -1)
				return nil;
			return "data";
			break;
		case FsFexpand:
			if(i1 == -1)
				return nil;
			return "expand";
			break;
		case FsFctl: return "ctl"; break;
		case FsFborder: return "border"; break;
		case FsFsnap: return "snap"; break;
		case FsFgeom:
			if((qid->dir_type == FsDclient) && (i1 == -1 || i2 == -1 || i3 == -1))
				return nil;
			else if(i1 == -1)
				return nil;
			return "geom";
			break;
		case FsFclass:
		case FsFname:
			if((qid->dir_type == FsDclient) && (i1 == -1 || i2 == -1 || i3 == -1))
				return nil;
			else if(i1 == -1)
				return nil;
			if(type == FsFname)
				return "name";
			else 
				return "class";
			break;
		case FsFtags:
			if((qid->dir_type == FsDclient) && (i1 == -1 || i2 == -1 || i3 == -1))
				return nil;
			else if((qid->dir_type == FsDGclient) && (i1 == -1))
				return nil;
		 	return "tags";
			break;
		case FsFmode:
			if(i1 == -1 || i2 == -1)
				return nil;
			return "mode";
			break;
		case FsFevent: return "event"; break;
		default: return nil; break;
	}
}

static unsigned char
name2type(char *name, unsigned char dir_type)
{
    unsigned int i;
	if(!name || !name[0] || !strncmp(name, "/", 2) || !strncmp(name, "..", 3))
		return FsDroot;
	if(!strncmp(name, "tags", 5))
		return FsFtags;
	if(!strncmp(name, "clients", 8))
		return FsDclients;
	if(!strncmp(name, "ws", 3) && (dir_type == FsDroot))
		return FsDws;
	if(!strncmp(name, "bar", 4))
		return FsDbar;
	if(!strncmp(name, "def", 4))
		return FsDdef;
	if(!strncmp(name, "ctl", 4))
		return FsFctl;
	if(!strncmp(name, "event", 6))
		return FsFevent;
	if(!strncmp(name, "snap", 5))
		return FsFsnap;
	if(!strncmp(name, "class", 5))
		return FsFclass;
	if(!strncmp(name, "name", 5))
		return FsFname;
	if(!strncmp(name, "border", 7))
		return FsFborder;
	if(!strncmp(name, "geom", 5))
		return FsFgeom;
	if(!strncmp(name, "expand", 7))
		return FsFexpand;
	if(!strncmp(name, "colors", 7))
		return FsFcolors;
	if(!strncmp(name, "selcolors", 10))
		return FsFselcolors;
	if(!strncmp(name, "normcolors", 11))
		return FsFnormcolors;
	if(!strncmp(name, "font", 5))
		return FsFfont;
	if(!strncmp(name, "keys", 5))
		return FsFkeys;
	if(!strncmp(name, "rules", 6))
		return FsFrules;
	if(!strncmp(name, "data", 5))
		return FsFdata;
	if(!strncmp(name, "mode", 5))
		return FsFmode;
	if(!strncmp(name, "tag", 4))
		return FsFtag;
	if((dir_type == FsDbar) && name2label(name))
		return FsDlabel;
	if(!strncmp(name, "sel", 4))
		goto dyndir;
   	i = (unsigned short) cext_strtonum(name, 0, 0xffff, &err);
    if(err)
		return -1;
dyndir:
	/*fprintf(stderr, "nametotype: dir_type = %d\n", dir_type);*/
	switch(dir_type) {
	case FsDbar: return FsDlabel; break;
	case FsDws: return FsDarea; break;
	case FsDclients: return FsDGclient; break;
	case FsDarea: return FsDclient; break;
	}
	return -1;
}

static int
mkqid(Qid *dir, char *wname, Qid *new)
{
	unsigned char dir_type;
	int dir_i1 = -1, dir_i2 = -1, dir_i3 = -1, i;
	unsigned char type;

	decode_qpath(dir, &dir_type, &dir_i1, &dir_i2, &dir_i3);
	type = name2type(wname, dir_type);

	new->dir_type = dir_type;
    new->version = 0;
	switch(type) {
	case FsDroot:
		*new = root_qid;
		break;
	case FsDdef:
	case FsDclients:
	case FsDbar:
		if(dir_type != FsDroot)
			return -1;
		new->type = IXP_QTDIR;
		new->path = mkqpath(type, 0, 0, 0);
		break;
	case FsDws:
		new->type = IXP_QTDIR;
		new->path = mkqpath(FsDws, ntag ? tag[sel]->id : 0, 0, 0);
		break;
	case FsDarea:
		if(dir_i1 == -1 || dir_type != FsDws)
			return -1;
		{
			Tag *p = tag[dir_i1];
			new->type = IXP_QTDIR;
			if(!strncmp(wname, "sel", 4)) {
				new->path = mkqpath(FsDarea, p->id, p->area[p->sel]->id, 0);
			}
			else {
				i = cext_strtonum(wname, 0, 0xffff, &err);
				if(err || (i >= p->narea))
					return -1;
				new->path = mkqpath(FsDarea, p->id, p->area[i]->id, 0);
			}
		}
		break;
	case FsDclient:
		if(dir_i1 == -1 || dir_i2 == -1 || dir_type != FsDarea)
			return -1;
		{
			Tag *p = tag[dir_i1];
			Area *a = p->area[dir_i2];
			new->type = IXP_QTDIR;
			if(!strncmp(wname, "sel", 4)) {
				if(!a->nframe)
					return -1;
				new->path = mkqpath(FsDclient, p->id, a->id, a->frame[a->sel]->id);
			}
			else {
				i = cext_strtonum(wname, 0, 0xffff, &err);
				if(err || (i >= a->nframe))
					return -1;
				new->path = mkqpath(FsDclient, p->id, a->id, a->frame[i]->id);
			}
		}
		break;
	case FsDGclient:
		if(dir_type != FsDclients)
			return -1;
		i = cext_strtonum(wname, 0, 0xffff, &err);
		if(err || (i >= nclient))
			return -1;
		new->path = mkqpath(FsDGclient, client[i]->id, 0, 0);
		break;
	case FsDlabel:
		if(dir_type !=  FsDbar)
			return -1;
		{
			Label *l;
			if(!(l = name2label(wname)))
				return -1;
			new->type = IXP_QTDIR;
			new->path = mkqpath(FsDlabel, l->id, 0, 0);
		}
		break;
	case FsFdata:
	case FsFcolors:
		if((dir_i1 == -1) || (dir_type != FsDlabel))
			return -1;
		new->type = IXP_QTFILE;
		new->path = mkqpath(type, qpath_i1id(dir->path), qpath_i2id(dir->path), qpath_i3id(dir->path));
		break;
	case FsFmode:
		if(dir_i1 == -1 || dir_i2 == -1 || dir_type != FsDarea)
			return -1;
		new->type = IXP_QTFILE;
		new->path = mkqpath(type, qpath_i1id(dir->path), qpath_i2id(dir->path), qpath_i3id(dir->path));
		break;
	case FsFgeom:
	case FsFname:
	case FsFclass:
	case FsFtags:
		if((dir_type == FsDclient) && ((dir_i1 == -1 || dir_i2 == -1 || dir_i3 == -1)))
			return -1;
		else if((dir_type == FsDGclient) && (dir_i1 == -1))
			return -1;
		new->type = IXP_QTFILE;
		new->path = mkqpath(type, qpath_i1id(dir->path), qpath_i2id(dir->path), qpath_i3id(dir->path));
		break;
	case FsFborder:
	case FsFfont:
	case FsFrules:
	case FsFselcolors:
	case FsFnormcolors:
	case FsFsnap:
	case FsFkeys:
		if(dir_type != FsDdef) 
			return -1;
	default:
		new->type = IXP_QTFILE;
		new->path = mkqpath(type, qpath_i1id(dir->path), qpath_i2id(dir->path), qpath_i3id(dir->path));
		break;
	}
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
type2stat(Stat *stat, char *wname, Qid *dir)
{
	unsigned char dir_type, type;
	int dir_i1 = 0, dir_i2 = 0, dir_i3 = 0;
	char buf[256];
	Frame *f;

	decode_qpath(dir, &dir_type, &dir_i1, &dir_i2, &dir_i3);
	if((dir_i1 == -1) || (dir_i2 == -1) || (dir_i3 == -1))
		return -1;
	type = name2type(wname, dir_type);

    switch (type) {
    case FsDclient:
    case FsDGclient:
    case FsDarea:
    case FsDws:
    case FsDdef:
	case FsDclients:
	case FsDlabel:
    case FsDroot:
		return mkstat(stat, dir, wname, 0, DMDIR | DMREAD | DMEXEC);
        break;
	case FsDbar:
		return mkstat(stat, dir, wname, 0, DMDIR | DMREAD | DMWRITE | DMEXEC);
        break;
	case FsFctl:
		return mkstat(stat, dir, wname, 0, DMWRITE);
		break;
    case FsFevent:
		return mkstat(stat, dir, wname, 0, DMREAD);
		break;
    case FsFborder:
		snprintf(buf, sizeof(buf), "%d", def.border);
		return mkstat(stat, dir, wname, strlen(buf), DMREAD | DMWRITE);
        break;
    case FsFgeom:
		if(dir_type == FsDclient) {
			f = tag[dir_i1]->area[dir_i2]->frame[dir_i3];
			snprintf(buf, sizeof(buf), "%d %d %d %d", f->rect.x, f->rect.y,
					f->rect.width, f->rect.height);
		}
		else {
			Client *c = client[dir_i1];
			snprintf(buf, sizeof(buf), "%d %d %d %d", c->rect.x, c->rect.y,
					c->rect.width, c->rect.height);
		}
		return mkstat(stat, dir, wname, strlen(buf), DMREAD | DMWRITE);
        break;
    case FsFsnap:
		snprintf(buf, sizeof(buf), "%d", def.snap);
		return mkstat(stat, dir, wname, strlen(buf), DMREAD | DMWRITE);
		break;
    case FsFclass:
		if(dir_type == FsDclient) {
			f = tag[dir_i1]->area[dir_i2]->frame[dir_i3];
			return mkstat(stat, dir, wname, strlen(f->client->classinst), DMREAD);
		}
		else 
			return mkstat(stat, dir, wname, strlen(client[dir_i1]->classinst), DMREAD);
        break;
    case FsFname:
		if(dir_type == FsDclient) {
			f = tag[dir_i1]->area[dir_i2]->frame[dir_i3];
			return mkstat(stat, dir, wname, strlen(f->client->name), DMREAD);
		}
		else 
			return mkstat(stat, dir, wname, strlen(client[dir_i1]->name), DMREAD);
        break;
    case FsFtags:
		switch(dir_type) {
		case FsDclient:
			f = tag[dir_i1]->area[dir_i2]->frame[dir_i3];
			return mkstat(stat, dir, wname, strlen(f->client->tags), DMREAD | DMWRITE);
			break;
		case FsDGclient:
			return mkstat(stat, dir, wname, strlen(client[dir_i1]->tags), DMREAD | DMWRITE);
			break;
		default:
			{
				unsigned int i, len = 0;
				for(i = 0; i < nctag; i++)
					len += strlen(ctag[i]) + 1;
				return mkstat(stat, dir, wname, len, DMREAD);
			}
			break;
		}
		break;
	case FsFtag:
		if(dir_type == FsDws)
			return mkstat(stat, dir, wname, strlen(def.tag), DMREAD | DMWRITE);
		return mkstat(stat, dir, wname, 0, 0);
		break;
    case FsFexpand:
		return mkstat(stat, dir, wname, strlen(expand), DMREAD | DMWRITE);
		break;
    case FsFdata:
		return mkstat(stat, dir, wname, (dir_i1 == nlabel) ? 0 : strlen(label[dir_i1]->data), DMREAD | DMWRITE);
		break;	
    case FsFmode:
		return mkstat(stat, dir, wname, strlen(mode2str(tag[dir_i1]->area[dir_i2]->mode)), DMREAD | DMWRITE);
		break;	
    case FsFcolors:
    case FsFselcolors:
    case FsFnormcolors:
		return mkstat(stat, dir, wname, 23, DMREAD | DMWRITE);
		break;
    case FsFkeys:
		return mkstat(stat, dir, wname, def.keys ? strlen(def.keys) : 0, DMREAD | DMWRITE);
		break;
    case FsFrules:
		return mkstat(stat, dir, wname, def.rules ? strlen(def.rules) : 0, DMREAD | DMWRITE);
		break;
    case FsFfont:
		return mkstat(stat, dir, wname, strlen(def.font), DMREAD | DMWRITE);
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
            dir = fcall->wqid[nwqid];
		}
        if(!nwqid) {
			fprintf(stderr, "%s", "xwalk: no such file\n");
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
xcreate(IXPConn *c, Fcall *fcall)
{
    IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	unsigned char type;

    if(!(fcall->mode | IXP_OWRITE))
        return Enomode;
    if(!m)
        return Enofid;
	if(!strncmp(fcall->name, ".", 2) || !strncmp(fcall->name, "..", 3))
		return "illegal file name";
	type = qpath_type(m->qid.path);
	switch(type) {
	case FsDbar:
		if(!strncmp(fcall->name, "expand", 7))
			return "illegal file name";
		get_label(fcall->name);
		break;
	default:
		return Enoperm;
		break;
	}
	mkqid(&m->qid, fcall->name, &m->qid);
	fcall->qid = m->qid;
	fcall->id = RCREATE;
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
	if(type != FsDlabel)
		return Enoperm;
	/* clunk */
	cext_array_detach((void **)c->map, m, &c->mapsz);
	free(m);
	switch(type) {
	case FsDlabel:
		{
			Label *l = label[i1];
			/* now detach the label */
			destroy_label(l);
			free(l);
			draw_bar();
		}
		break;
	default:
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
	char buf[256];
	unsigned char type;
	int i1 = 0, i2 = 0, i3 = 0;
	Frame *f;

    if(!m)
        return Enofid;
	decode_qpath(&m->qid, &type, &i1, &i2, &i3);
	if((i1 == -1) || (i2 == -1) || (i3 == -1))
		return Enofile;

	fcall->count = 0;
	if(fcall->offset) {
		switch (type) {
		case FsDclients:
			/* jump to offset */
			len = 0;
			for(i = 0; i < nclient; i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len += type2stat(&stat, buf, &m->qid);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < nclient; i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len = type2stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case FsDbar:
			/* jump to offset */
			len = type2stat(&stat, "expand", &m->qid);
			for(i = 0; i < nlabel; i++) {
				len += type2stat(&stat, label[i]->name, &m->qid);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < nlabel; i++) {
				len = type2stat(&stat, label[i]->name, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case FsDws:
			/* jump to offset */
			len = type2stat(&stat, "ctl", &m->qid);
			len += type2stat(&stat, "tag", &m->qid);
			for(i = 0; i < tag[i1]->narea; i++) {
				if(i == tag[i1]->sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i);
				len += type2stat(&stat, buf, &m->qid);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < tag[i1]->narea; i++) {
				if(i == tag[i1]->sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i);
				len = type2stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case FsDarea:
			/* jump to offset */
			len = type2stat(&stat, "ctl", &m->qid);
			if(i2)
				len += type2stat(&stat, "mode", &m->qid);
			for(i = 0; i < tag[i1]->area[i2]->nframe; i++) {
				if(i == tag[i1]->area[i2]->sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i);
				len += type2stat(&stat, buf, &m->qid);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; i < tag[i1]->area[i2]->nframe; i++) {
				if(i == tag[i1]->area[i2]->sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i);
				len = type2stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case FsFevent:
			memcpy(&c->pending, fcall, sizeof(Fcall));
			c->is_pending = 1;
			return nil;
			break;
		case FsFkeys:
			len = def.keys ? strlen(def.keys) : 0;
			if(len <= fcall->offset) {
				fcall->count = 0;
				break;
			}
			fcall->count = len - fcall->offset;
			if(fcall->count > fcall->iounit) {
				memcpy(p, def.keys + fcall->offset, fcall->iounit);
				fcall->count = fcall->iounit;
			}
			else if(fcall->count)
				memcpy(p, def.keys + fcall->offset, fcall->count);
			break;
		case FsFrules:
			len = def.rules ? strlen(def.rules) : 0;
			if(len <= fcall->offset) {
				fcall->count = 0;
				break;
			}
			fcall->count = len - fcall->offset;
			if(fcall->count > fcall->iounit) {
				memcpy(p, def.rules + fcall->offset, fcall->iounit);
				fcall->count = fcall->iounit;
			}
			else if(fcall->count)
				memcpy(p, def.rules + fcall->offset, fcall->count);
			break;
		case FsFtags:
			if(m->qid.dir_type != FsDroot)
		   		return Enoperm;
			len = 0;
			/* jump to offset */
			for(i = 0; i < nctag; i++) {
				len += strlen(ctag[i]) + 1;
				if(len <= fcall->offset)
					continue;
			}
			/* offset found, proceeding */
			for(; i < nctag; i++) {
				len = strlen(ctag[i]) + 1;
				if(fcall->count + len > fcall->iounit)
					break;
				memcpy(p + fcall->count, ctag[i], len - 1);
				memcpy(p + fcall->count + len - 1, "\n", 1);
				fcall->count += len;
			}
			break;
		default:
			break;
		}
	}
	else {
		switch (type) {
		case FsDroot:
			fcall->count = type2stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "event", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "def", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "bar", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "tags", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "clients", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "ws", &m->qid);
			p = ixp_enc_stat(p, &stat);
			break;
		case FsDclients:
			for(i = 0; i < nclient; i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len = type2stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case FsDbar:
			fcall->count = type2stat(&stat, "expand", &m->qid);
			p = ixp_enc_stat(p, &stat);
			for(i = 0; i < nlabel; i++) {
				len = type2stat(&stat, label[i]->name, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case FsDlabel:
			if(i1 >= nlabel)
				return Enofile;
			fcall->count = type2stat(&stat, "colors", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "data", &m->qid);
			p = ixp_enc_stat(p, &stat);
			break;
		case FsDdef:
			fcall->count = type2stat(&stat, "border", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "snap", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "selcolors", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "normcolors", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "font", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "keys", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "rules", &m->qid);
			p = ixp_enc_stat(p, &stat);
			break;
		case FsDws:
			fcall->count = type2stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "tag", &m->qid);
			p = ixp_enc_stat(p, &stat);
			for(i = 0; i < tag[i1]->narea; i++) {
				if(i == tag[i1]->sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i);
				len = type2stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case FsDarea:
			fcall->count = type2stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			if(i2)
				fcall->count += type2stat(&stat, "mode", &m->qid);
				p = ixp_enc_stat(p, &stat);
			for(i = 0; i < tag[i1]->area[i2]->nframe; i++) {
				if(i == tag[i1]->area[i2]->sel)
					snprintf(buf, sizeof(buf), "%s", "sel");
				else
					snprintf(buf, sizeof(buf), "%u", i);
				len = type2stat(&stat, buf, &m->qid);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case FsDGclient:
		case FsDclient:
			fcall->count = type2stat(&stat, "class", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "name", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "tags", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "geom", &m->qid);
			p = ixp_enc_stat(p, &stat);
			fcall->count += type2stat(&stat, "ctl", &m->qid);
			p = ixp_enc_stat(p, &stat);
			break;
		case FsFctl:
			return Enoperm;
			break;
		case FsFevent:
			memcpy(&c->pending, fcall, sizeof(Fcall));
			c->is_pending = 1;
			return nil;
			break;
		case FsFborder:
			snprintf(buf, sizeof(buf), "%u", def.border);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case FsFgeom:
			if(m->qid.dir_type == FsDclient) {
				f = tag[i1]->area[i2]->frame[i3];
				snprintf(buf, sizeof(buf), "%d %d %d %d", f->rect.x, f->rect.y,
						f->rect.width, f->rect.height);
			}
			else {
				Client *c =  client[i1];
				snprintf(buf, sizeof(buf), "%d %d %d %d", c->rect.x, c->rect.y,
						c->rect.width, c->rect.height);
			}
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case FsFsnap:
			snprintf(buf, sizeof(buf), "%u", def.snap);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case FsFclass:
			if(m->qid.dir_type == FsDclient) {
				if((fcall->count = strlen(tag[i1]->area[i2]->frame[i3]->client->classinst)))
					memcpy(p, tag[i1]->area[i2]->frame[i3]->client->classinst, fcall->count);
			}
			else {
				if((fcall->count = strlen(client[i1]->classinst)))
					memcpy(p, client[i1]->classinst, fcall->count);
			}
			break;
		case FsFname:
			if(m->qid.dir_type == FsDclient) {
				if((fcall->count = strlen(tag[i1]->area[i2]->frame[i3]->client->name)))
					memcpy(p, tag[i1]->area[i2]->frame[i3]->client->name, fcall->count);
			}
			else {
				if((fcall->count = strlen(client[i1]->name)))
					memcpy(p, client[i1]->name, fcall->count);
			}
			break;
		case FsFtags:
			switch(m->qid.dir_type) {
			case FsDclient:
				if((fcall->count = strlen(tag[i1]->area[i2]->frame[i3]->client->tags)))
					memcpy(p, tag[i1]->area[i2]->frame[i3]->client->tags, fcall->count);
				break;
			case FsDGclient:
				if((fcall->count = strlen(client[i1]->tags)))
					memcpy(p, client[i1]->tags, fcall->count);
				break;
			default:
				for(i = 0; i < nctag; i++) {
					len = strlen(ctag[i]) + 1;
					if(fcall->count + len > fcall->iounit)
						break;
					memcpy(p + fcall->count, ctag[i], len - 1);
					memcpy(p + fcall->count + len - 1, "\n", 1);
					fcall->count += len;
				}
				break;
			}
			break;
		case FsFexpand:
			fcall->count = strlen(expand);
			memcpy(p, expand, fcall->count);
			break;
		case FsFdata:
			if(i1 >= nlabel)
				return Enofile;
			if((fcall->count = strlen(label[i1]->data)))
				memcpy(p, label[i1]->data, fcall->count);
			break;
		case FsFtag:
			if((fcall->count = strlen(def.tag)))
				memcpy(p, def.tag, fcall->count);
			break;
		case FsFcolors:
			if(i1 >= nlabel)
				return Enofile;
			if((fcall->count = strlen(label[i1]->colstr)))
				memcpy(p, label[i1]->colstr, fcall->count);
			break;
		case FsFselcolors:
			if((fcall->count = strlen(def.selcolor)))
				memcpy(p, def.selcolor, fcall->count);
			break;
		case FsFnormcolors:
			if((fcall->count = strlen(def.normcolor)))
				memcpy(p, def.normcolor, fcall->count);
			break;
		case FsFkeys:
			fcall->count = def.keys ? strlen(def.keys) : 0;
			if(fcall->count > fcall->iounit) {
				memcpy(p, def.keys, fcall->iounit);
				fcall->count = fcall->iounit;
			}
			else if(fcall->count)
				memcpy(p, def.keys, fcall->count);
			break;
		case FsFrules:
			fcall->count = def.rules ? strlen(def.rules) : 0;
			if(fcall->count > fcall->iounit) {
				memcpy(p, def.rules, fcall->iounit);
				fcall->count = fcall->iounit;
			}
			else if(fcall->count)
				memcpy(p, def.rules, fcall->count);
			break;
		case FsFfont:
			if((fcall->count = strlen(def.font)))
				memcpy(p, def.font, fcall->count);
			break;
		case FsFmode:
			if(!i2)
				return Enofile;
			snprintf(buf, sizeof(buf), "%s", mode2str(tag[i1]->area[i2]->mode));
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;	
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
	name = qid2name(&m->qid);
	if(!type2stat(&fcall->stat, name, &m->qid))
		return Enofile;
    fcall->id = RSTAT;
	ixp_server_respond_fcall(c, fcall);
    return nil;
}

static void
draw_clients()
{
	unsigned int i, j;
	for(i = 0; i < nclient; i++)
		for(j = 0; j < client[i]->nframe; j++)
			if(client[i]->frame[j]->area->tag == tag[sel])
				draw_client(client[i]);
}

static char *
xwrite(IXPConn *c, Fcall *fcall)
{
	char buf[256], *tmp;
    IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	unsigned char type;
	unsigned int len;
	int i, i1 = 0, i2 = 0, i3 = 0;
	Frame *f;

    if(!m)
        return Enofid;
	decode_qpath(&m->qid, &type, &i1, &i2, &i3);
	if((i1 == -1) || (i2 == -1) || (i3 == -1))
		return Enofile;

	switch(type) {
	case FsFctl:
		if(fcall->count > sizeof(buf) - 1)
			return Enocommand;
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		switch(m->qid.dir_type) {
		case FsDroot:
			if(!strncmp(buf, "quit", 5))
				srv.running = 0;
			else if(!strncmp(buf, "select", 6))
				select_tag(&buf[7]);
			else
				return Enocommand;
			break;
		case FsDws:
			if(!strncmp(buf, "select ", 7))
				select_area(tag[i1]->area[tag[i1]->sel], &buf[7]);
			break;
		case FsDarea:
			if(!strncmp(buf, "select ", 7)) {
			   Area *a = tag[i1]->area[i2];
			   if(a->nframe)
				   select_client(a->frame[a->sel]->client, &buf[7]);
			}
			break;
		case FsDclient:
			f = tag[i1]->area[i2]->frame[i3];
			if(!strncmp(buf, "kill", 5))
				kill_client(f->client);
			else if(!strncmp(buf, "sendto ", 7))
				send2area_client(f->client, &buf[7]);
			break;
		case FsDGclient:
			if(!strncmp(buf, "kill", 5))
				kill_client(client[i1]);
			else if(!strncmp(buf, "sendto ", 7))
				send2area_client(client[i1], &buf[7]);
			break;
		default:
			break;
		}
		break;
	case FsFsnap:
		if(fcall->count > sizeof(buf))
			return Ebadvalue;
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		i = cext_strtonum(buf, 0, 0xffff, &err);
		if(err)
			return Ebadvalue;
		def.snap = i;
		break;
	case FsFborder:
		if(fcall->count > sizeof(buf))
			return Ebadvalue;
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		i = cext_strtonum(buf, 0, 0xffff, &err);
		if(err)
			return Ebadvalue;
		def.border = i;
		resize_all_clients();
		break;
	case FsFtags:
		if(m->qid.dir_type == FsDroot)
			return Enoperm;
		if(fcall->count > sizeof(buf))
			return Ebadvalue;
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		if(m->qid.dir_type == FsDclient) {
			f = tag[i1]->area[i2]->frame[i3];
			cext_strlcpy(f->client->tags, buf, sizeof(f->client->tags));
		}
		else
			cext_strlcpy(client[i1]->tags, buf, sizeof(client[i1]->tags));
		update_tags();
		break;
	case FsFgeom:
		if(fcall->count > sizeof(buf))
			return Ebadvalue;
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		if(m->qid.dir_type == FsDclient) {
			f = tag[i1]->area[i2]->frame[i3];
			blitz_strtorect(&rect, &f->rect, buf);
			resize_client(f->client, &f->rect, 0, False);
		}
		break;
    case FsFexpand:
		memcpy(expand, fcall->data, fcall->count);
		expand[fcall->count] = 0;
		draw_bar();
		break;
	case FsFdata:
		len = fcall->count;
		if(len >= sizeof(label[i1]->data))
			len = sizeof(label[i1]->data) - 1;
		memcpy(label[i1]->data, fcall->data, len);
		label[i1]->data[len] = 0;
		draw_bar();
		break;
	case FsFtag:
		memcpy(def.tag, fcall->data, fcall->count);
		def.tag[fcall->count] = 0;
		break;
	case FsFcolors:
		if((i1 >= nlabel) || (fcall->count != 23)
			|| (fcall->data[0] != '#') || (fcall->data[8] != '#')
		    || (fcall->data[16] != '#')
		  )
			return Ebadvalue;
		memcpy(label[i1]->colstr, fcall->data, fcall->count);
		label[i1]->colstr[fcall->count] = 0;
		blitz_loadcolor(dpy, screen, label[i1]->colstr, &label[i1]->color);
		draw_bar();
		break;
	case FsFselcolors:
		if((fcall->count != 23)
			|| (fcall->data[0] != '#') || (fcall->data[8] != '#')
		    || (fcall->data[16] != '#')
		  )
			return Ebadvalue;
		memcpy(def.selcolor, fcall->data, fcall->count);
		def.selcolor[fcall->count] = 0;
		blitz_loadcolor(dpy, screen, def.selcolor, &def.sel);
		draw_clients();
		break;
	case FsFnormcolors:
		if((fcall->count != 23)
			|| (fcall->data[0] != '#') || (fcall->data[8] != '#')
		    || (fcall->data[16] != '#')
		  )
			return Ebadvalue;
		memcpy(def.normcolor, fcall->data, fcall->count);
		def.normcolor[fcall->count] = 0;
		blitz_loadcolor(dpy, screen, def.normcolor, &def.norm);
		draw_clients();
		break;
	case FsFkeys:
		if(def.keyssz < fcall->offset + fcall->count + 1) {
			def.keyssz = fcall->offset + fcall->count + 1;
			tmp = cext_emallocz(def.keyssz);
			len = def.keys ? strlen(def.keys) : 0;
			if(len) {
				memcpy(tmp, def.keys, len);
				free(def.keys);
			}
			def.keys = tmp;
		}
		memcpy(def.keys + fcall->offset, fcall->data, fcall->count);
		def.keys[fcall->offset + fcall->count] = 0;
		break;
	case FsFrules:
		if(def.rulessz < fcall->offset + fcall->count + 1) {
			def.rulessz = fcall->offset + fcall->count + 1;
			tmp = cext_emallocz(def.rulessz);
			len = def.rules ? strlen(def.rules) : 0;
			if(len) {
				memcpy(tmp, def.rules, len);
				free(def.rules);
			}
			def.rules = tmp;
		}
		memcpy(def.rules + fcall->offset, fcall->data, fcall->count);
		def.rules[fcall->offset + fcall->count] = 0;
		break;
	case FsFfont:
		if(def.font)
			free(def.font);
		def.font = cext_emallocz(fcall->count + 1);
		memcpy(def.font, fcall->data, fcall->count);
		XFreeFont(dpy, xfont);
    	xfont = blitz_getfont(dpy, def.font);
		update_bar_geometry();
		break;
	case FsFmode:
		if(!i2)
			return Enofile;
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		if((i = str2mode(buf)) == -1)
			return Ebadvalue;
		tag[i1]->area[i2]->mode = i;
		arrange_area(tag[i1]->area[i2]);
		break;	
	default:
		return Enoperm;
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
	if(qpath_type(m->qid.path) == FsFkeys)
	   update_keys();	
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
	unsigned int i;
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
			else if(qpath_type(m->qid.path) == FsFevent) {
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
