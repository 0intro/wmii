/*
 * (C)opyright MMIV-MMVI Anselm R. Garbe <garbeam at gmail dot com>
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

#include <ixp.h>
#include <cext.h>

static IXPClient c = { 0 };

static char version[] = "wmiir - " VERSION ", (C)opyright MMIV-MMVI Anselm R. Garbe\n";

static void
usage()
{
	fprintf(stderr, "%s", "usage: wmiir [-a <address>] [-v] create | read | remove | write <file>\n");
	exit(1);
}

static void
write_data(unsigned int fid)
{
	void *data = cext_emallocz(c.fcall.iounit);
	unsigned long long offset = 0;
	unsigned int len = 0;

	while((len = read(0, data, c.fcall.iounit)) > 0) {
		if(ixp_client_write(&c, fid, offset, len, data) != len) {
			fprintf(stderr, "wmiir: cannot write file: %s\n", c.errstr);
			break;
		}
		offset += len;
	}
	if(offset == 0) /* do an explicit empty write when no writing has been done yet */
		if(ixp_client_write(&c, fid, offset, 0, 0) != 0)
			fprintf(stderr, "wmiir: cannot write file: %s\n", c.errstr);
	free(data);
}

static int
xcreate(char *file)
{
	unsigned int fid;
	char *p = strrchr(file, '/');

	fid = c.root_fid << 2;
	/* walk to bottom-most directory */
	*p = 0;
	if(ixp_client_walk(&c, fid, file) == -1) {
		fprintf(stderr, "wmiir: cannot walk to '%s': %s\n", file, c.errstr);
		return -1;
	}
	p++;
	if(ixp_client_create(&c, fid, p, DMWRITE, IXP_OWRITE) == -1) {
		fprintf(stderr, "wmiir: cannot create file '%s': %s\n", p, c.errstr);
		return -1;
	}
	return ixp_client_close(&c, fid);
}

static int
xwrite(char *file, unsigned char mode)
{
	/* open */
	unsigned int fid = c.root_fid << 2;
	if(ixp_client_open(&c, fid, file, mode) == -1) {
		fprintf(stderr, "wmiir: cannot open file '%s': %s\n", file, c.errstr);
		return -1;
	}
	write_data(fid);
	return ixp_client_close(&c, fid);
}

static int
comp_stat(const void *s1, const void *s2)
{
	Stat *st1 = (Stat *)s1;
	Stat *st2 = (Stat *)s2;
	return strcmp(st1->name, st2->name);
}

static void
setrwx(long m, char *s)
{
	static char *modes[] =
	{
		"---",
		"--x",
		"-w-",
		"-wx",
		"r--",
		"r-x",
		"rw-",
		"rwx",
	};
	strncpy(s, modes[m], 3);
}

static char *
mode2str(unsigned int mode)
{
	static char buf[16];

	if(mode & DMDIR)
		buf[0]='d';
	else
		buf[0]='-';

	buf[1]='-';
	setrwx((mode >> 6) & 7, &buf[2]);
	setrwx((mode >> 3) & 7, &buf[5]);
	setrwx((mode >> 0) & 7, &buf[8]);
	buf[11] = 0;
	return buf;
}

static char *
time2str(unsigned int t)
{
	static char buf[32];
	cext_strlcpy(buf, ctime((time_t *)&t), sizeof(buf));
	buf[strlen(buf) - 1] = 0;
	return buf;
}

static void
xls(void *result, unsigned int msize)
{
	unsigned int n = 0, i = 0;
	void *p = result;
	Stat *dir;
	static Stat stat;

	do {
		p = ixp_dec_stat(p, &stat);
		n++;
	}
	while(p - result < msize);
	dir = (Stat *)cext_emallocz(sizeof(Stat) * n);
	p = result;
	do {
		p = ixp_dec_stat(p, &dir[i++]);
	}
	while(p - result < msize);
	qsort(dir, n, sizeof(Stat), comp_stat);
	for(i = 0; i < n; i++) {
		fprintf(stdout, "%s %s %s %5llu %s %s\n", mode2str(dir[i].mode),
				dir[i].uid, dir[i].gid, dir[i].length,
				time2str(dir[i].mtime), dir[i].name);
	}
	free(dir);
}

static int
xread(char *file)
{
	unsigned int fid = c.root_fid << 2;
	int count, is_directory = 0;
	static unsigned char result[IXP_MAX_MSG];
	void *dircontent = nil;
	unsigned int dircontentsz = 0;
	unsigned int ndircontent = 0;
	unsigned long long offset = 0;

	if(ixp_client_open(&c, fid, file, IXP_OREAD) == -1) {
		fprintf(stderr, "wmiir: cannot open file '%s': %s\n", file, c.errstr);
		return -1;
	}
	is_directory = !c.fcall.nwqid || (c.fcall.qid.type == IXP_QTDIR);

	while((count = ixp_client_read(&c, fid, offset, result, IXP_MAX_MSG)) > 0) {
		if(is_directory) {
			if(ndircontent + count > dircontentsz) {
				void *tmp = dircontent;
				if(!dircontentsz)
					dircontentsz = IXP_MAX_MSG;
				else
					dircontentsz *= 2;
				dircontent = cext_emallocz(dircontentsz);
				if(tmp) {
					memcpy(dircontent, tmp, ndircontent);
					free(tmp);
				}
			}
			memcpy(dircontent + ndircontent, result, count);
			ndircontent += count;
		}
		else {
			unsigned int i;
			for(i = 0; i < count; i++)
				fputc(result[i], stdout);
			fflush(stdout);
		}
		offset += count;
	}
	if(count == -1) {
		fprintf(stderr, "wmiir: cannot read file/directory '%s': %s\n", file, c.errstr);
		return -1;
	}
	if(is_directory && ndircontent)
		xls(dircontent, ndircontent);
	return ixp_client_close(&c, fid);
}

static int
xremove(char *file)
{
	unsigned int fid;

	fid = c.root_fid << 2;
	if(ixp_client_remove(&c, fid, file) == -1) {
		fprintf(stderr, "wmiir: cannot remove file '%s': %s\n", file, c.errstr);
		return -1;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	int ret = 0, i = 0;
	char *cmd, *file, *address = getenv("WMII_ADDRESS");

	/* command line args */
	if(argc < 2)
		usage();

	for(i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
		switch (argv[i][1]) {
			case 'v':
				fprintf(stdout, "%s", version);
				exit(0);
				break;
			case 'a':
				if(i + 1 < argc)
					address = argv[++i];
				else
					usage();
				break;
			default:
				usage();
				break;
		}
	}
	cmd = argv[argc - 2];
	file = argv[argc - 1];

	if(!address) {
		fprintf(stderr, "%s", "wmiir: error: $WMII_ADDRESS not set\n");
		usage();
	}

	if(ixp_client_dial(&c, address, getpid()) == -1) {
		fprintf(stderr, "wmiir: %s\n", c.errstr);
		exit(1);
	}

	if(!strncmp(cmd, "create", 7))
		ret = xcreate(file);
	else if(!strncmp(cmd, "read", 5))
		ret = xread(file);
	else if(!strncmp(cmd, "remove", 7))
		ret = xremove(file);
	else if(!strncmp(cmd, "write", 6))
		ret = xwrite(file, IXP_OWRITE);
	else
		usage();

	/* close socket */
	ixp_client_hangup(&c);

	return ret;
}
