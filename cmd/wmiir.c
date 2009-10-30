/* Copyight ©2007-2009 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */
#define IXP_NO_P9_
#define IXP_P9_STRUCTS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ixp.h>
#include <util.h>
#include <fmt.h>

static IxpClient *client;

static void
usage(void) {
	fprint(1,
	       "usage: %s [-a <address>] {create | ls [-dlp] | read | remove | write} <file>\n"
	       "       %s [-a <address>] xwrite <file> <data>\n"
	       "       %s -v\n", argv0, argv0, argv0);
	exit(1);
}

static int
errfmt(Fmt *f) {
	return fmtstrcpy(f, ixp_errbuf());
}

/* Utility Functions */
static void
write_data(IxpCFid *fid, char *name) {
	void *buf;
	int len;

	buf = emalloc(fid->iounit);;
	for(;;) {
		len = read(0, buf, fid->iounit);
		if(len <= 0)
			break;
		if(ixp_write(fid, buf, len) != len)
			fatal("cannot write file %q\n", name);
	}
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
modestr(uint mode) {
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

static char*
timestr(time_t val) {
	static char buf[32];

	strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", localtime(&val));
	return buf;
}

static void
print_stat(Stat *s, int lflag, char *file, int pflag) {
	char *slash;

	slash = "";
	if(pflag)
		slash = "/";
	else
		file = "";

	if(lflag)
		print("%s %s %s %5llud %s %s%s%s\n",
				modestr(s->mode), s->uid, s->gid, s->length,
				timestr(s->mtime), file, slash, s->name);
	else {
		if((s->mode&P9_DMDIR) && strcmp(s->name, "/"))
			print("%s%s%s/\n", file, slash, s->name);
		else
			print("%s%s%s\n", file, slash, s->name);
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
		fatal("Can't open file '%s': %r\n", file);

	write_data(fid, file);
	ixp_close(fid);
	return 0;
}

static int
xawrite(int argc, char *argv[]) {
	IxpCFid *fid;
	char *file, *buf;
	int nbuf, i;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	file = EARGF(usage());
	fid = ixp_open(client, file, P9_OWRITE);
	if(fid == nil)
		fatal("Can't open file '%s': %r\n", file);

	nbuf = 1;
	for(i=0; i < argc; i++)
		nbuf += strlen(argv[i]) + (i > 0);
	buf = emalloc(nbuf);
	buf[0] = '\0';
	while(argc) {
		strcat(buf, ARGF());
		if(argc)
			strcat(buf, " ");
	}

	if(ixp_write(fid, buf, nbuf) == -1)
		fatal("cannot write file '%s': %r\n", file);
	ixp_close(fid);
	free(buf);
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
		fatal("Can't create file '%s': %r\n", file);

	if((fid->qid.type&P9_DMDIR) == 0)
		write_data(fid, file);
	ixp_close(fid);
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
	do {
		if(!ixp_remove(client, file))
			fprint(2, "%s: Can't remove file '%s': %r\n", argv0, file);
	}while((file = ARGF()));
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

	if(argc == 0)
		usage();
	file = EARGF(usage());
	do {
		fid = ixp_open(client, file, P9_OREAD);
		if(fid == nil)
			fatal("Can't open file '%s': %r\n", file);

		buf = emalloc(fid->iounit);
		while((count = ixp_read(fid, buf, fid->iounit)) > 0)
			write(1, buf, count);
		ixp_close(fid);

		if(count == -1)
			fprint(2, "%s: cannot read file '%s': %r\n", argv0, file);
	} while((file = ARGF()));

	return 0;
}

static int
xls(int argc, char *argv[]) {
	IxpMsg m;
	Stat *stat;
	IxpCFid *fid;
	char *file;
	char *buf;
	int lflag, dflag, pflag;
	int count, nstat, mstat, i;

	lflag = dflag = pflag = 0;

	ARGBEGIN{
	case 'l':
		lflag++;
		break;
	case 'd':
		dflag++;
		break;
	case 'p':
		pflag++;
		break;
	default:
		usage();
	}ARGEND;

	count = 0;
	file = EARGF(usage());
	do {
		stat = ixp_stat(client, file);
		if(stat == nil)
			fatal("cannot stat file '%s': %r\n", file);

		i = strlen(file);
		if(file[i-1] == '/') {
			file[i-1] = '\0';
			if(!(stat->mode&P9_DMDIR))
				fatal("%s: not a directory", file);
		}
		if(dflag || (stat->mode&P9_DMDIR) == 0) {
			print_stat(stat, lflag, file, pflag);
			ixp_freestat(stat);
			continue;
		}
		ixp_freestat(stat);

		fid = ixp_open(client, file, P9_OREAD);
		if(fid == nil)
			fatal("Can't open file '%s': %r\n", file);

		nstat = 0;
		mstat = 16;
		stat = emalloc(mstat * sizeof *stat);
		buf = emalloc(fid->iounit);
		while((count = ixp_read(fid, buf, fid->iounit)) > 0) {
			m = ixp_message(buf, count, MsgUnpack);
			while(m.pos < m.end) {
				if(nstat == mstat) {
					mstat <<= 1;
					stat = erealloc(stat, mstat * sizeof *stat);
				}
				ixp_pstat(&m, &stat[nstat++]);
			}
		}
		ixp_close(fid);

		qsort(stat, nstat, sizeof *stat, comp_stat);
		for(i = 0; i < nstat; i++) {
			print_stat(&stat[i], lflag, file, pflag);
			ixp_freestat(&stat[i]);
		}
		free(stat);
	} while((file = ARGF()));

	if(count == -1)
		fatal("cannot read directory '%s': %r\n", file);
	return 0;
}

static int
xnamespace(int argc, char *argv[]) {
	char *path;

	ARGBEGIN{
	default:
		usage();
	}ARGEND;

	path = ixp_namespace();
	if(path == nil)
		fatal("can't find namespace: %r\n");
	print("%s\n", path);
	return 0;
}

static int
xsetsid(int argc, char *argv[]) {
	char *av0;

	av0 = nil;
	ARGBEGIN{
	case '0':
		av0 = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;
	if(av0 == nil)
		av0 = argv[0];
	if(av0 == nil)
		return 1;

	setsid();
	execvp(av0, argv);
	fatal("setsid: can't exec: %r");
	return 1; /* NOTREACHED */
}

typedef struct exectab exectab;
struct exectab {
	char *cmd;
	int (*fn)(int, char**);
} fstab[] = {
	{"cat", xread},
	{"create", xcreate},
	{"ls", xls},
	{"read", xread},
	{"remove", xremove},
	{"rm", xremove},
	{"write", xwrite},
	{"xwrite", xawrite},
	{0, }
}, utiltab[] = {
	{"namespace", xnamespace},
	{"ns", xnamespace},
	{"setsid", xsetsid},
	{0, }
};

int
main(int argc, char *argv[]) {
	char *address;
	exectab *tab;
	int ret;

	quotefmtinstall();
	fmtinstall('r', errfmt);

	address = getenv("WMII_ADDRESS");

	ARGBEGIN{
	case 'v':
		print("%s-" VERSION ", " COPYRIGHT "\n", argv0);
		exit(0);
	case 'a':
		address = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;

	if(argc < 1)
		usage();

	for(tab=utiltab; tab->cmd; tab++)
		if(!strcmp(*argv, tab->cmd))
			return tab->fn(argc, argv);

	if(address && *address)
		client = ixp_mount(address);
	else
		client = ixp_nsmount("wmii");
	if(client == nil)
		fatal("can't mount: %r\n");

	for(tab=fstab; tab->cmd; tab++)
		if(strcmp(*argv, tab->cmd) == 0) break;
	if(tab->cmd == 0)
		usage();

	ret = tab->fn(argc, argv);

	ixp_unmount(client);
	return ret;
}

