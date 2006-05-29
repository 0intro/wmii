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
#include <netinet/in.h>
#include <netdb.h>
#include <sys/un.h>
#include <unistd.h>

#include "ixp.h"

static int
connect_unix_sock(char *address)
{
	int fd = 0;
	struct sockaddr_un addr = { 0 };
	socklen_t su_len;

	/* init */
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, address, sizeof(addr.sun_path));
	su_len = sizeof(struct sockaddr) + strlen(addr.sun_path);

	if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -1;
	if(connect(fd, (struct sockaddr *) &addr, su_len)) {
		close(fd);
		return -1;
	}
	return fd;
}

static int
connect_inet_sock(char *host)
{
	int fd = 0;
	struct sockaddr_in addr = { 0 };
	struct hostent *hp;
	char *port = strrchr(host, '!');
	unsigned int prt;

	if(!port)
		return -1;
	*port = 0;
	port++;
	if(sscanf(port, "%d", &prt) != 1)
		return -1;

	/* init */
	if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;
	hp = gethostbyname(host);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(prt);
	bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);

	if(connect(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in))) {
		close(fd);
		return -1;
	}
	return fd;
}

int
ixp_connect_sock(char *address)
{
	char *p = strchr(address, '!');
	char *addr, *type;

	if(!p)
		return -1;
	*p = 0;

	addr = &p[1];
	type = address; /* unix, inet */

	if(!strncmp(type, "unix", 5))
		return connect_unix_sock(addr);
	else if(!strncmp(type, "tcp", 4))
		return connect_inet_sock(addr);
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
create_inet_sock(char *host, char **errstr)
{
	int fd;
	struct sockaddr_in addr = { 0 };
	struct hostent *hp;
	char *port = strrchr(host, '!');
	unsigned int prt;

	if(!port) {
		*errstr = "no port provided in address";
		return -1;
	}
	*port = 0;
	port++;
	if(sscanf(port, "%d", &prt) != 1) {
		*errstr = "invalid port number";
		return -1;
	}
	signal(SIGPIPE, SIG_IGN);
	if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		*errstr = "cannot open socket";
		return -1;
	}
	addr.sin_family = AF_INET;
	addr.sin_port = htons(prt);

	if(!strncmp(host, "*", 1))
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	else if((hp = gethostbyname(host)))
		bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
	else {
		*errstr = "cannot translate hostname to an address";
		return -1;
	}

	if(bind(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) < 0) {
		*errstr = "cannot bind socket";
		close(fd);
		return -1;
	}

	if(listen(fd, IXP_MAX_CACHE) < 0) {
		*errstr = "cannot listen on socket";
		close(fd);
		return -1;
	}
	return fd;
}

static int
create_unix_sock(char *file, char **errstr)
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
	strncpy(addr.sun_path, file, sizeof(addr.sun_path));
	su_len = sizeof(struct sockaddr) + strlen(addr.sun_path);

	unlink(file); /* remove old socket, if any */
	if(bind(fd, (struct sockaddr *) &addr, su_len) < 0) {
		*errstr = "cannot bind socket";
		close(fd);
		return -1;
	}
	chmod(file, S_IRWXU);

	if(listen(fd, IXP_MAX_CACHE) < 0) {
		*errstr = "cannot listen on socket";
		close(fd);
		return -1;
	}
	return fd;
}

int
ixp_create_sock(char *address, char **errstr)
{
	char *p = strchr(address, '!');
	char *addr, *type;

	if(!p) {
		*errstr = "no socket type defined";
		return -1;
	}
	*p = 0;

	addr = &p[1];
	type = address; /* unix, inet */

	if(!strncmp(type, "unix", 5))
		return create_unix_sock(addr, errstr);
	else if(!strncmp(type, "tcp", 4))
		return create_inet_sock(addr, errstr);
	else
		*errstr = "unkown socket type";
	return -1;
}
