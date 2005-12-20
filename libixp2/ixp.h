/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <sys/types.h>

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef nil
#define nil 0
#endif

#define IXP_VERSION   	"9P2000"
#define IXP_MAX_VERSION 32
#define IXP_MAX_ERROR   128
#define IXP_MAX_CONN  	32
#define IXP_MAX_MSG   	8192
#define IXP_MAX_FLEN  	128
#define IXP_MAX_ULEN  	32
#define IXP_MAX_STAT  	64
#define IXP_MAX_WELEM 	16		/* MAXWELEM */
#define IXP_MAX_TFUNCS	14


/*
	size[4] Tversion tag[2] msize[4] version[s]
	size[4] Rversion tag[2] msize[4] version[s]
	size[4] Tauth tag[2] afid[4] uname[s] aname[s]
	size[4] Rauth tag[2] aqid[13]
	size[4] Rerror tag[2] ename[s]
	size[4] Tflush tag[2] oldtag[2]
	size[4] Rflush tag[2]
	size[4] Tattach tag[2] fid[4] afid[4] uname[s] aname[s]
	size[4] Rattach tag[2] qid[13]
	size[4] Twalk tag[2] fid[4] newfid[4] nwname[2] nwname*(wname[s])
	size[4] Rwalk tag[2] nwqid[2] nwqid*(wqid[13])
	size[4] Topen tag[2] fid[4] mode[1]
	size[4] Ropen tag[2] qid[13] iounit[4]
	size[4] Tcreate tag[2] fid[4] name[s] perm[4] mode[1]
	size[4] Rcreate tag[2] qid[13] iounit[4]
	size[4] Tread tag[2] fid[4] offset[8] count[4]
	size[4] Rread tag[2] count[4] data[count]
	size[4] Twrite tag[2] fid[4] offset[8] count[4] data[count]
	size[4] Rwrite tag[2] count[4]
	size[4] Tclunk tag[2] fid[4]
	size[4] Rclunk tag[2]
	size[4] Tremove tag[2] fid[4]
	size[4] Rremove tag[2]
	size[4] Tstat tag[2] fid[4]
	size[4] Rstat tag[2] stat[n]
	size[4] Twstat tag[2] fid[4] stat[n]
	size[4] Rwstat tag[2]

	stat[n]:
	size[2]			total byte count of the following data
	type[2]			for kernel use
	dev[4]			for kernel use
	qid.type[1]		the type of the file (directory, etc.),
					represented as a bit vector corresponding to the high 8 bits of the file's mode word.
	qid.vers[4]		version number for given path
	qid.path[8]		the file server's unique identification for the file
	mode[4]			permissions and flags
	atime[4]		last access time
	mtime[4]		last modification time
	length[8]		length of file in bytes
	name[ s ]		file name; must be / if the file is the root directory of the server
	uid[ s ]		owner name
	gid[ s ]		group name
	muid[ s ]
*/

/* 9P message types */
enum {
	TVERSION = 100,
	RVERSION,
	TAUTH = 102,
	RAUTH,
	TATTACH = 104,
	RATTACH,
	TERROR = 106,
	RERROR,
	TFLUSH = 108,
	RFLUSH,
	TWALK = 110,
	RWALK,
	TOPEN = 112,
	ROPEN,
	TCREATE = 114,
	RCREATE,
	TREAD = 116,
	RREAD,
	TWRITE = 118,
	RWRITE,
	TCLUNK = 120,
	RCLUNK,
	TREMOVE = 122,
	RREMOVE,
	TSTAT = 124,
	RSTAT,
	TWSTAT = 126,
	RWSTAT,
};

/* modes */
enum {
	IXP_OREAD = 0x00,
	IXP_OWRITE = 0x01,
	IXP_ORDWR = 0x02,
	IXP_OEXEC = 0x03,
	IXP_OEXCL = 0x04,
	IXP_OTRUNC = 0x10,
	IXP_OREXEC = 0x20,
	IXP_ORCLOSE = 0x40,
	IXP_OAPPEND = 0x80,
};

/* qid.types */
enum {
	IXP_QTDIR = 0x80,
	IXP_QTAPPEND = 0x40,
	IXP_QTEXCL = 0x20,
	IXP_QTMOUNT = 0x10,
	IXP_QTAUTH = 0x08,
	IXP_QTTMP = 0x04,
	IXP_QTSYMLINK = 0x02,
	IXP_QTLINK = 0x01,
	IXP_QTFILE = 0x00,
};

/* this should work on all architectures */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

#define IXP_NOTAG    (u16)~0U	/* Dummy tag */
#define IXP_NOFID    (u32)~0	/* No auth */

typedef struct {
	u8 type;
	u32 version;
	u64 path;
} Qid;

/* stat structure */
typedef struct {
	u16 size;
	u16 type;
	u32 dev;
	Qid qid;
	u32 mode;
	u32 atime;
	u32 mtime;
	u64 length;
	char name[IXP_MAX_FLEN];
	char uid[IXP_MAX_ULEN];
	char gid[IXP_MAX_ULEN];
	char muid[IXP_MAX_ULEN];
} Stat;

typedef struct {
	u8 id;
	u16 tag;
	u32 fid;
	u32 maxmsg;					/* Tversion, Rversion */
	char version[IXP_MAX_VERSION];	/* Tversion, Rversion */
	u16 oldtag;					/* Tflush */
	char errstr[IXP_MAX_ERROR];	/* Rerror */
	Qid qid;					/* Rattach, Ropen, Rcreate */
	u32 iounit;					/* Ropen, Rcreate */
	Qid aqid;					/* Rauth */
	u32 afid;					/* Tauth, Tattach */
	char uname[IXP_MAX_ULEN];	/* Tauth, Tattach */
	char aname[IXP_MAX_FLEN];	/* Tauth, Tattach */
	u32 perm;					/* Tcreate */
	char name[IXP_MAX_FLEN];	/* Tcreate */
	u8 mode;					/* Tcreate, Topen */
	u32 newfid;					/* Twalk */
	u16 nwname;					/* Twalk */
	char *wname[IXP_MAX_WELEM];	/* Twalk */
	u16 nwqid;					/* Rwalk */
	Qid wqid[IXP_MAX_WELEM];	/* Rwalk */
	u64 offset;					/* Tread, Twrite */
	u32 count;					/* Tread, Twrite, Rread */
	Stat stat;					/* Rstat */
	u16 nstat;					/* Twstat, Rstat */
	u8 data[IXP_MAX_MSG];		/* Twrite, Rread, Twstat,
								 * Rstat */
} Fcall;

typedef struct IXPServer IXPServer;
typedef struct IXPConn IXPConn;
typedef struct {
	u8 id;
	int (*tfunc) (IXPServer *, IXPConn *);
} IXPTFunc;

struct IXPConn {
	int fd;
	int dont_close;
	void (*read) (IXPServer *, IXPConn *);
	void *aux;
};

struct IXPServer {
	int running;
	IXPConn conn[IXP_MAX_CONN];
	void (*freeconn) (IXPServer *, IXPConn *);
	int maxfd;
	fd_set rd;
	IXPTFunc *funcs;
	Fcall fcall;
	char *errstr;
};

typedef struct {
	int fd;
	u32 root_fid;
	Qid root_qid;
	Fcall fcall;
	char *errstr;
} IXPClient;

/* client.c */
int ixp_client_init(IXPClient * c, char *sockfile);
void ixp_client_deinit(IXPClient * c);
int ixp_client_remove(IXPClient * c, u32 newfid, char *filepath);
int ixp_client_create(IXPClient * c, u32 dirfid, char *name, u32 perm, u8 mode);
int ixp_client_walk(IXPClient * c, u32 newfid, char *filepath);
int ixp_client_open(IXPClient * c, u32 newfid, char *filepath, u8 mode);
u32 ixp_client_read(IXPClient * c, u32 fid, u64 offset, void *result, u32 res_len);
u32 ixp_client_write(IXPClient * c, u32 fid, u64 offset, u32 count, u8 * data);
int ixp_client_close(IXPClient * c, u32 fid);

/* convert.c */
void *ixp_enc_u8(u8 * msg, u8 val);
void *ixp_dec_u8(u8 * msg, u8 * val);
void *ixp_enc_u16(u8 * msg, u16 val);
void *ixp_dec_u16(u8 * msg, u16 * val);
void *ixp_enc_u32(u8 * msg, u32 val);
void *ixp_dec_u32(u8 * msg, u32 * val);
void *ixp_enc_u64(u8 * msg, u64 val);
void *ixp_dec_u64(u8 * msg, u64 * val);
void *ixp_enc_string(u8 * msg, const char *s);
void *ixp_dec_string(u8 * msg, char *string, u16 stringlen, u16 * len);
void *ixp_enc_data(u8 * msg, u8 * data, u32 datalen);
void *ixp_dec_data(u8 * msg, u8 * data, u32 datalen);
void *ixp_enc_prefix(u8 * msg, u32 size, u8 id, u16 tag);
void *ixp_dec_prefix(u8 * msg, u32 * size, u8 * id, u16 * tag);
void *ixp_enc_qid(u8 * msg, Qid * qid);
void *ixp_dec_qid(u8 * msg, Qid * qid);
void *ixp_enc_stat(u8 * msg, Stat * stat);
void *ixp_dec_stat(u8 * msg, Stat * stat);

/* message.c */
u16 ixp_sizeof_stat(Stat * stat);
u32 ixp_fcall_to_msg(Fcall * fcall, void *msg, u32 msglen);
u32 ixp_msg_to_fcall(void *msg, u32 msglen, Fcall * fcall);

/* server.c */
IXPConn *ixp_server_add_conn(IXPServer * s, int fd, int dont_close, void (*read) (IXPServer *, IXPConn *));
int ixp_server_tversion(IXPServer *, IXPConn * c);
void ixp_server_rm_conn(IXPServer * s, IXPConn * c);
void ixp_server_loop(IXPServer * s);
int ixp_server_init(IXPServer * s, char *sockfile, IXPTFunc * funcs, void (*freeconn) (IXPServer *, IXPConn *));
void ixp_server_deinit(IXPServer * s);

/* socket.c */
int ixp_connect_sock(char *sockfile);
int ixp_accept_sock(int fd);
int ixp_create_sock(char *sockfile, char **errstr);

/* transport.c */
u32 ixp_send_message(int fd, void *msg, u32 msize, char **errstr);
u32 ixp_recv_message(int fd, void *msg, u32 msglen, char **errstr);
