/*
 *(C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 *See LICENSE file for license details.
 */

#include <sys/types.h>
#include <cext.h>

#define IXP_VERSION	"9P2000"
#define IXP_NOTAG	(unsigned short)~0U	/*Dummy tag */
#define IXP_NOFID	(unsigned int)~0	/*No auth */

enum {	IXP_MAX_VERSION = 32,
	IXP_MAX_ERROR = 128,
	IXP_MAX_CACHE = 32,
	IXP_MAX_MSG = 8192,
	IXP_MAX_FLEN = 128,
	IXP_MAX_ULEN = 32,
	IXP_MAX_WELEM = 16 };

/* 9P message types */
enum {	TVERSION = 100,
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

/* borrowed from libc.h of Plan 9 */
enum {	IXP_DMDIR = 0x80000000,		/* mode bit for directories */
	IXP_DMAPPEND = 0x40000000,	/* mode bit for append only files */
	IXP_DMEXCL = 0x20000000,	/* mode bit for exclusive use files */
	IXP_DMMOUNT = 0x10000000,	/* mode bit for mounted channel */
	IXP_DMAUTH = 0x08000000,	/* mode bit for authentication file */
	IXP_DMTMP = 0x04000000,		/* mode bit for non-backed-up file */
	IXP_DMREAD = 0x4<<6,		/* mode bit for read permission */
	IXP_DMWRITE = 0x2<<6,		/* mode bit for write permission */
	IXP_DMEXEC = 0x1<<6			/* mode bit for execute permission */
};

/* modes */
enum {	IXP_OREAD = 0x00,
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
enum {	IXP_QTDIR = 0x80,
	IXP_QTAPPEND = 0x40,
	IXP_QTEXCL = 0x20,
	IXP_QTMOUNT = 0x10,
	IXP_QTAUTH = 0x08,
	IXP_QTTMP = 0x04,
	IXP_QTSYMLINK = 0x02,
	IXP_QTLINK = 0x01,
	IXP_QTFILE = 0x00,
};

/* from libc.h in p9p */
enum {	OREAD		= 0,	/* open for read */
	OWRITE		= 1,	/* write */
	ORDWR		= 2,	/* read and write */
	OEXEC		= 3,	/* execute, == read but check execute permission */
	OTRUNC		= 16,	/* or'ed in (except for exec), truncate file first */
	OCEXEC		= 32,	/* or'ed in, close on exec */
	ORCLOSE		= 64,	/* or'ed in, remove on close */
	ODIRECT		= 128,	/* or'ed in, direct access */
	ONONBLOCK	= 256,	/* or'ed in, non-blocking call */
	OEXCL		= 0x1000,	/* or'ed in, exclusive use (create only) */
	OLOCK		= 0x2000,	/* or'ed in, lock after opening */
	OAPPEND		= 0x4000	/* or'ed in, append only */
};

/* bits in Qid.type */
enum {	QTDIR		= 0x80,		/* type bit for directories */
	QTAPPEND	= 0x40,		/* type bit for append only files */
	QTEXCL		= 0x20,		/* type bit for exclusive use files */
	QTMOUNT		= 0x10,		/* type bit for mounted channel */
	QTAUTH		= 0x08,		/* type bit for authentication file */
	QTTMP		= 0x04,		/* type bit for non-backed-up file */
	QTSYMLINK	= 0x02,		/* type bit for symbolic link */
	QTFILE		= 0x00		/* type bits for plain file */
};

/* bits in Dir.mode */
enum {	DMDIR		= 0x80000000,	/* mode bit for directories */
	DMAPPEND	= 0x40000000,	/* mode bit for append only files */
	DMEXCL		= 0x20000000,	/* mode bit for exclusive use files */
	DMMOUNT		= 0x10000000,	/* mode bit for mounted channel */
	DMAUTH		= 0x08000000,	/* mode bit for authentication file */
	DMTMP		= 0x04000000,	/* mode bit for non-backed-up file */
	DMSYMLINK	= 0x02000000,	/* mode bit for symbolic link (Unix, 9P2000.u) */
	DMDEVICE	= 0x00800000,	/* mode bit for device file (Unix, 9P2000.u) */
	DMNAMEDPIPE	= 0x00200000,	/* mode bit for named pipe (Unix, 9P2000.u) */
	DMSOCKET	= 0x00100000,	/* mode bit for socket (Unix, 9P2000.u) */
	DMSETUID	= 0x00080000,	/* mode bit for setuid (Unix, 9P2000.u) */
	DMSETGID	= 0x00040000,	/* mode bit for setgid (Unix, 9P2000.u) */
	
	DMREAD		= 0x4,		/* mode bit for read permission */
	DMWRITE		= 0x2,		/* mode bit for write permission */
	DMEXEC		= 0x1		/* mode bit for execute permission */
};


typedef struct Qid Qid;
struct Qid {
	unsigned char type;
	unsigned int version;
	unsigned long long path;
	/* internal use only */
	unsigned char dir_type;
};

/* stat structure */
typedef struct Stat {
	unsigned short type;
	unsigned int dev;
	Qid qid;
	unsigned int mode;
	unsigned int atime;
	unsigned int mtime;
	unsigned long long length;
	char *name;
	char *uid;
	char *gid;
	char *muid;
} Stat;

/* from fcall(3) in plan9port */
typedef struct Fcall {
	unsigned char type;
	unsigned short tag;
	unsigned int fid;
	union {
		struct { /* Tversion, Rversion */
			unsigned int msize;
			char	*version;
		};
		struct { /* Tflush */
			unsigned short oldtag;
		};
		struct { /* Rerror */
			char *ename;
		};
		struct { /* Ropen, Rcreate */
			Qid qid; /* +Rattach */
			unsigned int iounit;
		};
		struct { /* Rauth */
			Qid aqid;
		};
		struct { /* Tauth, Tattach */
			unsigned int	afid;
			char		*uname;
			char		*aname;
		};
		struct { /* Tcreate */
			unsigned int	perm;
			char		*name;
			unsigned char	mode; /* +Topen */
		};
		struct { /* Twalk */
			unsigned int	newfid;
			unsigned short	nwname;
			char	*wname[IXP_MAX_WELEM];
		};
		struct { /* Rwalk */
			unsigned short	nwqid;
			Qid	wqid[IXP_MAX_WELEM];
		};
		struct { /* Twrite */
			long long	offset; /* +Tread */
			/* +Rread */
			unsigned int	count; /* +Tread */
			unsigned char	*data;
		};
		struct { /* Twstat, Rstat */
			unsigned short	nstat;
			unsigned char	*stat;
		};
	};
} Fcall;

typedef struct IXPServer IXPServer;
typedef struct IXPConn IXPConn;
typedef struct IXPMap IXPMap;
typedef struct Intmap Intmap;

struct IXPMap {
	IXPMap *next;
	unsigned int fid;
	unsigned short sel;
	unsigned short nwqid;
	Qid wqid[IXP_MAX_WELEM];
};

typedef struct Intlist Intlist;
struct Intmap {
	unsigned long nhash;
	Intlist	**hash;
};

struct IXPConn {
	IXPServer	*srv;
	void		*aux;
	int		fd;
	void		(*read) (IXPConn *);
	void		(*close) (IXPConn *);
	char		closed;

	/* Implementation details */
	/* do not use */
	IXPConn		*next;
};

struct IXPServer {
	int running;
	IXPConn *conn;
	int maxfd;
	fd_set rd;
};

typedef struct IXPClient {
	int fd;
	unsigned int root_fid;
	Qid root_qid;
	Fcall fcall;
	char *errstr;
} IXPClient;

typedef struct P9Conn P9Conn;
typedef struct Fid {
	unsigned long	fid;
	char		omode;
	char		*uid;
	Qid		qid;
	void		*aux;
	
	/* Implementation details */
	/* do not use */
	P9Conn		*conn;
	Intmap		*map;
} Fid;

typedef struct Req Req;
struct Req {
	Fid	*fid;
	Fid	*newfid;
	Req	*oldreq;
	Fcall	ifcall;
	Fcall	ofcall;
	void	*aux;

	/* Implementation detail */
	/* do not use */
	P9Conn	*conn;
};

typedef struct P9Srv {
	void (*attach)(Req *r);
	void (*clunk)(Req *r);
	void (*create)(Req *r);
	void (*flush)(Req *r);
	void (*open)(Req *r);
	void (*read)(Req *r);
	void (*remove)(Req *r);
	void (*stat)(Req *r);
	void (*walk)(Req *r);
	void (*write)(Req *r);
	void (*freefid)(Fid *f);
} P9Srv;

/* client.c */
int ixp_client_dial(IXPClient *c, char *address, unsigned int rootfid);
void ixp_client_hangup(IXPClient *c);
int ixp_client_remove(IXPClient *c, unsigned int newfid, char *filepath);
int ixp_client_create(IXPClient *c, unsigned int dirfid, char *name,
		unsigned int perm, unsigned char mode);
int ixp_client_walk(IXPClient *c, unsigned int newfid, char *filepath);
int ixp_client_stat(IXPClient *c, unsigned int newfid, char *filepath);
int ixp_client_walkopen(IXPClient *c, unsigned int newfid, char *filepath,
		unsigned char mode);
int ixp_client_open(IXPClient *c, unsigned int newfid, unsigned char mode);
int ixp_client_read(IXPClient *c, unsigned int fid,
		unsigned long long offset, void *result,
		unsigned int res_len);
int ixp_client_write(IXPClient *c, unsigned int fid,
		unsigned long long offset,
		unsigned int count, unsigned char *data);
int ixp_client_close(IXPClient *c, unsigned int fid);
int ixp_client_do_fcall(IXPClient * c);

/* convert.c */
void ixp_pack_u8(unsigned char **msg, int *msize, unsigned char val);
void ixp_unpack_u8(unsigned char **msg, unsigned char *val);
void ixp_pack_u16(unsigned char **msg, int *msize, unsigned short val);
void ixp_unpack_u16(unsigned char **msg, unsigned short *val);
void ixp_pack_u32(unsigned char **msg, int *msize, unsigned int val);
void ixp_unpack_u32(unsigned char **msg, unsigned int *val);
void ixp_pack_u64(unsigned char **msg, int *msize, unsigned long long val);
void ixp_unpack_u64(unsigned char **msg, unsigned long long *val);
void ixp_pack_string(unsigned char **msg, int *msize, const char *s);
void ixp_unpack_strings(unsigned char **msg, unsigned short n, char **strings);
void ixp_unpack_string(unsigned char **msg, char **string, unsigned short *len);
void ixp_pack_data(unsigned char **msg, int *msize, unsigned char *data,
		unsigned int datalen);
void ixp_unpack_data(unsigned char **msg, unsigned char **data,
		unsigned int datalen);
void ixp_pack_prefix(unsigned char *msg, unsigned int size,
		unsigned char id, unsigned short tag);
void ixp_unpack_prefix(unsigned char **msg, unsigned int *size,
		unsigned char *id, unsigned short *tag);
void ixp_pack_qid(unsigned char **msg, int *msize, Qid *qid);
void ixp_unpack_qid(unsigned char **msg, Qid *qid);
void ixp_pack_stat(unsigned char **msg, int *msize, Stat *stat);
void ixp_unpack_stat(unsigned char **msg, Stat *stat);

/* request.c */
void respond(Req *r, char *error);
void serve_9pcon(IXPConn *c);

/* intmap.c */
void initmap(Intmap *m, unsigned long nhash, void *hash);
void freemap(Intmap *map, void (*destroy)(void*));
void execmap(Intmap *map, void (*destroy)(void*));
void* lookupkey(Intmap *map, unsigned long id);
void* insertkey(Intmap *map, unsigned long id, void *v);
int caninsertkey(Intmap *map, unsigned long id, void *v);
void* deletekey(Intmap *map, unsigned long id);

/* message.c */
unsigned short ixp_sizeof_stat(Stat *stat);
unsigned int ixp_fcall2msg(void *msg, Fcall *fcall, unsigned int msglen);
unsigned int ixp_msg2fcall(Fcall *call, void *msg, unsigned int msglen);

/* server.c */
IXPConn *ixp_server_open_conn(IXPServer *s, int fd, void *aux,
		void (*read)(IXPConn *c), void (*close)(IXPConn *c));
void ixp_server_close_conn(IXPConn *c);
char *ixp_server_loop(IXPServer *s);
IXPMap *ixp_server_fid2map(IXPConn *c, unsigned int fid);
unsigned int ixp_server_receive_fcall(IXPConn *c, Fcall *fcall);
int ixp_server_respond_fcall(IXPConn *c, Fcall *fcall);
int ixp_server_respond_error(IXPConn *c, Fcall *fcall, char *errstr);
void ixp_server_close(IXPServer *s);

/* socket.c */
int ixp_connect_sock(char *address);
int ixp_create_sock(char *address, char **errstr);

/* transport.c */
unsigned int ixp_send_message(int fd, void *msg, unsigned int msize, char **errstr);
unsigned int ixp_recv_message(int fd, void *msg, unsigned int msglen, char **errstr);
