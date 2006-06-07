/*
 *(C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 *See LICENSE file for license details.
 */

#include <sys/types.h>
#include <cext.h>

#define IXP_VERSION	"9P2000"
#define IXP_NOTAG	(unsigned short)~0U	/*Dummy tag */
#define IXP_NOFID	(unsigned int)~0	/*No auth */

enum { IXP_MAX_VERSION = 32 };
enum { IXP_MAX_ERROR = 128 };
enum { IXP_MAX_CACHE = 32 };
enum { IXP_MAX_MSG = 8192 };
enum { IXP_MAX_FLEN = 128 };
enum { IXP_MAX_ULEN = 32 };
enum { IXP_MAX_WELEM = 16 };

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

/* borrowed from libc.h of Plan 9 */
enum {
	IXP_DMDIR = 0x80000000,		/* mode bit for directories */
	IXP_DMAPPEND = 0x40000000,	/* mode bit for append only files */
	IXP_DMEXCL = 0x20000000,	/* mode bit for exclusive use files */
	IXP_DMMOUNT = 0x10000000,	/* mode bit for mounted channel */
	IXP_DMAUTH = 0x08000000,	/* mode bit for authentication file */
	IXP_DMTMP = 0x04000000,		/* mode bit for non-backed-up file */
	IXP_DMREAD = 0x4<<6,		/* mode bit for read permission */
	IXP_DMWRITE = 0x2<<6,		/* mode bit for write permission */
	IXP_DMEXEC = 0x1<<6		/* mode bit for execute permission */
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

typedef struct Qid Qid;
struct Qid {
	unsigned char type;
	unsigned int version;
	unsigned long long path;
	/* internal use only */
	unsigned char dir_type;
};

/* stat structure */
typedef struct {
	unsigned short type;
	unsigned int dev;
	Qid qid;
	unsigned int mode;
	unsigned int atime;
	unsigned int mtime;
	unsigned long long length;
	char name[IXP_MAX_FLEN];
	char uid[IXP_MAX_ULEN];
	char gid[IXP_MAX_ULEN];
	char muid[IXP_MAX_ULEN];
} Stat;

typedef struct {
	unsigned char id;
	unsigned short tag;
	unsigned int fid;
	unsigned int maxmsg;			/* Tversion, Rversion */
	char version[IXP_MAX_VERSION];		/* Tversion, Rversion */
	unsigned short oldtag;			/* Tflush */
	char errstr[IXP_MAX_ERROR];		/* Rerror */
	Qid qid;				/* Rattach, Ropen, Rcreate */
	unsigned int iounit;			/* Ropen, Rcreate */
	Qid aqid;				/* Rauth */
	unsigned int afid;			/* Tauth, Tattach */
	char uname[IXP_MAX_ULEN];		/* Tauth, Tattach */
	char aname[IXP_MAX_FLEN];		/* Tauth, Tattach */
	unsigned int perm;			/* Tcreate */
	char name[IXP_MAX_FLEN];		/* Tcreate */
	unsigned char mode;			/* Tcreate, Topen */
	unsigned int newfid;			/* Twalk */
	unsigned short nwname;			/* Twalk */
	char wname[IXP_MAX_WELEM][IXP_MAX_FLEN];/* Twalk */
	unsigned short nwqid;			/* Rwalk */
	Qid wqid[IXP_MAX_WELEM];		/* Rwalk */
	unsigned long long offset;		/* Tread, Twrite */
	unsigned int count;			/* Tread, Twrite, Rread */
	Stat stat;				/* Rstat */
	unsigned short nstat;			/* Twstat, Rstat */
	unsigned char data[IXP_MAX_MSG];	/* Twrite, Rread, Twstat, Rstat */
} Fcall;

typedef struct IXPServer IXPServer;
typedef struct IXPConn IXPConn;
typedef struct IXPMap IXPMap;

struct IXPMap {
	unsigned int fid;
	unsigned short sel;
	unsigned short nwqid;
	Qid wqid[IXP_MAX_WELEM];
};


typedef struct {
	unsigned int size;
	IXPMap **data;
} MapVector;

struct IXPConn {
	int fd;
	IXPServer *srv;
	void (*read) (IXPConn *);
	void (*close) (IXPConn *);
	MapVector map;
	Fcall pending;
	int is_pending;
};

typedef struct {
	unsigned int size;
	IXPConn **data;
} ConnVector;

struct IXPServer {
	int running;
	ConnVector conn;
	int maxfd;
	fd_set rd;
};

typedef struct {
	int fd;
	unsigned int root_fid;
	Qid root_qid;
	Fcall fcall;
	char *errstr;
} IXPClient;

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
void *ixp_pack_u8(unsigned char *msg, unsigned char val);
void *ixp_unpack_u8(unsigned char *msg, unsigned char *val);
void *ixp_pack_u16(unsigned char *msg, unsigned short val);
void *ixp_unpack_u16(unsigned char *msg, unsigned short *val);
void *ixp_pack_u32(unsigned char *msg, unsigned int val);
void *ixp_unpack_u32(unsigned char *msg, unsigned int *val);
void *ixp_pack_u64(unsigned char *msg, unsigned long long val);
void *ixp_unpack_u64(unsigned char *msg, unsigned long long *val);
void *ixp_pack_string(unsigned char *msg, const char *s);
void *ixp_unpack_string(unsigned char *msg, char *string,
		unsigned short stringlen, unsigned short *len);
void *ixp_pack_data(unsigned char *msg, unsigned char *data,
		unsigned int datalen);
void *ixp_unpack_data(unsigned char *msg, unsigned char *data,
		unsigned int datalen);
void *ixp_pack_prefix(unsigned char *msg, unsigned int size,
		unsigned char id, unsigned short tag);
void *ixp_unpack_prefix(unsigned char *msg, unsigned int *size,
		unsigned char *id, unsigned short *tag);
void *ixp_pack_qid(unsigned char *msg, Qid *qid);
void *ixp_unpack_qid(unsigned char *msg, Qid *qid);
void *ixp_pack_stat(unsigned char *msg, Stat *stat);
void *ixp_unpack_stat(unsigned char *msg, Stat *stat);

/* message.c */
unsigned short ixp_sizeof_stat(Stat *stat);
unsigned int ixp_fcall2msg(void *msg, Fcall *fcall, unsigned int msglen);
unsigned int ixp_msg2fcall(Fcall *call, void *msg, unsigned int msglen);

/* server.c */
IXPConn *ixp_server_open_conn(IXPServer *s, int fd,
		void (*read)(IXPConn *c), void (*close)(IXPConn *c));
void ixp_server_close_conn(IXPConn *c);
char *ixp_server_loop(IXPServer *s);
IXPMap *ixp_server_fid2map(IXPConn *c, unsigned int fid);
unsigned int ixp_server_receive_fcall(IXPConn *c, Fcall *fcall);
int ixp_server_respond_fcall(IXPConn *c, Fcall *fcall);
int ixp_server_respond_error(IXPConn *c, Fcall *fcall, char *errstr);
void ixp_server_close(IXPServer *s);
Vector *ixp_vector_of_maps(MapVector *mv);

/* socket.c */
int ixp_connect_sock(char *address);
int ixp_accept_sock(int fd);
int ixp_create_sock(char *address, char **errstr);

/* transport.c */
unsigned int ixp_send_message(int fd, void *msg, unsigned int msize, char **errstr);
unsigned int ixp_recv_message(int fd, void *msg, unsigned int msglen, char **errstr);
