/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdarg.h>
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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/socket.h>

#include "wm.h"

static char E9pversion[] = "9P version not supported";
static char Enoperm[] = "permission denied";
static char Enofile[] = "file not found";
static char Efidinuse[] = "fid in use";
static char Enomode[] = "mode not supported";
static char Enofunc[] = "function not supported";
static char Enocommand[] = "command not supported";
static char Ebadvalue[] = "bad value";

enum { WMII_IOUNIT = 2048 };

/*
 * filesystem specification
 * /			FsDroot
 * /def/		FsDdef
 * /def/border		FsFborder	0..n
 * /def/font		FsFfont		xlib font name
 * /def/selcolors	FsFselcolors	selected colors
 * /def/normcolors	FsFnormcolors	normal colors
 * /def/tagrules	FsFtagrules	tagging rules
 * /def/keys		FsFkeys		keys
 * /def/grabmod		FsFgrabmod	grab modifier
 * /def/colrules	FsFcolrules	column rules
 * /bar/		FsDbars
 * /bar/lab/		FsDbar
 * /bar/lab/data	FsFdata		<arbitrary data which gets displayed>
 * /bar/lab/colors	FsFcolors	<#RRGGBB> <#RRGGBB> <#RRGGBB>
 * /client/		FsDclients
 * /client/1/		FsDGclient	see /tag/X/X/X/X/ namespace below
 * /event		FsFevent
 * /ctl			FsFctl		command interface (root)
 * /tag			FsDtag
 * /tag/X/		FsDview
 * /tag/X/ctl		FsFctl		command interface (tag)
 * /tag/X/name		FsFname		current view name
 * /tag/X/index		FsFindex		current view name
 * /tag/X/sel/		FsDarea
 * /tag/X/1/		FsDarea
 * /tag/X/1/ctl		FsFctl		command interface (area)
 * /tag/X/1/mode	FsFmode		column mode
 * /tag/X/1/sel/	FsDclient
 * /tag/X/1/1/props	FsFprops	Class:instance of client
 * /tag/X/1/1/index	FsFindex	index of client in /client
 * /tag/X/1/1/name	FsFname		name of client
 * /tag/X/1/1/tags	FsFtags		tag of client
 * /tag/X/1/geom	FsFgeom		geometry of client
 * /tag/X/1/ctl 	FsFctl		command interface (client)
 */
const char *dirnames[] = {
};

/* IXP stuff */

PackedQid root_qid;

/*
 * Qid->path is calculated related to the index of the associated structure.
 * i1 is associated to tag, key, global client, or bar
 * i2 is associated to area
 * i3 is associated to client
 * ie /view/sel/ctl is i1id = sel tag id, i2id = sel area id , i3id = 0 (no id)
 */

static unsigned char
dir_of_qid(PackedQid wqid[IXP_MAX_WELEM], unsigned short qsel)
{
	return qsel ? wqid[qsel - 1].ptype : FsDroot;
}

static Bool
unpack_qpath(PackedQid wqid[IXP_MAX_WELEM], unsigned short qsel,
		unsigned char *type, void **i1, void **i2, void **i3)
{
	*i1 = *i2 = *i3 = nil;
	*type = wqid[qsel].ptype;

	if(wqid[qsel].i1id) {
		unsigned char dir_type = dir_of_qid(wqid, qsel);
		if((dir_type == FsDGclient) || (dir_type == FsDclients))
			*i1 = (void *)client_of_id(wqid[qsel].i1id);
		else {
			switch(*type) {
			case FsFdata:
			case FsFcolors:
			case FsDbar: *i1 = (void *)bar_of_id(wqid[qsel].i1id); break;
			default: *i1 = (void *)view_of_id(wqid[qsel].i1id); break;
			}
		}
		if(!*i1)
			return False;
		if(wqid[qsel].i2id) {
			if(!(*i2 = (void *)area_of_id(VIEW(*i1), wqid[qsel].i2id)))
				return False;
			if(wqid[qsel].i3id) 
				if(!(*i3 = (void *)frame_of_id(AREA(*i2), wqid[qsel].i3id)))
					return False;
		}
	}
	return True;
}

static char *
name_of_qid(PackedQid wqid[IXP_MAX_WELEM], unsigned short qsel)
{
	unsigned char dir_type, type;
	void *i1, *i2, *i3;
	static char buf[256];

	unpack_qpath(wqid, qsel, &type, &i1, &i2, &i3);
	dir_type = dir_of_qid(wqid, qsel);

	switch(type) {
	case FsDroot: return "/"; break;
	case FsDdef: return "def"; break;
		case FsDtag: return "tag"; break;
		case FsDclients: return "client"; break;
		case FsDbars: return "bar"; break;
		case FsDview:
			if(dir_type != FsDtag)
				return nil;
			if(VIEW(i1) == sel)
				return "sel";
			return VIEW(i1)->name;
			break;
		case FsDbar:
			if(!i1)
				return nil;
			return BAR(i1)->name;
			break;
		case FsDarea:
			if(!i1 || !i2)
				return nil;
			if(VIEW(i1)->sel == AREA(i2))
				return "sel";
			snprintf(buf, sizeof(buf), "%u", idx_of_area(i2));
			return buf;
			break;
		case FsDGclient:
			if(!i1)
				return nil;
			snprintf(buf, sizeof(buf), "%u", idx_of_client(CLIENT(i1)));
			return buf;
			break;
		case FsDclient:
			if(!i2 || !i3)
				return nil;
			if(AREA(i2)->sel == FRAME(i3))
				return "sel";
			snprintf(buf, sizeof(buf), "%u", idx_of_frame(i3));
			return buf;
			break;
		case FsFselcolors: return "selcolors"; break;
		case FsFnormcolors: return "normcolors"; break;
		case FsFfont: return "font"; break;
		case FsFcolrules: return "colrules"; break;
		case FsFgrabmod: return "grabmod"; break;
		case FsFtagrules: return "tagrules"; break;
		case FsFkeys: return "keys"; break;
		case FsFcolors: return "colors"; break;
		case FsFdata:
			if(!i1)
				return nil;
			return "data";
			break;
		case FsFctl: return "ctl"; break;
		case FsFborder: return "border"; break;
		case FsFgeom:
			if((dir_type == FsDclient) && (!i1 || !i2 || !i3))
				return nil;
			else if(!i1)
				return nil;
			return "geom";
			break;
		case FsFtags:
		if((dir_type == FsDclient) && (!i1 || !i2 || !i3))
			return nil;
		else if((dir_type == FsDGclient) && !i1)
			return nil;
		return "tags";
		break;
	case FsFprops:
	case FsFindex:
	case FsFname:
		if((dir_type == FsDclient) && (!i1 || !i2 || !i3))
			return nil;
		else if(!i1)
			return nil;
		switch(type) {
		case FsFname:
			return "name";
			break;
		case FsFprops:
			return "props";
			break;
		case FsFindex:
			return "index";
			break;
		default:
			break;
		}
		break;
	case FsFmode:
		if((dir_type == FsDarea) && (!i1 || !i2))
			return nil;
		return "mode";
		break;
	case FsFevent: return "event"; break;
	default: return nil; break;
	}
	return nil;
}

static unsigned char
type_of_name(PackedQid wqid[IXP_MAX_WELEM], unsigned short qsel, char *name)
{
	unsigned char dir_type;
	void *i1, *i2, *i3;
	unsigned int i;

	unpack_qpath(wqid, qsel, &dir_type, &i1, &i2, &i3);

	if(!name || !name[0] || !strncmp(name, "/", 2))
		return FsDroot;
	if(!strncmp(name, "tag", 4))
		return FsDtag;
	if(!strncmp(name, "tags", 5))
		return FsFtags;
	if(!strncmp(name, "client", 7))
		return FsDclients;
	if(!strncmp(name, "", 5))
		return FsDview;
	if(!strncmp(name, "bar", 4))
		return FsDbars;
	if(!strncmp(name, "def", 4))
		return FsDdef;
	if(!strncmp(name, "ctl", 4))
		return FsFctl;
	if(!strncmp(name, "event", 6))
		return FsFevent;
	if(!strncmp(name, "props", 6))
		return FsFprops;
	if(!strncmp(name, "index", 6))
		return FsFindex;
	if(!strncmp(name, "name", 5))
		return FsFname;
	if(!strncmp(name, "border", 7))
		return FsFborder;
	if(!strncmp(name, "geom", 5))
		return FsFgeom;
	if(!strncmp(name, "colors", 7))
		return FsFcolors;
	if(!strncmp(name, "selcolors", 10))
		return FsFselcolors;
	if(!strncmp(name, "normcolors", 11))
		return FsFnormcolors;
	if(!strncmp(name, "font", 5))
		return FsFfont;
	if(!strncmp(name, "colrules", 9))
		return FsFcolrules;
	if(!strncmp(name, "grabmod", 8))
		return FsFgrabmod;
	if(!strncmp(name, "keys", 5))
		return FsFkeys;
	if(!strncmp(name, "tagrules", 9))
		return FsFtagrules;
	if(!strncmp(name, "data", 5))
		return FsFdata;
	if(!strncmp(name, "mode", 5))
		return FsFmode;
	if((dir_type == FsDbars) && bar_of_name(name))
		return FsDbar;
	if((dir_type == FsDtag) && view_of_name(name))
		return FsDview;
	if(!strncmp(name, "sel", 4))
		goto dyndir;
	if(sscanf(name, "%d", &i) != 1)
		return FsLast;
dyndir:
	switch(dir_type) {
	case FsDbars: return FsDbar; break;
	case FsDtag: return FsDview; break;
	case FsDview: return FsDarea; break;
	case FsDclients: return FsDGclient; break;
	case FsDarea: return FsDclient; break;
	}
	return FsLast;
}

static PackedQid *
qid_of_name(PackedQid wqid[IXP_MAX_WELEM], unsigned short qsel, char *name)
{
	void *i1, *i2, *i3;
	int i;
	unsigned char dir_type, type;
	Client *c;
	static PackedQid new;
	memset(&new, 0, sizeof(PackedQid));

	unpack_qpath(wqid, qsel, &dir_type, &i1, &i2, &i3);
	type = type_of_name(wqid, qsel, name);

	switch(type) {
	case FsDroot:
		new = root_qid;
		break;
	case FsDdef:
	case FsDtag:
	case FsDclients:
	case FsDbars:
		if(dir_type != FsDroot)
			return nil;
		new.type=  IXP_QTDIR;
		new.ptype= type;
		break;
	case FsDview:
		if((dir_type != FsDtag) || !view)
			return nil;
		View *v = strncmp(name, "sel", 4) ? view_of_name(name) : sel;
		if(!v)
			return nil;
		new.type=  IXP_QTDIR;
		new.ptype= FsDview;
		new.i1id=  v->id;
		break;
	case FsDarea:
		if(!i1 || dir_type != FsDview)
			return nil;
		{
			View *p = VIEW(i1);
			new.type=  IXP_QTDIR;
			new.ptype= FsDarea;
			new.i1id=  p->id;
			if(!strncmp(name, "sel", 4)) {
				new.i2id=  p->sel->id;
			}
			else {
				Area *a;
				if(sscanf(name, "%d", &i) != 1)
					return nil;
				for(a=p->area; i && a; a=a->next, i--);
				if(!a)
					return nil;
				new.i2id=  a->id;
			}
		}
		break;
	case FsDclient:
		if(!i1 || !i2 || dir_type != FsDarea)
			return nil;
		{
			View *p = VIEW(i1);
			Area *a = AREA(i2);
			new.type=  IXP_QTDIR;
			new.ptype= FsDclient;
			new.i1id=  p->id;
			new.i2id=  a->id;
			if(!strncmp(name, "sel", 4)) {
				if(!a->frame)
					return nil;
				new.i3id=  a->sel->id;
			}
			else {
				Frame *f;
				if(sscanf(name, "%d", &i) != 1)
					return nil;
				for(f=a->frame; f && i; f=f->anext, i--);
				if(!f)
					return nil;
				new.i3id=  f->id;
			}
		}
		break;
	case FsDGclient:
		if(dir_type != FsDclients)
			return nil;
		if(sscanf(name, "%d", &i) != 1)
			return nil;
		for(c=client; i && c; c=c->next, i--);
		if(!c)
			return nil;
		new.type=  IXP_QTDIR;
		new.ptype= FsDGclient;
		new.i1id=  c->id;
		break;
	case FsDbar:
		if(dir_type !=  FsDbars)
			return nil;
		{
			Bar *l;
			if(!(l = bar_of_name(name)))
				return nil;
			new.type=  IXP_QTDIR;
			new.ptype= FsDbar;
			new.i1id=  l->id;
		}
		break;
	case FsFdata:
	case FsFcolors:
		if(!i1 || (dir_type != FsDbar))
			return nil;
		goto Mkfile;
		break;
	case FsFmode:
		if((dir_type == FsDarea) && (!i1 || !i2))
			return nil;
		goto Mkfile;
		break;
	case FsFgeom:
	case FsFindex:
		if(dir_type == FsDview)
			return nil;
	case FsFname:
	case FsFprops:
		if(dir_type == FsDroot)
			return nil;
	case FsFtags:
		if((dir_type == FsDclient) && ((!i1 || !i2 || !i3)))
			return nil;
		else if((dir_type == FsDGclient) && !i1)
			return nil;
		goto Mkfile;
		break;
	case FsFborder:
	case FsFgrabmod:
	case FsFfont:
	case FsFcolrules:
	case FsFtagrules:
	case FsFselcolors:
	case FsFnormcolors:
	case FsFkeys:
		if(dir_type != FsDdef)
			return nil;
	case FsFctl:
	case FsFevent:
Mkfile:
		new = wqid[qsel];
		new.type = IXP_QTFILE;
		new.ptype = type;
		break;
	default:
		return nil;
		break;
	}
	return &new;
}

static unsigned int
pack_stat(Stat *stat, PackedQid wqid[IXP_MAX_WELEM], unsigned short qsel,
		char *name, unsigned long long length, unsigned int mode)
{
	PackedQid *qid;
	stat->mode = mode;
	stat->atime = stat->mtime = time(0);
	cext_strlcpy(stat->uid, getenv("USER"), sizeof(stat->uid));
	cext_strlcpy(stat->gid, getenv("USER"), sizeof(stat->gid));
	cext_strlcpy(stat->muid, getenv("USER"), sizeof(stat->muid));

	cext_strlcpy(stat->name, name, sizeof(stat->name));
	stat->length = length;
	if((qid = qid_of_name(wqid, qsel ? qsel - 1 : 0, name)))
		stat->qid = qid->qid;

	return ixp_sizeof_stat(stat);
}

static unsigned int
stat_of_name(Stat *stat, char *name, PackedQid wqid[IXP_MAX_WELEM], unsigned short qsel)
{
	unsigned char dir_type, type;
	void *i1, *i2, *i3;
	char buf[256];
	XRectangle fr;
	Frame *f;

	if(!unpack_qpath(wqid, qsel, &dir_type, &i1, &i2, &i3))
		return 0;
	type = type_of_name(wqid, qsel, name);

	switch (type) {
	case FsDclient:
	case FsDGclient:
	case FsDarea:
	case FsDview:
	case FsDdef:
	case FsDclients:
	case FsDbar:
	case FsDroot:
	case FsDtag:
		return pack_stat(stat, wqid, qsel, name, 0, IXP_DMDIR | IXP_DMREAD | IXP_DMEXEC);
		break;
	case FsDbars:
		return pack_stat(stat, wqid, qsel, name, 0, IXP_DMDIR | IXP_DMREAD | IXP_DMWRITE | IXP_DMEXEC);
		break;
	case FsFctl:
		return pack_stat(stat, wqid, qsel, name, 0, IXP_DMWRITE);
		break;
	case FsFevent:
		return pack_stat(stat, wqid, qsel, name, 0, IXP_DMREAD | IXP_DMWRITE);
		break;
	case FsFborder:
		snprintf(buf, sizeof(buf), "%d", def.border);
		return pack_stat(stat, wqid, qsel, name, strlen(buf), IXP_DMREAD | IXP_DMWRITE);
		break;
	case FsFgeom:
		if(dir_type == FsDclient)
			fr = FRAME(i3)->rect;
		else if(CLIENT(i1)->frame)
			fr = CLIENT(i1)->sel->rect;
		else
			fr = CLIENT(i1)->rect;
		snprintf(buf, sizeof(buf), "%d %d %d %d", fr.x, fr.y, fr.width, fr.height);
		return pack_stat(stat, wqid, qsel, name, strlen(buf), IXP_DMREAD | IXP_DMWRITE);
		break;
	case FsFprops:
		if(dir_type == FsDclient) {
			f = FRAME(i3);
			return pack_stat(stat, wqid, qsel, name, strlen(f->client->props), IXP_DMREAD);
		}
		else
			return pack_stat(stat, wqid, qsel, name, strlen(CLIENT(i1)->props), IXP_DMREAD);
		break;
	case FsFindex:
		switch(dir_type) {
		case FsDclient:
			f = FRAME(i3);
			snprintf(buf, sizeof(buf), "%d", idx_of_client(f->client));
			break;
		case FsDarea:
			snprintf(buf, sizeof(buf), "%d", idx_of_area(i2));
			break;
		default:
			snprintf(buf, sizeof(buf), "%d", idx_of_client(i1));
			break;
		}
		return pack_stat(stat, wqid, qsel, name, strlen(buf), IXP_DMREAD);
		break;
	case FsFname:
		if(dir_type == FsDclient) {
			f = FRAME(i3);
			return pack_stat(stat, wqid, qsel, name, strlen(f->client->name), IXP_DMREAD);
		}
		else if(dir_type == FsDview)
			return pack_stat(stat, wqid, qsel, name,
					view ? strlen(VIEW(i1)->name) : 0, IXP_DMREAD);
		else
			return pack_stat(stat, wqid, qsel, name, strlen(CLIENT(i1)->name), IXP_DMREAD);
		break;
	case FsFtags:
		switch(dir_type) {
		case FsDclient:
			f = FRAME(i3);
			return pack_stat(stat, wqid, qsel, name, strlen(f->client->tags), IXP_DMREAD | IXP_DMWRITE);
			break;
		case FsDGclient:
			return pack_stat(stat, wqid, qsel,
					name, strlen(CLIENT(i1)->tags), IXP_DMREAD | IXP_DMWRITE);
			break;
		default:
			break;
		}
		break;
	case FsFdata:
		return pack_stat(stat, wqid, qsel, name,
				!BAR(i1)->next ? 0 : strlen(BAR(i1)->data), IXP_DMREAD | IXP_DMWRITE);
		break;
	case FsFmode:
		return pack_stat(stat, wqid, qsel, name,
					strlen(str_of_column_mode(AREA(i2)->mode)),
					IXP_DMREAD | IXP_DMWRITE);
		break;
	case FsFcolors:
	case FsFselcolors:
	case FsFnormcolors:
		return pack_stat(stat, wqid, qsel, name, 23, IXP_DMREAD | IXP_DMWRITE);
		break;
	case FsFkeys:
		return pack_stat(stat, wqid, qsel, name, def.keys ? strlen(def.keys) : 0, IXP_DMREAD | IXP_DMWRITE);
		break;
	case FsFtagrules:
		return pack_stat(stat, wqid, qsel, name, def.tagrules ? strlen(def.tagrules) : 0, IXP_DMREAD | IXP_DMWRITE);
		break;
	case FsFcolrules:
		return pack_stat(stat, wqid, qsel, name, def.colrules ? strlen(def.colrules) : 0, IXP_DMREAD | IXP_DMWRITE);
	case FsFfont:
		return pack_stat(stat, wqid, qsel, name, strlen(def.font), IXP_DMREAD | IXP_DMWRITE);
	case FsFgrabmod:
		return pack_stat(stat, wqid, qsel, name, strlen(def.grabmod), IXP_DMREAD | IXP_DMWRITE);
		break;
	}
	return 0;
}

static unsigned int
stat_of_names(unsigned char **p, int *iounit, PackedQid wqid[IXP_MAX_WELEM], unsigned short qsel, ...)
{
	va_list ap;
	char *str;
	unsigned int n = 0;
	Stat stat;

	va_start(ap, qsel);

	while((str = va_arg(ap, char *))) {
			n += stat_of_name(&stat, str, wqid, qsel);
			ixp_pack_stat(p, iounit, &stat);
	}

	va_end(ap);
	return n;
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
	new->next=c->map;
	c->map=new;
	new->wqid[0] = root_qid.qid;
	new->nwqid = 1;
	new->fid = fcall->fid;
	fcall->id = RATTACH;
	fcall->qid = new->wqid[new->sel];
	ixp_server_respond_fcall(c, fcall);
	return nil;
}

static char *
xwalk(IXPConn *c, Fcall *fcall)
{
	IXPMap *m;
	unsigned int qsel, nwqid;
	PackedQid wqid[IXP_MAX_WELEM], *qid;

	if(!(m = ixp_server_fid2map(c, fcall->fid)))
		return Enofile;
	if(fcall->fid != fcall->newfid && (ixp_server_fid2map(c, fcall->newfid)))
		return Efidinuse;

	for(qsel = 0; qsel < m->nwqid; qsel++)
		wqid[qsel].qid = m->wqid[qsel];
	if(qsel)
		qsel--;
	for(nwqid = 0; nwqid < fcall->nwname; nwqid++) {
		if(qsel >= IXP_MAX_WELEM)
			break;
		if(!strncmp(fcall->wname[nwqid], "..", 3)) {
			if(qsel)
				qsel--;
			qid = &wqid[qsel];
		}
		else {
			qid = qid_of_name(wqid, qsel, fcall->wname[nwqid]);
			if(!qid)
				break;
			qsel++;
		}
		fcall->wqid[nwqid] = wqid[qsel].qid = qid->qid;
	}

	if(fcall->nwname && !nwqid)
		return Enofile;

	/* a fid will only be valid, if the walk was complete */
	if(nwqid == fcall->nwname) {
		unsigned int i;
		if(fcall->fid != fcall->newfid) {
			m = cext_emallocz(sizeof(IXPMap));
			m->next=c->map;
			c->map=m;
		}
		for(i = 0; i <= qsel; i++)
			m->wqid[i] = wqid[i].qid;
		m->nwqid = qsel + 1;
		m->sel = qsel;
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
	PackedQid *qid = (PackedQid *)&m->wqid[m->sel];
	unsigned char type;

	if(!(fcall->mode | IXP_OWRITE))
		return Enomode;
	if(!m)
		return Enofile;
	if(!strncmp(fcall->name, ".", 2) || !strncmp(fcall->name, "..", 3))
		return "illegal file name";

	type = qid->ptype;
	switch(type) {
	case FsDbars:
		create_bar(fcall->name);
		break;
	default:
		return Enofile;
		break;
	}

	if(!(qid = qid_of_name((PackedQid *)&m->wqid, m->sel, fcall->name)))
		return Enofile;

	m->wqid[m->nwqid++] = fcall->qid = qid->qid;
	m->sel++;
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
		return Enofile;
	if(!(fcall->mode | IXP_OREAD) && !(fcall->mode | IXP_OWRITE))
		return Enomode;
	fcall->id = ROPEN;
	fcall->qid = m->wqid[m->sel];
	fcall->iounit = WMII_IOUNIT;
	ixp_server_respond_fcall(c, fcall);

	return nil;
}

static char *
xremove(IXPConn *c, Fcall *fcall)
{
	IXPMap *t, *m = ixp_server_fid2map(c, fcall->fid);
	unsigned char type;
	void *i1, *i2, *i3;

	if(!m)
		return Enofile;
	if(!unpack_qpath((PackedQid *)&m->wqid, m->sel, &type, &i1, &i2, &i3))
		return Enofile;
	if(type != FsDbar)
		return Enoperm;
	/* clunk */
	if(c->map == m)
		c->map = m->next;
	else {
		for(t=c->map; t && t->next != m; t=t->next);
		if(t)
			t->next = m->next;
	}
	free(m);
	switch(type) {
	case FsDbar:
		{
			Bar *b = BAR(i1);
			destroy_bar(b);
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
	PackedQid *pwqid = (PackedQid *)&m->wqid;
	View *v;
	Frame *f;
	Area *a;
	Client *cl;
	Bar *b;
	void *i1, *i2, *i3;
	unsigned int i, len;
	int iounit;
	unsigned char dir_type, type, *p = fcall->data;
	char buf[256];
	XRectangle fr;

	if(!m)
		return Enofile;
	if(!unpack_qpath(pwqid, m->sel, &type, &i1, &i2, &i3))
		return Enofile;
	dir_type = dir_of_qid(pwqid, m->sel);

	fcall->count = 0;
	iounit = fcall->iounit;
	if(fcall->offset) {
		switch (type) {
		case FsDtag:
			/* jump to offset */
			len = 0;
			if(view)
				len += stat_of_name(&stat, "sel", pwqid, m->sel);
			for(v=view; v; v=v->next) {
				len += stat_of_name(&stat, v->name, pwqid, m->sel);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; v; v=v->next) {
				len = stat_of_name(&stat, v->name, pwqid, m->sel);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				ixp_pack_stat(&p, &iounit, &stat);
			}
			break;
		case FsDclients:
			/* jump to offset */
			len = 0;
			for(cl=client, i=0; cl; cl=cl->next, i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len += stat_of_name(&stat, buf, pwqid, m->sel);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; cl; cl=cl->next, i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len = stat_of_name(&stat, buf, pwqid, m->sel);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				ixp_pack_stat(&p, &iounit, &stat);
			}
			break;
		case FsDbars:
			/* jump to offset */
			len = 0;
			for(b=bar; b; b=b->next) {
				len += stat_of_name(&stat, b->name, pwqid, m->sel);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; b; b=b->next) {
				len = stat_of_name(&stat, b->name, pwqid, m->sel);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				ixp_pack_stat(&p, &iounit, &stat);
			}
			break;
		case FsDview:
			/* jump to offset */
			len = 0;
			if(view) {
				len = stat_of_name(&stat, "name", pwqid, m->sel);
				len += stat_of_name(&stat, "ctl", pwqid, m->sel);
				if(VIEW(i1)->area)
					len += stat_of_name(&stat, "sel", pwqid, m->sel);
				for(a=VIEW(i1)->area, i=0; a; a=a->next, i++) {
					snprintf(buf, sizeof(buf), "%u", i);
					len += stat_of_name(&stat, buf, pwqid, m->sel);
					if(len <= fcall->offset)
						continue;
					break;
				}
				/* offset found, proceeding */
				for(; a; a=a->next, i++) {
					snprintf(buf, sizeof(buf), "%u", i);
					len = stat_of_name(&stat, buf, pwqid, m->sel);
					if(fcall->count + len > fcall->iounit)
						break;
					fcall->count += len;
					ixp_pack_stat(&p, &iounit, &stat);
				}
			}
			break;
		case FsDarea:
			/* jump to offset */
			len = stat_of_name(&stat, "ctl", pwqid, m->sel);
			len += stat_of_name(&stat, "index", pwqid, m->sel);
			if(i2)
				len += stat_of_name(&stat, "mode", pwqid, m->sel);
			if(AREA(i2)->frame)
				len += stat_of_name(&stat, "sel", pwqid, m->sel);
			for(f=AREA(i2)->frame, i=0; f; f=f->anext, i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len += stat_of_name(&stat, buf, pwqid, m->sel);
				if(len <= fcall->offset)
					continue;
				break;
			}
			/* offset found, proceeding */
			for(; f; f=f->anext, i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len = stat_of_name(&stat, buf, pwqid, m->sel);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				ixp_pack_stat(&p, &iounit, &stat);
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
		case FsFtagrules:
			len = def.tagrules ? strlen(def.tagrules) : 0;
			if(len <= fcall->offset) {
				fcall->count = 0;
				break;
			}
			fcall->count = len - fcall->offset;
			if(fcall->count > fcall->iounit) {
				memcpy(p, def.tagrules + fcall->offset, fcall->iounit);
				fcall->count = fcall->iounit;
			}
			else if(fcall->count)
				memcpy(p, def.tagrules + fcall->offset, fcall->count);
			break;
		case FsFcolrules:
			len = def.colrules ? strlen(def.colrules) : 0;
			if(len <= fcall->offset) {
				fcall->count = 0;
				break;
			}
			fcall->count = len - fcall->offset;
			if(fcall->count > fcall->iounit) {
				memcpy(p, def.colrules + fcall->offset, fcall->iounit);
				fcall->count = fcall->iounit;
			}
			else if(fcall->count)
				memcpy(p, def.colrules + fcall->offset, fcall->count);
			break;
		default:
			break;
		}
	}
	else {
		switch (type) {
		case FsDroot:
			fcall->count = stat_of_names(&p, &iounit, pwqid, m->sel, "ctl", "event", "def", "bar", nil);
			if(view) {
				fcall->count += stat_of_names(&p, &iounit, pwqid, m->sel, "tag", nil);
			}
			if(client) {
				fcall->count += stat_of_names(&p, &iounit, pwqid, m->sel, "client", nil);
			}
			break;
		case FsDtag:
			if(view) {
				fcall->count += stat_of_names(&p, &iounit, pwqid, m->sel, "sel", nil);
			}
			for(v=view; v; v=v->next) {
				len = stat_of_name(&stat, v->name, pwqid, m->sel);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				ixp_pack_stat(&p, &iounit, &stat);
			}
			break;
		case FsDclients:
			for(cl=client, i=0; cl; cl=cl->next, i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len = stat_of_name(&stat, buf, pwqid, m->sel);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				ixp_pack_stat(&p, &iounit, &stat);
			}
			break;
		case FsDbars:
			for(b=bar; b; b=b->next) {
				len = stat_of_name(&stat, b->name, pwqid, m->sel);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				ixp_pack_stat(&p, &iounit, &stat);
			}
			break;
		case FsDbar:
			if(!i1)
				return Enofile;
			fcall->count = stat_of_names(&p, &iounit, pwqid, m->sel, "colors", "data", nil);
			break;
		case FsDdef:
			fcall->count = stat_of_names(&p, &iounit, pwqid, m->sel, "border", "selcolors",
					"normcolors", "font", "keys", "tagrules", "grabmod", "colrules", nil);
			break;
		case FsDview:
			if(view) {
				fcall->count = stat_of_names(&p, &iounit, pwqid, m->sel, "name", "ctl", nil);
				if(VIEW(i1)->area) {
					fcall->count += stat_of_names(&p, &iounit, pwqid, m->sel, "sel", nil);
				}
				for(a=VIEW(i1)->area, i=0; a; a=a->next, i++) {
					snprintf(buf, sizeof(buf), "%u", i);
					len = stat_of_name(&stat, buf, pwqid, m->sel);
					if(fcall->count + len > fcall->iounit)
						break;
					fcall->count += len;
					ixp_pack_stat(&p, &iounit, &stat);
				}
			}
			break;
		case FsDarea:
			fcall->count = stat_of_names(&p, &iounit, pwqid, m->sel, "ctl", "index", nil);
			if(i2) {
				fcall->count += stat_of_names(&p, &iounit, pwqid, m->sel, "mode", nil);
			}
			if(AREA(i2)->frame) {
				fcall->count += stat_of_names(&p, &iounit, pwqid, m->sel, "sel", nil);
			}
			for(f=AREA(i2)->frame, i=0; f; f=f->anext, i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				len = stat_of_name(&stat, buf, pwqid, m->sel);
				if(fcall->count + len > fcall->iounit)
					break;
				fcall->count += len;
				ixp_pack_stat(&p, &iounit, &stat);
			}
			break;
		case FsDGclient:
		case FsDclient:
			fcall->count = stat_of_names(&p, &iounit, pwqid, m->sel, "props", "name",
					"index", "tags", "geom", "ctl", nil);
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
			if(dir_type == FsDclient)
				fr = FRAME(i3)->rect;
			else if(CLIENT(i1)->frame)
				fr = CLIENT(i1)->sel->rect;
			else
				fr = CLIENT(i1)->rect;
			snprintf(buf, sizeof(buf), "%d %d %d %d", fr.x, fr.y, fr.width, fr.height);
			fcall->count = strlen(buf);
			memcpy(p, buf, fcall->count);
			break;
		case FsFprops:
			if(dir_type == FsDclient) {
				if((fcall->count = strlen(FRAME(i3)->client->props)))
					memcpy(p, FRAME(i3)->client->props, fcall->count);
			}
			else {
				if((fcall->count = strlen(CLIENT(i1)->props)))
					memcpy(p, CLIENT(i1)->props, fcall->count);
			}
			break;
		case FsFindex:
			switch(dir_type) {
			case FsDclient:
				snprintf(buf, sizeof(buf), "%d",
						idx_of_client(FRAME(i3)->client));
				break;
			case FsDarea:
				snprintf(buf, sizeof(buf), "%d", idx_of_area(i2));
				break;
			default:
				snprintf(buf, sizeof(buf), "%d", idx_of_client(i1));
				break;
			}
			if((fcall->count = strlen(buf)))
				memcpy(p, buf, fcall->count);
			break;
		case FsFname:
			if(dir_type == FsDclient) {
				if((fcall->count = strlen(FRAME(i3)->client->name)))
					memcpy(p, FRAME(i3)->client->name, fcall->count);
			}
			else if(dir_type == FsDview) {
				if((fcall->count = strlen(VIEW(i1)->name)))
					memcpy(p, VIEW(i1)->name, fcall->count);
			}
			else {
				if((fcall->count = strlen(CLIENT(i1)->name)))
					memcpy(p, CLIENT(i1)->name, fcall->count);
			}
			break;
		case FsFtags:
			switch(dir_type) {
			case FsDclient:
				{
					Client *c = FRAME(i3)->client;
					if((fcall->count = strlen(c->tags)))
						memcpy(p, c->tags, fcall->count);
				}
				break;
			case FsDGclient:
				if((fcall->count = strlen(CLIENT(i1)->tags)))
					memcpy(p, CLIENT(i1)->tags, fcall->count);
				break;
			default:
				break;
			}
			break;
		case FsFdata:
			if(!i1)
				return Enofile;
			if((fcall->count = strlen(BAR(i1)->data)))
				memcpy(p, BAR(i1)->data, fcall->count);
			break;
		case FsFcolors:
			if(!i1)
				return Enofile;
			if((fcall->count = strlen(BAR(i1)->colstr)))
				memcpy(p, BAR(i1)->colstr, fcall->count);
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
		case FsFtagrules:
			fcall->count = def.tagrules ? strlen(def.tagrules) : 0;
			if(fcall->count > fcall->iounit) {
				memcpy(p, def.tagrules, fcall->iounit);
				fcall->count = fcall->iounit;
			}
			else if(fcall->count)
				memcpy(p, def.tagrules, fcall->count);
			break;
		case FsFgrabmod:
			if((fcall->count = strlen(def.grabmod)))
				memcpy(p, def.grabmod, fcall->count);
			break;
		case FsFcolrules:
			fcall->count = def.colrules ? strlen(def.colrules) : 0;
			if(fcall->count > fcall->iounit) {
				memcpy(p, def.colrules, fcall->iounit);
				fcall->count = fcall->iounit;
			}
			else if(fcall->count)
				memcpy(p, def.colrules, fcall->count);
			break;
		case FsFfont:
			if((fcall->count = strlen(def.font)))
				memcpy(p, def.font, fcall->count);
			break;
		case FsFmode:
			snprintf(buf, sizeof(buf), "%s",
					str_of_column_mode(AREA(i2)->mode));
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
		return Enofile;
	if(!(name = name_of_qid((PackedQid *)&m->wqid, m->sel)))
		return Enofile;
	if(!stat_of_name(&fcall->stat, name, (PackedQid *)&m->wqid, m->sel ? m->sel - 1 : 0))
		return Enofile;
	fcall->id = RSTAT;
	ixp_server_respond_fcall(c, fcall);
	return nil;
}

static char *
xwrite(IXPConn *c, Fcall *fcall)
{
	char buf[256], *tmp;
	IXPMap *m = ixp_server_fid2map(c, fcall->fid);
	void *i1, *i2, *i3;
	int i;
	unsigned char dir_type, type;
	unsigned int len;
	Frame *f;
	Client *cl;

	if(!m)
		return Enofile;
	if(!unpack_qpath((PackedQid *)&m->wqid, m->sel, &type, &i1, &i2, &i3))
		return Enofile;
	dir_type = dir_of_qid((PackedQid *)&m->wqid, m->sel);

	switch(type) {
	case FsFctl:
		if(fcall->count >= sizeof(buf))
			return Enocommand;
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		switch(dir_type) {
		case FsDroot:
			if(!strncmp(buf, "quit", 5))
				srv.running = 0;
			else if(!strncmp(buf, "view ", 5))
				select_view(&buf[5]);
			else
				return Enocommand;
			break;
		case FsDview:
			if(!strncmp(buf, "select ", 7)) {
				if(view)
					select_area(VIEW(i1)->sel, &buf[7]);
			}
			else
				return Enocommand;
			break;
		case FsDarea:
			if(!strncmp(buf, "select ", 7)) {
				Area *a = AREA(i2);
				if(a->frame)
					select_client(a->sel->client, &buf[7]);
			}
			else
				return Enocommand;
			break;
		case FsDclient:
			f = FRAME(i3);
			if(!strncmp(buf, "kill", 5))
				kill_client(f->client);
			else if(!strncmp(buf, "newcol ", 7))
				newcol_client(f->client, &buf[7]);
			else if(!strncmp(buf, "send ", 5))
				send_client(f->client, &buf[5]);
			else if(!strncmp(buf, "move ", 5))
				move_client(f->client, &buf[5]);
			else if(!strncmp(buf, "size ", 5))
				size_client(f->client, &buf[5]);
			else
				return Enocommand;
			break;
		case FsDGclient:
			if(!strncmp(buf, "kill", 5))
				kill_client(CLIENT(i1));
			else
				return Enocommand;
			break;
		default:
			break;
		}
		break;
	case FsFborder:
		if(fcall->count >= sizeof(buf))
			return Ebadvalue;
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		if(sscanf(buf, "%d", &i) != 1)
			return Ebadvalue;
		if(i < 0 || i > 100)
			return Ebadvalue;
		def.border = i;
		resize_all_clients();
		break;
	case FsFtags:
		if(!fcall->count || (fcall->count >= sizeof(buf)))
			return Ebadvalue;
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		cext_trim(buf, " \t/");
		if(dir_type == FsDclient)
			cl = FRAME(i3)->client;
		else
			cl = CLIENT(i1);
		apply_tags(cl, buf);
		update_views();
		draw_client(cl);
		break;
	case FsFdata:
		len = fcall->count;
		if(len >= sizeof(BAR(i1)->data))
			len = sizeof(BAR(i1)->data) - 1;
		memcpy(BAR(i1)->data, fcall->data, len);
		BAR(i1)->data[len] = 0;
		draw_bar();
		break;
	case FsFcolors:
		if(!i1 || (fcall->count != 23) || (fcall->data[0] != '#')
				|| (fcall->data[8] != '#') || (fcall->data[16] != '#'))
			return Ebadvalue;
		memcpy(BAR(i1)->colstr, fcall->data, fcall->count);
		BAR(i1)->colstr[fcall->count] = 0;
		blitz_loadcolor(&BAR(i1)->color, BAR(i1)->colstr);
		draw_bar();
		break;
	case FsFselcolors:
		if((fcall->count != 23) || (fcall->data[0] != '#')
				|| (fcall->data[8] != '#') || (fcall->data[16] != '#'))
			return Ebadvalue;
		memcpy(def.selcolor, fcall->data, fcall->count);
		def.selcolor[fcall->count] = 0;
		blitz_loadcolor(&def.sel, def.selcolor);
		draw_clients();
		break;
	case FsFnormcolors:
		if((fcall->count != 23) || (fcall->data[0] != '#')
				|| (fcall->data[8] != '#') || (fcall->data[16] != '#'))
			return Ebadvalue;
		memcpy(def.normcolor, fcall->data, fcall->count);
		def.normcolor[fcall->count] = 0;
		blitz_loadcolor(&def.norm, def.normcolor);
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
	case FsFtagrules:
		if(def.tagrulessz < fcall->offset + fcall->count + 1) {
			def.tagrulessz = fcall->offset + fcall->count + 1;
			tmp = cext_emallocz(def.tagrulessz);
			len = def.tagrules ? strlen(def.tagrules) : 0;
			if(len) {
				memcpy(tmp, def.tagrules, len);
				free(def.tagrules);
			}
			def.tagrules = tmp;
		}
		memcpy(def.tagrules + fcall->offset, fcall->data, fcall->count);
		def.tagrules[fcall->offset + fcall->count] = 0;
		break;
	case FsFcolrules:
		if(def.colrulessz < fcall->offset + fcall->count + 1) {
			def.colrulessz = fcall->offset + fcall->count + 1;
			tmp = cext_emallocz(def.colrulessz);
			len = def.colrules ? strlen(def.colrules) : 0;
			if(len) {
				memcpy(tmp, def.colrules, len);
				free(def.colrules);
			}
			def.colrules = tmp;
		}
		memcpy(def.colrules + fcall->offset, fcall->data, fcall->count);
		def.colrules[fcall->offset + fcall->count] = 0;
		break;
	case FsFgeom:
		if(fcall->count >= sizeof(buf))
			return Ebadvalue;
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		if(dir_type == FsDclient) {
			XRectangle new;
			f = FRAME(i3);
			new = f->rect;
			strtorect(&new, buf);
			if(new.width == 0)
				new.width = rect.width;
			if(new.height == 0)
				new.height = rect.height;
			if(i2)
				resize_column(f->client, &new, nil);
			else
				resize_client(f->client, &new, False);
		}
		break;
	case FsFgrabmod:
		{
			unsigned long mod;
			if(fcall->count >= sizeof(buf))
				return Ebadvalue;
			memcpy(buf, fcall->data, fcall->count);
			buf[fcall->count] = 0;
			mod = mod_key_of_str(buf);
			if((mod != Mod1Mask) && (mod != Mod2Mask) && (mod != Mod3Mask)
					&& (mod != Mod4Mask) && (mod != Mod5Mask))
				return Ebadvalue;
			cext_strlcpy(def.grabmod, buf, sizeof(def.grabmod));
			def.mod = mod;
			if(view)
				restack_view(sel);
		}
		break;
	case FsFfont:
		if(def.font)
			free(def.font);
		def.font = cext_emallocz(fcall->count + 1);
		memcpy(def.font, fcall->data, fcall->count);
		blitz_loadfont(&blitzfont, def.font);
		resize_bar();
		break;
	case FsFmode:
		if(fcall->count >= sizeof(buf))
			return Ebadvalue;
		if(dir_type == FsDarea && AREA(i2) == AREA(i2)->view->area)
			return Enofile;
		memcpy(buf, fcall->data, fcall->count);
		buf[fcall->count] = 0;
		if((i = column_mode_of_str(buf)) == -1)
			return Ebadvalue;
		AREA(i2)->mode = i;
		arrange_column(AREA(i2), True);
		restack_view(VIEW(i1));
		draw_clients();
		break;
	case FsFevent:
		if(fcall->count >= sizeof(buf))
			return Ebadvalue;
		if(fcall->count) {
			memcpy(buf, fcall->data, fcall->count);
			buf[fcall->count] = 0;
			write_event(buf);
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

static char *
xclunk(IXPConn *c, Fcall *fcall)
{
	IXPMap *t, *m = ixp_server_fid2map(c, fcall->fid);
	Client *cl;
	unsigned char type;

	if(!m)
		return Enofile;
	type = ((PackedQid *)&m->wqid[m->sel])->ptype;
	if(type == FsFkeys)
		update_keys();
	else if(type == FsFtagrules) {
		update_rules(&trule, def.tagrules);
		for(cl=client; cl; cl=cl->next)
			apply_rules(cl);
		update_views();
	}
	else if(type == FsFcolrules) {
		update_rules(&vrule, def.colrules);
	}
	if(c->map == m)
		c->map = m->next;
	else {
		for(t=c->map; t && t->next != m; t=t->next);
		if(t)
			t->next = m->next;
	}
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
	IXPConn *c;

	for(c=srv.conn; c; c=c->next) {
		if(c->is_pending) {
			/* pending reads on /event only, no qid checking */
			IXPMap *m = ixp_server_fid2map(c, c->pending.fid);
			if(!m) {
				if(ixp_server_respond_error(c, &c->pending, Enofile))
					return;
			}
			else if(((PackedQid *)&m->wqid[m->sel])->ptype == FsFevent) {
				/* pending reads on /event only, no qid checking */
				c->pending.count = strlen(event);
				memcpy(c->pending.data, event, c->pending.count);
				c->pending.id = RREAD;
				if(ixp_server_respond_fcall(c, &c->pending))
					return;
			}
		}
	}
}

void
new_ixp_conn(IXPConn *c)
{
	int fd = accept(c->fd, nil, nil);

	if(fd >= 0)
		ixp_server_open_conn(c->srv, fd, do_fcall, ixp_server_close_conn);
}
