/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ixp.h"

unsigned int
ixp_send_message(int fd, void *msg, unsigned int msize, char **errstr)
{
	unsigned int num = 0;
	int r;

	/* send message */
	while(num < msize) {
		r = write(fd, msg + num, msize - num);
		if(r == -1 && errno == EINTR)
			continue;
		if(r < 1) {
			*errstr = "broken pipe";
			return 0;
		}
		num += r;
	}
	return num;
}

static unsigned int
ixp_recv_data(int fd, void *msg, unsigned int msize, char **errstr)
{
	unsigned int num = 0;
	int r = 0;

	/* receive data */
	while(num < msize) {
		r = read(fd, msg + num, msize - num);
		if(r == -1 && errno == EINTR)
			continue;
		if(r < 1) {
			*errstr = "broken pipe";
			return 0;
		}
		num += r;
	}
	return num;
}

unsigned int
ixp_recv_message(int fd, void *msg, unsigned int msglen, char **errstr)
{
	unsigned int msize;

	/* receive header */
	if(ixp_recv_data(fd, msg, sizeof(unsigned int), errstr) !=
			sizeof(unsigned int))
		return 0;
	ixp_unpack_u32((void *)&msg, nil, &msize);
	if(msize > msglen) {
		*errstr = "invalid message header";
		return 0;
	}
	/* receive message */
	if(ixp_recv_data(fd, msg, msize - sizeof(unsigned int), errstr)
       != msize - sizeof(unsigned int))
		return 0;
	return msize;
}
