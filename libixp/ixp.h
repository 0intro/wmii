/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_CONN        32
#define MAX_OPEN_FILES  16
#define DEAD_SERVER  "server closed connection unexpectedly"
#define MAX_SEEN_SHUTDOWN   3

#define _MAX(x,y) ((x) > (y) ? (x) : (y))

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

typedef struct ResHeader ResHeader;
typedef struct ReqHeader ReqHeader;
typedef struct Connection Connection;
typedef struct File File;
typedef struct IXPClient IXPClient;
typedef struct IXPServer IXPServer;
typedef int     IXPRequest;
typedef int     IXPResponse;
typedef enum {
	HALT, SHUTDOWN, RUNNING
}               IXPRunlevel;

struct ReqHeader {
	IXPRequest      req;
	int             fd;
	size_t          offset;
	size_t          buf_len;
};

struct ResHeader {
	IXPResponse     res;
	int             fd;
	size_t          buf_len;
};

/** Definition of a connection to IXP */
struct Connection {
	IXPServer      *s;	/* !< server for this connection */
	int             index;	/* !< index inside server */
	int             fd;	/* !< file descriptor */
	int             mode;	/* 0 for reading, 1 for writing */
	int             header;
	void           *data;
	size_t          len;
	size_t          remain;
	void            (*read) (Connection *);
	void            (*write) (Connection *);
	File           *files[MAX_OPEN_FILES];
	int             seen[MAX_OPEN_FILES];
};

struct File {
	char           *name;
	void           *content;
	size_t          size;
	int             lock;
	int             bind;
	File           *parent;
	File           *next;
	/* introduced IXPServer in signature for IXPServer->errstr */
	void            (*after_write) (IXPServer * s, File *);
	void            (*before_read) (IXPServer * s, File *);
};

struct IXPClient {
	int             fd;
	fd_set          rd, wr;
	char           *errstr;	/* 0 if succes, CHECK AFTER EACH of following
				 * operations */
	/* returns fd if path exists */
	void            (*create) (IXPClient *, char *path);
	void            (*remove) (IXPClient *, char *path);
	int             (*open) (IXPClient *, char *path);
	void            (*close) (IXPClient *, int fd);
	                size_t(*read) (IXPClient *, int fd, void *out_buf,
				                       size_t out_buf_len);
	void            (*write) (IXPClient *, int fd, void *content, size_t in_len);
};

struct IXPServer {
	char           *sockfile;
	IXPRunlevel     runlevel;
	Connection      conn[MAX_CONN];
	fd_set          rd, wr;	/* socks to wakeup while select() */
	int             nfds;	/* number of file descriptors */
	File           *root;
	char           *errstr;	/* 0 if succes, CHECK AFTER EACH of following
				 * operations */
	File           *(*create) (IXPServer *, char *);
	File           *(*open) (IXPServer *, char *);
	                size_t(*read) (IXPServer *, int, size_t, void *, size_t);
	void            (*write) (IXPServer *, int, size_t, void *, size_t);
	void            (*close) (IXPServer *, int);
	void            (*remove) (IXPServer *, char *);
};

/* client.c, implements client stub functions for fs access */
IXPClient      *init_client(char *sockfile);
void            deinit_client(IXPClient * c);
size_t 
seek_read(IXPClient * c, int fd, size_t offset, void *out_buf,
	  size_t out_buf_len);
void 
seek_write(IXPClient * c, int fd, size_t offset,
	   void *content, size_t in_len);

/* message.c */
void           *tcreate_message(char *path, size_t * msg_len);
void           *topen_message(char *path, size_t * msg_len);
void           *
tread_message(int fd, size_t offset, size_t buf_len,
	      size_t * msg_len);
void           *
twrite_message(int fd, size_t offset, void *msg, size_t in_len,
	       size_t * msg_len);
void           *tclose_message(int fd, size_t * msg_len);
void           *tremove_message(char *path, size_t * msg_len);
void           *rcreate_message(size_t * msg_len);
void           *ropen_message(int fd, size_t * msg_len);
void           *rread_message(void *content, size_t content_len, size_t * msg_len);
void           *rwrite_message(size_t * msg_len);
void           *rclose_message(size_t * msg_len);
void           *rremove_message(size_t * msg_len);
void           *rerror_message(char *errstr, size_t * msg_len);

/* ramfs.c */
int             is_directory(File * f);
File           *ixp_walk(IXPServer * s, char *path);
File           *ixp_create(IXPServer * s, char *path);
File           *ixp_open(IXPServer * s, char *path);
size_t 
ixp_read(IXPServer * s, int fd, size_t offset, void *out_buf,
	 size_t out_buf_len);
void 
ixp_write(IXPServer * s, int fd, size_t offset,
	  void *content, size_t in_len);
void            ixp_close(IXPServer * s, int fd);
void            ixp_remove(IXPServer * s, char *path);
void            ixp_remove_file(IXPServer * s, File * f);

/* server.c, uses fs directly for manipulation */
IXPServer      *init_server(char *sockfile, void (*cleanup) (void));
void            deinit_server(IXPServer * s);
File           *fd_to_file(IXPServer * s, int fd);
void            run_server(IXPServer * s);
void            run_server_with_fd_support(IXPServer * s, int fd, void (*fd_read) (Connection *),	/* callback for read on
													 * fd */
			                   void (*fd_write) (Connection *));	/* callback for write on
										 * fd */
void            set_error(IXPServer * s, char *errstr);
