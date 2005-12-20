/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
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
 * /default/ 			Ditem
 * /default/bNpress		Fevent		<command, gets executed>
 * /default/bgcolor		Fcolor		<#RRGGBB, #RGB>
 * /default/fgcolor		Fcolor		<#RRGGBB, #RGB>
 * /default/bordercolor	Fcolor		<#RRGGBB, #RGB>
 * /1/					Ditem
 * /1/data 				Fdata		<arbitrary data which gets displayed>
 * /1/bNpress			Fevent		<command, gets executed>
 * /1/bgcolor			Fcolor		<#RRGGBB, #RGB>
 * /1/fgcolor			Fcolor		<#RRGGBB, #RGB>
 * /1/bordercolor		Fcolor		<#RRGGBB, #RGB> ...
 */
enum {							/* 8-bit qid.path.type */
	Droot,
	Ditem,
	Fdisplay,
	Fnew,
	Fdata,						/* data to display */
	Fevent,
	Fcolor,
	Ffont
};

#define NONE (u16)0xffff

typedef struct {
	char *name;
	u8 type;
} QFile;

static QFile qfilelist[] = {
	{"display", Fdisplay},
	{"font", Ffont},
	{"new", Fnew},
	{"data", Fdata},
	{"bgcolor", Fcolor},
	{"fgcolor", Fcolor},
	{"bordercolor", Fcolor},
	{"b1press", Fevent},
	{"b2press", Fevent},
	{"b3press", Fevent},
	{"b4press", Fevent},
	{"b5press", Fevent},
	{0, 0},
};

typedef struct Map Map;
struct Map {
	u32 fid;
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
		"  (C)opyright MMIV-MMV Anselm R. Garbe\n", 0
};

static void usage()
{
	fprintf(stderr, "%s %d",
			"usage: wmiibar -s <socket file> [-v]\n"
			"      -s    socket file\n" "      -v    version info\n",
			NONE);
	exit(1);
}

static int dummy_error_handler(Display * dpy, XErrorEvent * err)
{
	return 0;
}

static void exit_cleanup()
{
	if (mypid == getpid())
		unlink(sockfile);
}

static u64 make_qpath(u8 type, u16 item, u16 file)
{
	return ((u64) file << 24) | ((u64) item << 8) | (u64) type;
}

static u8 qpath_type(u64 path)
{
	return path & 0xff;
}

static u16 qpath_item(u64 path)
{
	return (path >> 8) & 0xffff;
}

/*
static          u16
qpath_file(u64 path)
{
	return (path >> 24) & 0xffff;
}
*/

static Map *fid_to_map(Map *maps, u32 fid)
{
	Map *m;
	for (m = maps; m; m = m->next)
		if (m->fid == fid)
			return m;
	return nil;
}

static Bool qfile_index(char *name, u16 * index)
{
	int i;
	for (i = 0; qfilelist[i].name; i++)
		if (!strncmp(name, qfilelist[i].name, strlen(qfilelist[i].name))) {
			*index = i;
			return True;
		}
	return False;
}

static Bool make_qid(Qid * dir, char *wname, Qid * new)
{
	u16 idx;
	const char *errstr;
	if (dir->type != IXP_QTDIR)
		return False;
	new->version = nil;
	if (!qfile_index(wname, &idx)) {
		new->type = IXP_QTDIR;
		if (!strncmp(wname, "..", 3)) {
			*new = root_qid;
			return True;
		} else if (!strncmp(wname, "default", 8)) {
			new->path = make_qpath(Ditem, 0, NONE);
			return True;
		}
		/* check if wname is a number, otherwise file not found */
		idx = (u16) cext_strtonum(wname, 1, 0xffff, &errstr);
		if (errstr || nitems < idx)
			return False;
		/* found */
		new->path = make_qpath(Ditem, idx, NONE);
	} else {
		new->type = IXP_QTFILE;
		new->path = make_qpath(qfilelist[idx].type, qpath_item(dir->path), idx);
	}
	return True;
}

static int attach(IXPServer *s, IXPConn *c)
{
	Map *maps = c->aux;
	Map *m, *new = cext_emallocz(sizeof(Map));

	if (!maps)
		c->aux = new;
	else {
		for (m = maps; m && m->next; m = m->next);
		m->next = new; 
	}
	fprintf(stderr, "attaching %u %u %s %s\n", s->fcall.fid, s->fcall.afid, s->fcall.uname, s->fcall.aname);
	new->qid = root_qid;
	new->fid = s->fcall.fid;
	s->fcall.id = RATTACH;
	s->fcall.qid = root_qid;
	return TRUE;
}

static int walk(IXPServer * s, IXPConn * c)
{
	u16 nwqid = 0;
	Qid qid;
	Map *map;

	fprintf(stderr, "%s", "walking\n");
	if (!(map = fid_to_map(c->aux, s->fcall.fid))) {
		s->errstr = "no directory associated with fid";
		return FALSE;
	}
	if (s->fcall.fid != s->fcall.newfid
		&& (fid_to_map(c->aux, s->fcall.newfid))) {
		s->errstr = "fid alreay in use";
		return FALSE;
	}
	if (s->fcall.nwname) {
		qid = map->qid;
		for (nwqid = 0; (nwqid < s->fcall.nwname) && make_qid(&qid, s->fcall.wname[nwqid], &s->fcall.wqid[nwqid]); nwqid++)
			qid = s->fcall.wqid[nwqid];
		if (!nwqid) {
			s->errstr = "file not found";
			return FALSE;
		}
	}
	/*
	 * following condition is required by 9P, a fid will only be valid if
	 * the walk was complete
	 */
	if (nwqid == s->fcall.nwname) {
		Map *m, *maps = c->aux;
		if (s->fcall.fid == s->fcall.newfid) {
			if (maps == map)
				maps = maps->next;
			else {
				for (m = maps; m && m->next != map; m = m->next);
				m->next = map->next;
			}
			free(map);
		}
		map = cext_emallocz(sizeof(Map));
		map->qid = qid;
		map->fid = s->fcall.newfid;
		for (m = maps; m && m->next; m = m->next);
		if (!m)
			maps = map;
		else
			m->next = map;
	}
	s->fcall.id = RWALK;
	s->fcall.nwqid = nwqid;
	return TRUE;
}

static int _open(IXPServer * s, IXPConn * c)
{
	Map *map = fid_to_map(c->aux, s->fcall.fid);

	fprintf(stderr, "%s", "opening\n");
	if (!map) {
		s->errstr = "invalid fid";
		return FALSE;
	}
	if ((s->fcall.mode != IXP_OREAD) && (s->fcall.mode != IXP_OWRITE)) {
		s->errstr = "mode not supported";
		return FALSE;
	}
	s->fcall.id = ROPEN;
	s->fcall.qid = map->qid;
	s->fcall.iounit =
		s->fcall.maxmsg - (sizeof(u8) + sizeof(u16) + 2 * sizeof(u32));
	return TRUE;
}

static int _read(IXPServer * s, IXPConn * c)
{
	Map *map = fid_to_map(c->aux, s->fcall.fid);
	Stat stat = { 0 };
	u8 *p;

	fprintf(stderr, "%s", "reading\n");
	if (!map) {
		s->errstr = "invalid fid";
		return FALSE;
	}
	stat.mode = 0xff;
	stat.atime = stat.mtime = time(0);
	cext_strlcpy(stat.uid, getenv("USER"), sizeof(stat.uid));
	cext_strlcpy(stat.gid, getenv("USER"), sizeof(stat.gid));
	cext_strlcpy(stat.muid, getenv("USER"), sizeof(stat.muid));

	fprintf(stderr, "%d\n", qpath_item(map->qid.path));
	switch (qpath_type(map->qid.path)) {
	default:
	case Droot:
		p = s->fcall.data;
		cext_strlcpy(stat.name, "display", sizeof(stat.name));
		stat.length = strlen(align);
		make_qid(&root_qid, "display", &stat.qid);
		stat.size = ixp_sizeof_stat(&stat);
		s->fcall.count = stat.size;
		p = ixp_enc_stat(p, &stat);
		cext_strlcpy(stat.name, "font", sizeof(stat.name));
		stat.length = strlen(font);
		make_qid(&root_qid, "font", &stat.qid);
		stat.size = ixp_sizeof_stat(&stat);;
		s->fcall.count += stat.size;
		p = ixp_enc_stat(p, &stat);
		cext_strlcpy(stat.name, "new", sizeof(stat.name));
		stat.length = 0;
		make_qid(&root_qid, "new", &stat.qid);
		stat.size = ixp_sizeof_stat(&stat);;
		s->fcall.count += stat.size;
		p = ixp_enc_stat(p, &stat);
		s->fcall.id = RREAD;
		fprintf(stderr, "%d msize\n", s->fcall.count);
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

	return TRUE;
}

static int _write(IXPServer *s, IXPConn *c)
{

	return FALSE;
}

static int clunk(IXPServer *s, IXPConn *c)
{
	Map *maps = c->aux;
	Map *m, *map = fid_to_map(maps, s->fcall.fid);

	if (!map) {
		s->errstr = "invalid fid";
		return FALSE;
	}
	if (maps == map)
		maps = maps->next;
	else {
		for (m = maps; m && m->next != map; m = m->next);
		m->next = map->next;
	}
	free(map);
	s->fcall.id = RCLUNK;
	return TRUE;
}

static void freeconn(IXPServer * s, IXPConn * c)
{
	Map *m, *maps = c->aux;

	while ((m = maps)) {
		maps = maps->next;
		free(m);
	}
	c->aux = nil;	
}

static IXPTFunc funcs[] = {
	{TVERSION, ixp_server_tversion},
	{TATTACH, attach},
	{TWALK, walk},
	{TOPEN, _open},
	{TREAD, _read},
	{TWRITE, _write},
	{TCLUNK, clunk},
	{0, 0}
};

int main(int argc, char *argv[])
{
	int i;
	Item *item;

	/* command line args */
	for (i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
		switch (argv[i][1]) {
		case 'v':
			fprintf(stdout, "%s", version[0]);
			exit(0);
			break;
		case 's':
			if (i + 1 < argc)
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
	if (!dpy) {
		fprintf(stderr, "%s", "wmiibar: cannot open display\n");
		exit(1);
	}
	XSetErrorHandler(dummy_error_handler);
	screen_num = DefaultScreen(dpy);

	if (!ixp_server_init(&srv, sockfile, funcs, freeconn)) {
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
	if (srv.errstr) {
		fprintf(stderr, "wmiibar: fatal: %s\n", srv.errstr);
		ixp_server_deinit(&srv);
		exit(1);
	}
	ixp_server_deinit(&srv);
	return 0;
}
