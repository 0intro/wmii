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
 * /display				Fdisplay	'top', 'bottom', 'none'
 * /font				Ffont		<xlib font name>
 * /new					Fnew 		returns id of new item
 * /event				Fevent
 * /default/ 			Ditem
 * /default/bgcolor		Fcolor		<#RRGGBB, #RGB>
 * /default/fgcolor		Fcolor		<#RRGGBB, #RGB>
 * /default/bordercolor	Fcolor		<#RRGGBB, #RGB>
 * /1/					Ditem
 * /1/data 				Fdata		<arbitrary data which gets displayed>
 * /1/bgcolor			Fcolor		<#RRGGBB, #RGB>
 * /1/fgcolor			Fcolor		<#RRGGBB, #RGB>
 * /1/bordercolor		Fcolor		<#RRGGBB, #RGB> ...
 */
enum {                          /* 8-bit qid.path.type */
    Droot,
    Ditem,
    Fdisplay,
    Fnew,
    Fdata,                      /* data to display */
    Fevent,
    Fcolor,
    Ffont
};

#define NONE (unsigned short)0xffff

typedef struct {
    char *name;
    unsigned char type;
} QFile;

static QFile qfilelist[] = {
    {"display", Fdisplay},
    {"font", Ffont},
    {"new", Fnew},
    {"data", Fdata},
    {"bgcolor", Fcolor},
    {"fgcolor", Fcolor},
    {"bordercolor", Fcolor},
    {"event", Fevent},
    {0, 0},
};

typedef struct Map Map;
struct Map {
    unsigned int fid;
    Qid qid;
    Map *next;
};

typedef struct {
    int id;
    char text[256];
    int value;
    unsigned long bg;
    unsigned long fg;
    unsigned long border[4];
    XFontStruct *font;
    char event[5][256];
} Item;

static size_t nitems = 0;
static char *sockfile = nil;
static pid_t mypid = 0;
static IXPServer srv = { 0 };
static Qid root_qid;
static Display *dpy;
static int screen_num;
static char *align = nil;
static char *font = nil;
/*
static GC gc;
static Window win;
static XRectangle geom;
static int mapped = 0;
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
            "      -v    version info\n", NONE);
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

static unsigned long long
make_qpath(unsigned char type, unsigned short item, unsigned short file)
{
    return ((unsigned long long) file << 24) | ((unsigned long long) item << 8) | (unsigned long long) type;
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

/*
static          unsigned short
qpath_file(unsigned long long path)
{
	return (path >> 24) & 0xffff;
}
*/

static Map *
fid_to_map(Map * maps, unsigned int fid)
{
    Map *m;
    for(m = maps; m; m = m->next)
        if(m->fid == fid)
            return m;
    return nil;
}

static Bool
qfile_index(char *name, unsigned short *index)
{
    int i;
    for(i = 0; qfilelist[i].name; i++)
        if(!strncmp(name, qfilelist[i].name, strlen(qfilelist[i].name))) {
            *index = i;
            return True;
        }
    return False;
}

static Bool
mkqid(Qid * dir, char *wname, Qid * new)
{
    unsigned short idx;
    const char *errstr;
    if(dir->type != IXP_QTDIR)
        return False;
    new->version = nil;
    if(!qfile_index(wname, &idx)) {
        new->type = IXP_QTDIR;
        if(!strncmp(wname, "..", 3)) {
            *new = root_qid;
            return True;
        } else if(!strncmp(wname, "default", 8)) {
            new->path = make_qpath(Ditem, 0, NONE);
            return True;
        }
        /* check if wname is a number, otherwise file not found */
        idx = (unsigned short) cext_strtonum(wname, 1, 0xffff, &errstr);
        if(errstr || nitems < idx)
            return False;
        /* found */
        new->path = make_qpath(Ditem, idx, NONE);
    } else {
        new->type = IXP_QTFILE;
        new->path =
            make_qpath(qfilelist[idx].type, qpath_item(dir->path), idx);
    }
    return True;
}

static int
xattach(IXPServer * s, IXPConn * c)
{
    Map *maps = c->aux;
    Map *m, *new = cext_emallocz(sizeof(Map));

    if(!maps)
        c->aux = new;
    else {
        for(m = maps; m && m->next; m = m->next); 
        m->next = new;
    }
    /*fprintf(stderr, "attaching %u %u %s %s\n", s->fcall.fid, s->fcall.afid,
            s->fcall.uname, s->fcall.aname);
			*/
    new->qid = root_qid;
    new->fid = s->fcall.fid;
    s->fcall.id = RATTACH;
    s->fcall.qid = root_qid;
    return 0;
}

static int
xwalk(IXPServer * s, IXPConn * c)
{
    unsigned short nwqid = 0;
    Qid qid;
    Map *map;

    /*fprintf(stderr, "%s", "walking\n");*/
    if(!(map = fid_to_map(c->aux, s->fcall.fid))) {
        s->errstr = "no directory associated with fid";
        return -1;
    }
    if(s->fcall.fid != s->fcall.newfid
       && (fid_to_map(c->aux, s->fcall.newfid))) {
        s->errstr = "fid alreay in use";
        return -1;
    }
    if(s->fcall.nwname) {
        qid = map->qid;
        for(nwqid = 0; (nwqid < s->fcall.nwname)
            && mkqid(&qid, s->fcall.wname[nwqid], &s->fcall.wqid[nwqid]); nwqid++)
            qid = s->fcall.wqid[nwqid];
        if(!nwqid) {
            s->errstr = "file not found";
            return -1;
        }
    }
    /* a fid will only be valid, if the walk was complete */
    if(nwqid == s->fcall.nwname) {
        Map *m, *maps = c->aux;
        if(s->fcall.fid == s->fcall.newfid) {
            if(maps == map)
                maps = maps->next;
            else {
                for(m = maps; m && m->next != map; m = m->next);
                m->next = map->next;
            }
            free(map);
        }
        map = cext_emallocz(sizeof(Map));
        map->qid = qid;
        map->fid = s->fcall.newfid;
        for(m = maps; m && m->next; m = m->next);
        if(!m)
            maps = map;
        else
            m->next = map;
    }
    s->fcall.id = RWALK;
    s->fcall.nwqid = nwqid;
    return 0;
}

static int
xopen(IXPServer * s, IXPConn * c)
{
    Map *map = fid_to_map(c->aux, s->fcall.fid);

    /*fprintf(stderr, "%s", "opening\n");*/
    if(!map) {
        s->errstr = "invalid fid";
        return -1;
    }
    if((s->fcall.mode != IXP_OREAD) && (s->fcall.mode != IXP_OWRITE)) {
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
mkstat(Stat *stat, char *name, unsigned long long length, unsigned int mode)
{
    stat->mode = 0xfff | mode; /* --rwxrwxrwx */
    stat->atime = stat->mtime = time(0);
    cext_strlcpy(stat->uid, getenv("USER"), sizeof(stat->uid));
    cext_strlcpy(stat->gid, getenv("USER"), sizeof(stat->gid));
    cext_strlcpy(stat->muid, getenv("USER"), sizeof(stat->muid));

    cext_strlcpy(stat->name, name, sizeof(stat->name));
    stat->length = length;
    mkqid(&root_qid, name, &stat->qid);

	return ixp_sizeof_stat(stat);
}

static int
xread(IXPServer * s, IXPConn * c)
{
	Stat stat;
    Map *map = fid_to_map(c->aux, s->fcall.fid);
    unsigned char *p = s->fcall.data;

    /*fprintf(stderr, "reading %lld\n", s->fcall.offset);*/
    if(!map) {
        s->errstr = "invalid fid";
        return -1;
    }
    /*fprintf(stderr, "%d\n", qpath_item(map->qid.path));*/
    switch (qpath_type(map->qid.path)) {
    default:
    case Droot:
		s->fcall.count = mkstat(&stat, "display", strlen(align), 0x0);
        p = ixp_enc_stat(p, &stat);
        s->fcall.count += mkstat(&stat, "font", strlen(font), 0x0);
        p = ixp_enc_stat(p, &stat);
        s->fcall.count += mkstat(&stat, "new", 0, 0x0);
        p = ixp_enc_stat(p, &stat);
        s->fcall.count += mkstat(&stat, "event", 0, 0x0);
        p = ixp_enc_stat(p, &stat);
        s->fcall.count += mkstat(&stat, "default", 0, DMDIR);
        p = ixp_enc_stat(p, &stat);
		/* todo: add all labels */
        s->fcall.count += mkstat(&stat, "1", 0, DMDIR);
        p = ixp_enc_stat(p, &stat);
        s->fcall.count += mkstat(&stat, "2", 0, DMDIR);
        p = ixp_enc_stat(p, &stat);
        s->fcall.count += mkstat(&stat, "3", 0, DMDIR);
        p = ixp_enc_stat(p, &stat);
        s->fcall.id = RREAD;
		if(s->fcall.offset >= s->fcall.count)
			s->fcall.count = 0; /* EOF */
        break;
    case Ditem:
        break;
    case Fdisplay:
        break;
    case Fnew:
        break;
    case Fdata:
        break;
    case Fevent:
        break;
    case Fcolor:
        break;
    case Ffont:
        break;
    }

    return 0;
}

static int
xstat(IXPServer * s, IXPConn * c)
{
    Map *map = fid_to_map(c->aux, s->fcall.fid);

    /*fprintf(stderr, "%s", "stating\n");*/
    if(!map) {
        s->errstr = "invalid fid";
        return -1;
    }

    s->fcall.id = RSTAT;
	s->fcall.stat.mode = 0xfff | DMDIR;
    s->fcall.stat.atime = s->fcall.stat.mtime = time(0);

	/*fprintf(stderr, "atime=%ld\n", s->fcall.stat.atime);*/
    cext_strlcpy(s->fcall.stat.uid, getenv("USER"), sizeof(s->fcall.stat.uid));
    cext_strlcpy(s->fcall.stat.gid, getenv("USER"), sizeof(s->fcall.stat.gid));
    cext_strlcpy(s->fcall.stat.muid, getenv("USER"), sizeof(s->fcall.stat.muid));

    /*fprintf(stderr, "%d\n", qpath_item(map->qid.path));*/
    switch (qpath_type(map->qid.path)) {
    default:
    case Droot:
		s->fcall.stat.name[0] = '/';
		s->fcall.stat.name[1] = 0;
        s->fcall.stat.length = 0;
		s->fcall.stat.qid = root_qid;
    /*fprintf(stderr, "stat: %ld %ld \n", s->fcall.stat.type, s->fcall.stat.dev);*/
    /*fprintf(stderr, "qid: %ld %ld %lld\n", root_qid.type, root_qid.version, root_qid.path);*/
        break;
    case Ditem:
        break;
    case Fdisplay:
        break;
    case Fnew:
        break;
    case Fdata:
        break;
    case Fevent:
        break;
    case Fcolor:
        break;
    case Ffont:
        break;
    }

    return 0;
}

static int
xwrite(IXPServer * s, IXPConn * c)
{
    return -1;
}

static int
xclunk(IXPServer * s, IXPConn * c)
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
freeconn(IXPServer * s, IXPConn * c)
{
    Map *m, *maps = c->aux;

	/*fprintf(stderr, "%s", "freecon\n");*/
    while((m = maps)) {
        c->aux = maps = maps->next;
        free(m);
    }
}

static IXPTFunc funcs[] = {
    {TVERSION, ixp_server_tversion},
    {TATTACH, xattach},
    {TWALK, xwalk},
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
    Item *item;

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
    root_qid.path = make_qpath(Droot, NONE, NONE);

    mypid = getpid();
    atexit(exit_cleanup);

    /* default item settings */
    item = cext_emallocz(sizeof(Item));

    align = "bottom";
    font = "fixed";

    ixp_server_loop(&srv);
    if(srv.errstr) {
        fprintf(stderr, "wmiibar: fatal: %s\n", srv.errstr);
        ixp_server_deinit(&srv);
        exit(1);
    }
    ixp_server_deinit(&srv);
    return 0;
}
