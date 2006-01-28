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

#include "../libixp2/ixp.h"
#include "blitz.h"

/*
 * filesystem specification
 * / 					Droot
 * /display				Fdisplay	'north', 'south', 'none'
 * /font				Ffont		<xlib font name>
 * /new					Fnew 		returns id of new item
 * /event				Fevent
 * /default/ 			Ditem
 * /default/color		Fcolor		<#RRGGBB> <#RRGGBB> <#RRGGBB>
 * /1/					Ditem
 * /1/data 				Fdata		<arbitrary data which gets displayed>
 * /1/color				Fcolor		<#RRGGBB> <#RRGGBB> <#RRGGBB>
 */

/* 8-bit qid.path.type */
enum {                          
    Droot,
    Ditem,
    Fdisplay,
    Ffont,
    Fevent,
    Fdata,                      /* data to display */
    Fcolor
};

typedef struct Map Map;
struct Map {
	unsigned int fid;
	Qid qid; 
	Map *next;
};  

typedef struct {
    char data[256];
	char color[24];
    unsigned long bg;
    unsigned long fg;
    unsigned long border;
} Item;

static size_t nitem = 0;
static size_t itemsz = 0;
static Item **item = 0;
static char *sockfile = nil;
static pid_t mypid = 0;
static IXPServer srv = { 0 };
static Qid root_qid;
static Display *dpy;
static int screen_num;
static char *font = nil;
static Align align = SOUTH;
/*
static XFontStruct *xfont;
static GC gc;
static Window win;
static XRectangle geom;
static Pixmap pmap;

static Draw zero_draw = { 0 };
*/

static char *version[] = {
    "wmiibar - window manager improved bar - " VERSION "\n"
        "  (C)opyright MMIV-MMVI Anselm R. Garbe\n", 0
};

static void
usage()
{
    fprintf(stderr, "%s %d",
            "usage: wmiibar -a <server address> [-v]\n"
            "      -a    server address \n"
            "      -v    version info\n", 0);
    exit(1);
}

static int
dummy_error_handler(Display * dpy, XErrorEvent * err)
{
    return 0;
}

static void
exit_cleanup()
{
    if(mypid == getpid())
        unlink(sockfile);
}

static void
new_item()
{
	size_t i;
	if(!item) {
		itemsz = 2;
		item = cext_emallocz(sizeof(Item *) * itemsz);
	}
	if(nitem + 1 >= itemsz) {
		Item **tmp = item;
		itemsz *= 2;
		item = cext_emallocz(sizeof(Item *) * itemsz);
		for(i = 0; tmp[i]; i++)
			item[i] = tmp[i];
		free(tmp);
	}
	item[nitem++] = cext_emallocz(sizeof(Item));
}

void
detach_item(Item *it)
{
	size_t i;
	for(i = 0; item[i] != it; i++);
	for(; item[i + 1]; i++)
		item[i] = item[i + 1];
	item[i] = nil;
	nitem--;
}

static unsigned long long
mkqpath(unsigned char type, unsigned short item)
{
    return ((unsigned long long) item << 8) | (unsigned long long) type;
}

static unsigned char
qpath_type(unsigned long long path)
{
    return path & 0xff;
}

static unsigned short
qpath_item(unsigned long long path)
{
    return (path >> 8) & 0xffff;
}

static char *
qid_to_name(Qid *qid)
{
	unsigned char type = qpath_type(qid->path);
	unsigned short i = qpath_item(qid->path);
	static char buf[32];

	switch(type) {
		case Droot: return "/"; break;
		case Ditem:
			if(!i) 
				return "default";
			else if(i == nitem)
				return "new";
			snprintf(buf, sizeof(buf), "%u", i);
			return buf;
			break;
		case Fdisplay: return "display"; break;
		case Ffont: return "font"; break;
		case Fdata: return "data"; break;
		case Fevent: return "event"; break;
		case Fcolor: return "color"; break;
		default: return nil; break;
	}
}

static int
name_to_type(char *name)
{
    const char *errstr;
    unsigned int i;
	if(!name || !name[0] || !strncmp(name, "..", 3))
		return Droot;
	if(!strncmp(name, "default", 8) || !strncmp(name, "new", 4))
		return Ditem;
	if(!strncmp(name, "display", 8))
		return Fdisplay;
	if(!strncmp(name, "font", 5))
		return Ffont;
	if(!strncmp(name, "data", 5))
		return Fdata;
	if(!strncmp(name, "event", 6))
		return Fevent;
	if(!strncmp(name, "color", 6))
		return Fcolor;
   	i = (unsigned short) cext_strtonum(name, 1, 0xffff, &errstr);
    if(!errstr && i <= nitem)
		return Ditem;
	return -1;
}

static Map *
fid_to_map(Map *maps, unsigned int fid)
{
	Map *m;
	for(m = maps; m && (m->fid != fid); m = m->next);
	return m;
}

static int
mkqid(Qid *dir, char *wname, Qid *new)
{
    const char *errstr;
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
		if(!strncmp(wname, "default", 8))
			new->path = mkqpath(Ditem, 0);
		else if(!strncmp(wname, "new", 4))
			new->path = mkqpath(Ditem, nitem);
		else {
			unsigned short i = cext_strtonum(wname, 1, 0xffff, &errstr);
			if(i >= nitem)
				return -1;
			new->path = mkqpath(Ditem, i);
		}
		break;
	case Fcolor:
	case Fdata:
		if(!i || i > nitem)
			return -1;
	default:
		new->type = IXP_QTFILE;
    	new->path = mkqpath(type, i);
		break;
	}
    return 0;
}

static int
xversion(IXPServer *s, IXPConn * c)
{
    if(strncmp(s->fcall.version, IXP_VERSION, strlen(IXP_VERSION))) {
        s->errstr = "9P versions differ";
        return -1;
    } else if(s->fcall.maxmsg > IXP_MAX_MSG)
        s->fcall.maxmsg = IXP_MAX_MSG;
    s->fcall.id = RVERSION;
    return 0;
}

static int
xattach(IXPServer *s, IXPConn *c)
{
    Map *maps = c->aux;
    Map *new = cext_emallocz(sizeof(Map));

    c->aux = new;
	new->next = maps;
    new->qid = root_qid;
    new->fid = s->fcall.fid;
    s->fcall.id = RATTACH;
    s->fcall.qid = root_qid;
    return 0;
}

static int
xwalk(IXPServer *s, IXPConn *c)
{
    unsigned short nwqid = 0;
    Qid dir = root_qid;
    Map *map;

    if(!(map = fid_to_map(c->aux, s->fcall.fid))) {
        s->errstr = "no dir associated with fid";
        return -1;
    }
    if(s->fcall.fid != s->fcall.newfid
       && (fid_to_map(c->aux, s->fcall.newfid))) {
        s->errstr = "fid alreay in use";
        return -1;
    }
    if(s->fcall.nwname) {
        dir = map->qid;
        for(nwqid = 0; (nwqid < s->fcall.nwname)
            && !mkqid(&dir, s->fcall.wname[nwqid], &s->fcall.wqid[nwqid]); nwqid++)
            dir = s->fcall.wqid[nwqid];
        if(!nwqid) {
            s->errstr = "file not found";
            return -1;
        }
    }
    /* a fid will only be valid, if the walk was complete */
    if(nwqid == s->fcall.nwname) {
        if(s->fcall.fid != s->fcall.newfid) {
			Map *maps = c->aux;
			map = c->aux = cext_emallocz(sizeof(Map));
			map->next = maps;
        }
        map->qid = dir;
        map->fid = s->fcall.newfid;
    }
    s->fcall.id = RWALK;
    s->fcall.nwqid = nwqid;
    return 0;
}

static int
xopen(IXPServer *s, IXPConn *c)
{
    Map *map = fid_to_map(c->aux, s->fcall.fid);

    if(!map) {
        s->errstr = "invalid fid";
        return -1;
    }
    if(!(s->fcall.mode | IXP_OREAD) && !(s->fcall.mode | IXP_OWRITE)) {
		fprintf(stderr, "got mode 0x%x\n", s->fcall.mode);
        s->errstr = "mode not supported";
        return -1;
    }
    s->fcall.id = ROPEN;
    s->fcall.qid = map->qid;
    s->fcall.iounit =
        s->fcall.maxmsg - (sizeof(unsigned char) + sizeof(unsigned short) + 2 * sizeof(unsigned int));
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

static int
xremove(IXPServer *s, IXPConn *c)
{
    Map *map = fid_to_map(c->aux, s->fcall.fid);
	unsigned short i;

    if(!map) {
        s->errstr = "invalid fid";
        return -1;
    }
	i = qpath_item(map->qid.path);
    s->fcall.id = RREMOVE;
	if((qpath_type(map->qid.path) == Ditem) && i && (i < nitem)) {
		Item *it = item[i];
		detach_item(it);
		free(it);
		return 0;
	}
	s->errstr = "permission denied";
	return -1;
}

static int
xread(IXPServer *s, IXPConn *c)
{
	Stat stat;
    Map *map = fid_to_map(c->aux, s->fcall.fid);
    unsigned char *p = s->fcall.data;
	unsigned short i;
	char buf[32];

    if(!map) {
        s->errstr = "invalid fid";
        return -1;
    }
	i = qpath_item(map->qid.path);
    s->fcall.id = RREAD;
	s->fcall.count = 0; /* EOF by default */
	if(!s->fcall.offset) {
		switch (qpath_type(map->qid.path)) {
		case Droot:
			if(align == SOUTH || align == NORTH)
				s->fcall.count = mkstat(&stat, &root_qid, "display", 6, DMREAD | DMWRITE);
			else
				s->fcall.count = mkstat(&stat, &root_qid, "display", 5, DMREAD | DMWRITE); /* none */
			p = ixp_enc_stat(p, &stat);
			s->fcall.count += mkstat(&stat, &root_qid, "font", strlen(font), DMREAD | DMWRITE);
			p = ixp_enc_stat(p, &stat);
			s->fcall.count += mkstat(&stat, &root_qid, "new", 0, DMDIR | DMREAD | DMEXEC);
			p = ixp_enc_stat(p, &stat);
			s->fcall.count += mkstat(&stat, &root_qid, "event", 0, DMREAD);
			p = ixp_enc_stat(p, &stat);
			s->fcall.count += mkstat(&stat, &root_qid, "default", 0, DMDIR | DMREAD | DMEXEC);
			p = ixp_enc_stat(p, &stat);
			for(i = 1; i < nitem; i++) {
				snprintf(buf, sizeof(buf), "%u", i);
				s->fcall.count += mkstat(&stat, &root_qid, buf, 0, DMDIR | DMREAD | DMEXEC);
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Ditem:
			if(i > nitem)
				goto error_xread;
			if(!i) {
				s->fcall.count = mkstat(&stat, &root_qid, "color", 24, DMREAD | DMWRITE);
				p = ixp_enc_stat(p, &stat);
				break;
			}
			{
				if(i == nitem)
					new_item();
				Qid dir = {IXP_QTDIR, 0, mkqpath(Ditem, i)};
				s->fcall.count = mkstat(&stat, &dir, "color", 24, DMREAD | DMWRITE);
				p = ixp_enc_stat(p, &stat);
				s->fcall.count += mkstat(&stat, &dir, "data", strlen(item[i]->data), DMREAD | DMWRITE);
				p = ixp_enc_stat(p, &stat);
			}
			break;
		case Fdisplay:
			switch(align) {
			case SOUTH:
				memcpy(p, "south", 5);
				s->fcall.count = 5;
				break;
			case NORTH:
				memcpy(p, "north", 5);
				s->fcall.count = 5;
				break;
			default:
				memcpy(p, "none", 4);
				s->fcall.count = 4;
				break;
			}
			break;
		case Ffont:
			if((s->fcall.count = strlen(font)))
				memcpy(p, font, s->fcall.count);
			break;
		case Fevent:
			break;
		case Fdata:
			if(i == nitem)
				new_item();
			if(i >= nitem)
				goto error_xread;
			if((s->fcall.count = strlen(item[i]->data)))
				memcpy(p, item[i]->data, s->fcall.count);
			break;
		case Fcolor:
			if(i == nitem)
				new_item();
			if(i >= nitem)
				goto error_xread;
			if((s->fcall.count = strlen(item[i]->color)))
				memcpy(p, item[i]->color, s->fcall.count);
			break;
		default:
error_xread:
            s->errstr = "invalid read request";
			return -1;
			break;
		}
	}

    return 0;
}

static int
xstat(IXPServer *s, IXPConn *c)
{
    Map *map = fid_to_map(c->aux, s->fcall.fid);
	unsigned short i;
	Qid dir;

    if(!map) {
        s->errstr = "invalid fid";
        return -1;
    }
   	dir.version = 0;
	dir.type = IXP_QTDIR;
	dir.path = mkqpath(Ditem, i);

    s->fcall.id = RSTAT;
    switch (qpath_type(map->qid.path)) {
    case Droot:
    case Ditem:
		mkstat(&s->fcall.stat, &root_qid, qid_to_name(&map->qid), 0, DMDIR | DMREAD | DMEXEC);
        break;
	case Fdisplay:
		if(align == SOUTH || align == NORTH)
			mkstat(&s->fcall.stat, &root_qid, qid_to_name(&map->qid), 6, DMREAD | DMWRITE);
		else
			mkstat(&s->fcall.stat, &root_qid, qid_to_name(&map->qid), 5, DMREAD | DMWRITE);
		break;
    case Fevent:
		mkstat(&s->fcall.stat, &root_qid, qid_to_name(&map->qid), 0, DMREAD);
		break;
    case Ffont:
		mkstat(&s->fcall.stat, &root_qid, qid_to_name(&map->qid), strlen(font), DMREAD | DMWRITE);
        break;
    case Fdata:
		mkstat(&s->fcall.stat, &dir, qid_to_name(&map->qid), strlen(item[i]->data), DMREAD | DMWRITE);
		break;	
    case Fcolor:
		mkstat(&s->fcall.stat, &dir, qid_to_name(&map->qid), 24, DMREAD | DMWRITE);
		break;
    default:
		s->errstr = "invalid stat request";
		return -1;
		break;
    }
    return 0;
}

static int
xwrite(IXPServer *s, IXPConn *c)
{
	char buf[256];
    Map *map = fid_to_map(c->aux, s->fcall.fid);
	unsigned short i;

    if(!map) {
        s->errstr = "invalid fid";
        return -1;
    }

	i = qpath_item(map->qid.path);
    s->fcall.id = RWRITE;
	switch (qpath_type(map->qid.path)) {
	case Fdisplay:
		if(s->fcall.count > 5)
			goto error_xwrite;
		memcpy(buf, s->fcall.data, s->fcall.count);
		buf[s->fcall.count] = 0;
		if(!blitz_strtoalign(&align, buf))
			goto error_xwrite;
		/* TODO: resize/hide */
		break;
	case Ffont:
		if(font)
			free(font);
		font = cext_emallocz(s->fcall.count + 1);
		memcpy(font, s->fcall.data, s->fcall.count);
		/* TODO: XQueryFont */
		break;
	case Fdata:
		{
			unsigned int len = s->fcall.count;
			if(i == nitem)
				new_item();
			if(!i || (i >= nitem))
				goto error_xwrite;
			if(len >= sizeof(item[i]->data))
				len = sizeof(item[i]->data) - 1;
			memcpy(item[i]->data, s->fcall.data, len);
			item[i]->data[len] = 0;
			/* TODO: redraw */
		}
		break;
	case Fcolor:
		if(i == nitem)
			new_item();
		if((i >= nitem) || (s->fcall.count >= 24))
			goto error_xwrite;
		memcpy(item[i]->color, s->fcall.data, s->fcall.count);
		item[i]->color[s->fcall.count] = 0;
		/* TODO: update color */
		break;
	default:
error_xwrite:
		s->errstr = "invalid write request";
		return -1;
		break;
	}
	return 0;
}

static int
xclunk(IXPServer *s, IXPConn *c)
{
    Map *maps = c->aux;
    Map *m, *map = fid_to_map(maps, s->fcall.fid);

    if(!map) {
        s->errstr = "invalid fid";
        return -1;
    }
    if(maps == map)
        c->aux = maps = maps->next;
    else {
        for(m = maps; m && m->next != map; m = m->next);
        m->next = map->next;
    }
    free(map);
    s->fcall.id = RCLUNK;
    return 0;
}

static void
freeconn(IXPServer *s, IXPConn *c)
{
    Map *m, *maps = c->aux;

    while((m = maps)) {
        c->aux = maps = maps->next;
        free(m);
    }
}

static IXPTFunc funcs[] = {
    {TVERSION, xversion},
    {TATTACH, xattach},
    {TWALK, xwalk},
	{TREMOVE, xremove},
    {TOPEN, xopen},
    {TREAD, xread},
    {TWRITE, xwrite},
    {TCLUNK, xclunk},
    {TSTAT, xstat},
    {0, 0}
};

int
main(int argc, char *argv[])
{
    int i;

    /* command line args */
    for(i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
        switch (argv[i][1]) {
        case 'v':
            fprintf(stdout, "%s", version[0]);
            exit(0);
            break;
        case 'a':
            if(i + 1 < argc)
                sockfile = argv[++i];
            else
                usage();
            break;
        default:
            usage();
            break;
        }
    }

    dpy = XOpenDisplay(0);
    if(!dpy) {
        fprintf(stderr, "%s", "wmiibar: cannot open display\n");
        exit(1);
    }
    XSetErrorHandler(dummy_error_handler);
    screen_num = DefaultScreen(dpy);

    if(ixp_server_init(&srv, sockfile, funcs, freeconn) == -1) {
        fprintf(stderr, "wmiibar: fatal: %s\n", srv.errstr);
        exit(1);
    }
    root_qid.type = IXP_QTDIR;
    root_qid.version = 0;
    root_qid.path = mkqpath(Droot, 0);

    mypid = getpid();
    atexit(exit_cleanup);

    /* default item settings */
	new_item();

    font = strdup("fixed");

    ixp_server_loop(&srv);
    if(srv.errstr) {
        fprintf(stderr, "wmiibar: fatal: %s\n", srv.errstr);
        ixp_server_deinit(&srv);
        exit(1);
    }
    ixp_server_deinit(&srv);
    return 0;
}
