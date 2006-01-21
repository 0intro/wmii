/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "cext.h"
#include "ixp.h"

static int
connect_unix_sock(char *sockfile)
{
    int fd = 0;
    struct sockaddr_un addr = { 0 };
    socklen_t su_len;

    /* init */
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockfile, sizeof(addr.sun_path));
    su_len = sizeof(struct sockaddr) + strlen(addr.sun_path);

    if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        return -1;
    if(connect(fd, (struct sockaddr *) &addr, su_len)) {
        close(fd);
        return -1;
    }
    return fd;
}

int
ixp_connect_sock(char *sockfile)
{
	char *p = strchr(sockfile, '!');
	char *file, *type;

	if(!p)
		return -1;
	*p = 0;
	file = &p[1];
	type = sockfile; /* unix, tcp */

	if(strncmp(type, "unix", 5))
		return connect_unix_sock(file);
    return -1;
}

int
ixp_accept_sock(int fd)
{
    socklen_t su_len;
    struct sockaddr_un addr = { 0 };

    su_len = sizeof(struct sockaddr);
    return accept(fd, (struct sockaddr *) &addr, &su_len);
}

static int
create_unix_sock(char *sockfile, char **errstr)
{
    int fd;
    int yes = 1;
    struct sockaddr_un addr = { 0 };
    socklen_t su_len;

    signal(SIGPIPE, SIG_IGN);
    if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        *errstr = "cannot open socket";
        return -1;
    }
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                  (char *) &yes, sizeof(yes)) < 0) {
        *errstr = "cannot set socket options";
        close(fd);
        return -1;
    }
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockfile, sizeof(addr.sun_path));
    su_len = sizeof(struct sockaddr) + strlen(addr.sun_path);

    if(bind(fd, (struct sockaddr *) &addr, su_len) < 0) {
        *errstr = "cannot bind socket";
        close(fd);
        return -1;
    }
    chmod(sockfile, S_IRWXU);

    if(listen(fd, IXP_MAX_CONN) < 0) {
        *errstr = "cannot listen on socket";
        close(fd);
        return -1;
    }
    return fd;
}

int
ixp_create_sock(char *sockfile, char **errstr)
{
	char *p = strchr(sockfile, '!');
	char *file, *type;

	if(!p) {
		*errstr = "no socket type defined";
		return -1;
	}
	*p = 0;
	file = &p[1];
	type = sockfile; /* unix, tcp */

	if(!strncmp(type, "unix", 5))
		return create_unix_sock(file, errstr);
    return -1;
}
