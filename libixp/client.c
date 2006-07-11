/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "ixp.h"

int
ixp_client_do_fcall(IXPClient *c)
{
	static unsigned char msg[IXP_MAX_MSG];
	unsigned int msize = ixp_fcall2msg(msg, &c->ifcall, IXP_MAX_MSG);

	c->errstr = 0;
	if(ixp_send_message(c->fd, msg, msize, &c->errstr) != msize)
		return -1;
	if(!ixp_recv_message(c->fd, msg, IXP_MAX_MSG, &c->errstr))
		return -1;
	if(!(msize = ixp_msg2fcall(&c->ofcall, msg, IXP_MAX_MSG))) {
		c->errstr = "received bad message";
		return -1;
	}
	if(c->ofcall.type == RERROR) {
		c->errstr = c->ofcall.ename;
		return -1;
	}

	return 0;
}

int
ixp_client_dial(IXPClient *c, char *sockfile, unsigned int rootfid)
{

	if((c->fd = ixp_connect_sock(sockfile)) < 0) {
		c->errstr = "cannot connect server";
		return -1;
	}

	c->ifcall.type = TVERSION;
	c->ifcall.tag = IXP_NOTAG;
	c->ifcall.msize = IXP_MAX_MSG;
	c->ifcall.version = IXP_VERSION;
	if(ixp_client_do_fcall(c) == -1) {
		fprintf(stderr, "error: %s\n", c->errstr);
		ixp_client_hangup(c);
		return -1;
	}
	if(strncmp(c->ofcall.version, IXP_VERSION, strlen(IXP_VERSION))) {
		fprintf(stderr, "error: %s\n", c->errstr);
		c->errstr = "9P versions differ";
		ixp_client_hangup(c);
		return -1;	/* we cannot handle this version */
	}
	free(c->ofcall.version);
	c->root_fid = rootfid;

	c->ifcall.type = TATTACH;
	c->ifcall.tag = IXP_NOTAG;
	c->ifcall.fid = c->root_fid;
	c->ifcall.afid = IXP_NOFID;
	c->ifcall.uname = getenv("USER");
	c->ifcall.aname = "";
	if(ixp_client_do_fcall(c) == -1) {
		fprintf(stderr, "error: %s\n", c->errstr);
		ixp_client_hangup(c);
		return -1;
	}
	c->root_qid = c->ofcall.qid;

	return 0;
}

int
ixp_client_remove(IXPClient *c, unsigned int newfid, char *filepath)
{

	if(ixp_client_walk(c, newfid, filepath) == -1)
		return -1;
	c->ifcall.type = TREMOVE;
	c->ifcall.tag = IXP_NOTAG;
	c->ifcall.fid = newfid;

	return ixp_client_do_fcall(c);
}

int
ixp_client_create(IXPClient *c, unsigned int dirfid, char *name,
		unsigned int perm, unsigned char mode)
{
	c->ifcall.type = TCREATE;
	c->ifcall.tag = IXP_NOTAG;
	c->ifcall.fid = dirfid;
	c->ifcall.name = name;
	c->ifcall.perm = perm;
	c->ifcall.mode = mode;
	return ixp_client_do_fcall(c);
}

int
ixp_client_walk(IXPClient *c, unsigned int newfid, char *filepath)
{
	unsigned int i;
	char *wname[IXP_MAX_WELEM];

	c->ifcall.type = TWALK;
	c->ifcall.fid = c->root_fid;
	c->ifcall.newfid = newfid;
	if(filepath) {
		c->ifcall.name = filepath;
		c->ifcall.nwname =
			cext_tokenize(wname, IXP_MAX_WELEM, c->ifcall.name, '/');
		for(i = 0; i < c->ifcall.nwname; i++)
			c->ifcall.wname[i] = wname[i];
	}
	return ixp_client_do_fcall(c);
}

int
ixp_client_stat(IXPClient *c, unsigned int newfid, char *filepath)
{

	if(ixp_client_walk(c, newfid, filepath) == -1)
		return -1;

	c->ifcall.type = TSTAT;
	c->ifcall.tag = IXP_NOTAG;
	c->ifcall.fid = newfid;
	return ixp_client_do_fcall(c);
}

int
ixp_client_open(IXPClient *c, unsigned int newfid, unsigned char mode)
{

	c->ifcall.type = TOPEN;
	c->ifcall.tag = IXP_NOTAG;
	c->ifcall.fid = newfid;
	c->ifcall.mode = mode;
	return ixp_client_do_fcall(c);
}

int
ixp_client_walkopen(IXPClient *c, unsigned int newfid, char *filepath,
		unsigned char mode)
{

	if(ixp_client_walk(c, newfid, filepath) == -1)
		return -1;
	return ixp_client_open(c, newfid, mode);
}

int
ixp_client_read(IXPClient *c, unsigned int fid, unsigned long long offset,
		void *result, unsigned int res_len)
{
	unsigned int bytes = c->ofcall.iounit;

	c->ifcall.type = TREAD;
	c->ifcall.tag = IXP_NOTAG;
	c->ifcall.fid = fid;
	c->ifcall.offset = offset;
	c->ifcall.count = res_len < bytes ? res_len : bytes;
	if(ixp_client_do_fcall(c) == -1)
		return -1;
	memcpy(result, c->ofcall.data, c->ofcall.count);
	free(c->ofcall.data);

	return c->ofcall.count;
}

int
ixp_client_write(IXPClient *c, unsigned int fid,
		unsigned long long offset, unsigned int count,
		unsigned char *data)
{

	if(count > c->ofcall.iounit) {
		c->errstr = "iounit exceeded";
		return -1;
	}

	c->ifcall.type = TWRITE;
	c->ifcall.tag = IXP_NOTAG;
	c->ifcall.fid = fid;
	c->ifcall.offset = offset;
	c->ifcall.count = count;
	c->ifcall.data = (void *)data;
	if(ixp_client_do_fcall(c) == -1)
		return -1;

	return c->ofcall.count;
}

int
ixp_client_close(IXPClient *c, unsigned int fid)
{

	c->ifcall.type = TCLUNK;
	c->ifcall.tag = IXP_NOTAG;
	c->ifcall.fid = fid;
	return ixp_client_do_fcall(c);
}

void
ixp_client_hangup(IXPClient *c)
{
	/* session finished, now shutdown */
	if(c->fd) {
		shutdown(c->fd, SHUT_RDWR);
		close(c->fd);
	}
}
