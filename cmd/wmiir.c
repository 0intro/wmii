/* Copyright ©2007 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ixp.h>
#include <util.h>

static IxpClient *client;

static void
usage() {
	fprintf(stderr,
		   "usage: %1$s [-a <address>] {create | read | ls [-ld] | remove | write} <file>\n"
		   "       %1$s [-a <address>] xwrite <file> <data>\n"
		   "       %1$s -v\n", argv0);
	exit(1);
}

/* Utility Functions */
static void
write_data(IxpCFid *fid, char *name) {
	void *buf;
	uint len;

	buf = ixp_emalloc(fid->iounit);;
	while((len = read(0, buf, fid->iounit)) > 0)
		if(ixp_write(fid, buf, len) != len)
			fatal("cannot write file '%s': %s\n", name, errstr);
	/* do an explicit empty write when no writing has been done yet */
	if(fid->offset == 0)
		if(ixp_write(fid, buf, 0) != 0)
			fatal("cannot write file '%s': %s\n", name, errstr);
	free(buf);
}

static int
comp_stat(const void *s1, const void *s2) {
	Stat *st1, *st2;

	st1 = (Stat*)s1;
	st2 = (Stat*)s2;
	return strcmp(st1->name, st2->name);
}

static void
setrwx(long m, char *s) {
	static char *modes[] = {
		"---", "--x", "-w-",
		"-wx", "r--", "r-x",
		"rw-", "rwx",
	};
	strncpy(s, modes[m], 3);
}

static char *
str_of_mode(uint mode) {
	static char buf[16];

	buf[0]='-';
	if(mode & P9_DMDIR)
		buf[0]='d';
	buf[1]='-';
	setrwx((mode >> 6) & 7, &buf[2]);
	setrwx((mode >> 3) & 7, &buf[5]);
	setrwx((mode >> 0) & 7, &buf[8]);
	buf[11] = 0;
	return buf;
}

static char *
str_of_time(uint val) {
	static char buf[32];

	ctime_r((time_t*)&val, buf);
	buf[strlen(buf) - 1] = '\0';
	return buf;
}

static void
print_stat(Stat *s, int details) {
	if(details)
		fprintf(stdout, "%s %s %s %5llu %s %s\n", str_of_mode(s->mode),
				s->uid, s->gid, s->length, str_of_time(s->mtime), s->name);
	else {
		if((s->mode&P9_DMDIR) && strcmp(s->name, "/"))
			fprintf(stdout, "%s/\n", s->name);
		else
			fprintf(stdout, "%s\n", s->name);
	}
}

/* Service Functions */
static int
xwrite(int argc, char *argv[]) {
	IxpCFid *fid;
	char *file;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	fid = ixp_open(client, file, P9_OWRITE);
	if(fid == nil)
		fatal("Can't open file '%s': %s\n", file, errstr);

	write_data(fid, file);
	return 0;
}

static int
xawrite(int argc, char *argv[]) {
	IxpCFid *fid;
	char *file, *buf, *arg;
	int nbuf, mbuf, len;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	fid = ixp_open(client, file, P9_OWRITE);
	if(fid == nil)
		fatal("Can't open file '%s': %s\n", file, errstr);

	nbuf = 0;
	mbuf = 128;
	buf = ixp_emalloc(mbuf);
	while(argc) {
		arg = ARGF();
		len = strlen(arg);
		if(nbuf + len > mbuf) {
			mbuf <<= 1;
			buf = ixp_erealloc(buf, mbuf);
		}
		memcpy(buf+nbuf, arg, len);
		nbuf += len;
		if(argc)
			buf[nbuf++] = ' ';
	}

	if(ixp_write(fid, buf, nbuf) == -1)
		fatal("cannot write file '%s': %s\n", file, errstr);
	return 0;
}

static int
xcreate(int argc, char *argv[]) {
	IxpCFid *fid;
	char *file;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	fid = ixp_create(client, file, 0777, P9_OWRITE);
	if(fid == nil)
		fatal("Can't create file '%s': %s\n", file, errstr);

	if((fid->qid.type&P9_DMDIR) == 0)
		write_data(fid, file);

	return 0;
}

static int
xremove(int argc, char *argv[]) {
	char *file;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	if(ixp_remove(client, file) == 0)
		fatal("Can't remove file '%s': %s\n", file, errstr);
	return 0;
}

static int
xread(int argc, char *argv[]) {
	IxpCFid *fid;
	char *file, *buf;
	int count;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	fid = ixp_open(client, file, P9_OREAD);
	if(fid == nil)
		fatal("Can't open file '%s': %s\n", file, errstr);

	buf = ixp_emalloc(fid->iounit);
	while((count = ixp_read(fid, buf, fid->iounit)) > 0)
		write(1, buf, count);

	if(count == -1)
		fatal("cannot read file/directory '%s': %s\n", file, errstr);

	return 0;
}

static int
xls(int argc, char *argv[]) {
	Message m;
	Stat *stat;
	IxpCFid *fid;
	char *file, *buf;
	int lflag, dflag, count, nstat, mstat, i;

	lflag = dflag = 0;

	ARGBEGIN{
	case 'l':
		lflag++;
		break;
	case 'd':
		dflag++;
		break;
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());

	stat = ixp_stat(client, file);
	if(stat == nil)
		fatal("cannot stat file '%s': %s\n", file, errstr);

	if(dflag || (stat->mode&P9_DMDIR) == 0) {
		print_stat(stat, lflag);
		ixp_freestat(stat);
		return 0;
	}
	ixp_freestat(stat);

	fid = ixp_open(client, file, P9_OREAD);
	if(fid == nil)
		fatal("Can't open file '%s': %s\n", file, errstr);

	nstat = 0;
	mstat = 16;
	stat = ixp_emalloc(sizeof(*stat) * mstat);
	buf = ixp_emalloc(fid->iounit);
	while((count = ixp_read(fid, buf, fid->iounit)) > 0) {
		m = ixp_message(buf, count, MsgUnpack);
		while(m.pos < m.end) {
			if(nstat == mstat) {
				mstat <<= 1;
				stat = ixp_erealloc(stat, mstat);
			}
			ixp_pstat(&m, &stat[nstat++]);
		}
	}

	qsort(stat, nstat, sizeof(*stat), comp_stat);
	for(i = 0; i < nstat; i++) {
		print_stat(&stat[i], lflag);
		ixp_freestat(&stat[i]);
	}
	free(stat);

	if(count == -1)
		fatal("cannot read directory '%s': %s\n", file, errstr);
	return 0;
}

int
main(int argc, char *argv[]) {
	int ret;
	char *cmd, *address;

	address = getenv("WMII_ADDRESS");

	ARGBEGIN{
	case 'v':
		printf("%s-" VERSION ", ©2007 Kris Maglione\n", argv0);
		exit(0);
	case 'a':
		address = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;

	cmd = EARGF(usage());

	if(!address)
		fatal("$WMII_ADDRESS not set\n");

	client = ixp_mount(address);
	if(client == nil)
		fatal("%s\n", errstr);

	if(!strcmp(cmd, "create"))
		ret = xcreate(argc, argv);
	else if(!strcmp(cmd, "ls"))
		ret = xls(argc, argv);
	else if(!strcmp(cmd, "read"))
		ret = xread(argc, argv);
	else if(!strcmp(cmd, "remove"))
		ret = xremove(argc, argv);
	else if(!strcmp(cmd, "write"))
		ret = xwrite(argc, argv);
	else if(!strcmp(cmd, "xwrite"))
		ret = xawrite(argc, argv);
	else
		usage();

	ixp_unmount(client);
	return ret;
}
