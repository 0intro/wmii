/*
 * (C)opyright MMIV-MMV Anselm R. Garbe <garbeam at gmail dot com>
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

#include <cext.h>

u32 
ixp_send_message(int fd, void *msg, u32 msize, char **errstr)
{
	u32             num = 0;
	int             r;

	/* send message */
	while (num < msize) {
		r = write(fd, msg + num, msize - num);
		if (r == -1 && errno == EINTR)
			continue;
		if (r < 1) {
			*errstr = "cannot send message";
			return 0;
		}
		num += r;
	}
	return num;
}

static u32 
ixp_recv_data(int fd, void *msg, u32 msize, char **errstr)
{
	u32             num = 0;
	int             r = 0;

	/* receive data */
	while (num < msize) {
		r = read(fd, msg + num, msize - num);
		if (r == -1 && errno == EINTR)
			continue;
		if (r < 1) {
			*errstr = "cannot receive data";
			return 0;
		}
		num += r;
	}
	return num;
}

u32 
ixp_recv_message(int fd, void *msg, u32 msglen, char **errstr)
{
	u32             msize;

	/* receive header */
	if (ixp_recv_data(fd, msg, sizeof(u32), errstr) != sizeof(u32))
		return 0;
	ixp_dec_u32(msg, &msize);
	if (msize > msglen) {
		*errstr = "message size exceeds buffer size";
		return 0;
	}
	/* receive message */
	if (ixp_recv_data(fd, msg + sizeof(u32), msize - sizeof(u32), errstr)
	    != msize - sizeof(u32))
		return 0;
	return msize;
}
