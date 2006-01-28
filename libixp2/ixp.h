/*
 *(C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 *See LICENSE file for license details.
 */

#include <sys/types.h>

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
#define IXP_MAX_WELEM 	16      /*MAXWELEM */
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

/*9P message types */
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

/*borrowed from libc.h of Plan 9 */
#define DMDIR		0x80000000	/*mode bit for directories */
#define DMAPPEND	0x40000000	/*mode bit for append only files */
#define DMEXCL		0x20000000	/*mode bit for exclusive use files */
#define DMMOUNT		0x10000000	/*mode bit for mounted channel */
#define DMAUTH		0x08000000	/*mode bit for authentication file */
#define DMTMP		0x04000000	/*mode bit for non-backed-up file */

#define DMREAD		0x4<<6		/*mode bit for read permission */
#define DMWRITE		0x2<<6		/*mode bit for write permission */
#define DMEXEC		0x1<<6		/*mode bit for execute permission */

/*modes */
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

/*qid.types */
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

#define IXP_NOTAG    (unsigned short)~0U        /*Dummy tag */
#define IXP_NOFID    (unsigned int)~0   /*No auth */

typedef struct {
    unsigned char type;
    unsigned int version;
    unsigned long long path;
} Qid;

/*stat structure */
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
    unsigned int maxmsg;        /*Tversion, Rversion */
    char version[IXP_MAX_VERSION];      /*Tversion, Rversion */
    unsigned short oldtag;      /*Tflush */
    char errstr[IXP_MAX_ERROR]; /*Rerror */
    Qid qid;                    /*Rattach, Ropen, Rcreate */
    unsigned int iounit;        /*Ropen, Rcreate */
    Qid aqid;                   /*Rauth */
    unsigned int afid;          /*Tauth, Tattach */
    char uname[IXP_MAX_ULEN];   /*Tauth, Tattach */
    char aname[IXP_MAX_FLEN];   /*Tauth, Tattach */
    unsigned int perm;          /*Tcreate */
    char name[IXP_MAX_FLEN];    /*Tcreate */
    unsigned char mode;         /*Tcreate, Topen */
    unsigned int newfid;        /*Twalk */
    unsigned short nwname;      /*Twalk */
    char wname[IXP_MAX_WELEM][IXP_MAX_FLEN]; /*Twalk */
    unsigned short nwqid;       /*Rwalk */
    Qid wqid[IXP_MAX_WELEM];    /*Rwalk */
    unsigned long long offset;  /*Tread, Twrite */
    unsigned int count;         /*Tread, Twrite, Rread */
    Stat stat;                  /*Rstat */
    unsigned short nstat;       /*Twstat, Rstat */
    unsigned char data[IXP_MAX_MSG];    /*Twrite, Rread, Twstat,
                                         *Rstat */
} Fcall;

typedef struct IXPServer IXPServer;
typedef struct IXPConn IXPConn;

struct IXPConn {
    int fd;
    void (*read) (IXPServer *, IXPConn *);
    void (*close) (IXPServer *, IXPConn *);
    void *aux;
};

struct IXPServer {
    int running;
    IXPConn conn[IXP_MAX_CONN];
    int maxfd;
    fd_set rd;
};

typedef struct IXPMap IXPMap;
struct IXPMap {
	unsigned int fid;
	Qid qid; 
};

typedef struct {
	IXPMap **map;
	size_t mapsz;
	Fcall *fcall;
	Fcall **async;
	size_t asyncsz;
} IXPReq;


typedef struct {
    int fd;
    unsigned int root_fid;
    Qid root_qid;
    Fcall fcall;
    char *errstr;
} IXPClient;

/* client.c */
int ixp_client_init(IXPClient *c, char *address);
void ixp_client_deinit(IXPClient *c);
int ixp_client_remove(IXPClient *c, unsigned int newfid, char *filepath);
int ixp_client_create(IXPClient *c, unsigned int dirfid, char *name,
                      unsigned int perm, unsigned char mode);
int ixp_client_walk(IXPClient *c, unsigned int newfid, char *filepath);
int ixp_client_open(IXPClient *c, unsigned int newfid, char *filepath,
                    unsigned char mode);
int ixp_client_read(IXPClient *c, unsigned int fid,
                    unsigned long long offset, void *result,
                    unsigned int res_len);
int ixp_client_write(IXPClient *c, unsigned int fid,
                     unsigned long long offset,
                     unsigned int count, unsigned char *data);
int ixp_client_close(IXPClient *c, unsigned int fid);

/* convert.c */
void *ixp_enc_u8(unsigned char *msg, unsigned char val);
void *ixp_dec_u8(unsigned char *msg, unsigned char *val);
void *ixp_enc_u16(unsigned char *msg, unsigned short val);
void *ixp_dec_u16(unsigned char *msg, unsigned short *val);
void *ixp_enc_u32(unsigned char *msg, unsigned int val);
void *ixp_dec_u32(unsigned char *msg, unsigned int *val);
void *ixp_enc_u64(unsigned char *msg, unsigned long long val);
void *ixp_dec_u64(unsigned char *msg, unsigned long long *val);
void *ixp_enc_string(unsigned char *msg, const char *s);
void *ixp_dec_string(unsigned char *msg, char *string,
                     unsigned short stringlen, unsigned short *len);
void *ixp_enc_data(unsigned char *msg, unsigned char *data,
                   unsigned int datalen);
void *ixp_dec_data(unsigned char *msg, unsigned char *data,
                   unsigned int datalen);
void *ixp_enc_prefix(unsigned char *msg, unsigned int size,
                     unsigned char id, unsigned short tag);
void *ixp_dec_prefix(unsigned char *msg, unsigned int *size,
                     unsigned char *id, unsigned short *tag);
void *ixp_enc_qid(unsigned char *msg, Qid *qid);
void *ixp_dec_qid(unsigned char *msg, Qid *qid);
void *ixp_enc_stat(unsigned char *msg, Stat *stat);
void *ixp_dec_stat(unsigned char *msg, Stat *stat);

/* message.c */
unsigned short ixp_sizeof_stat(Stat *stat);
unsigned int ixp_fcall_to_msg(Fcall *fcall, void *msg,
                              unsigned int msglen);
unsigned int ixp_msg_to_fcall(void *msg, unsigned int msglen,
                              Fcall *fcall);

/* server.c */
IXPConn *ixp_server_alloc_conn(IXPServer *s);
void ixp_server_free_conn(IXPServer *s, IXPConn *c);
char *ixp_server_loop(IXPServer *s);
void ixp_server_init(IXPServer *s);
void ixp_server_deinit(IXPServer *s);
Fcall ** ixp_server_attach_fcall(Fcall *f, Fcall **array, size_t *size);
void ixp_server_detach_fcall(Fcall *f, Fcall **array);
IXPMap ** ixp_server_attach_map(IXPMap *m, IXPMap **array, size_t *size);
void ixp_server_detach_map(IXPMap *m, IXPMap **array);
IXPMap * ixp_server_fid2map(IXPReq *r, unsigned int fid);
void ixp_server_close_conn(IXPServer *s, IXPConn *c);

/* socket.c */
int ixp_connect_sock(char *address);
int ixp_accept_sock(int fd);
int ixp_create_sock(char *address, char **errstr);

/* transport.c */
unsigned int ixp_send_message(int fd, void *msg, unsigned int msize,
                              char **errstr);
unsigned int ixp_recv_message(int fd, void *msg, unsigned int msglen,
                              char **errstr);
