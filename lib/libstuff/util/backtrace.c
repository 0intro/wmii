/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#include <bio.h>
#include <plan9.h>
#undef nelem
#include <debug.h>
#include "util.h"

#ifdef __linux__
# define PROGTXT "exe"
#else
# define PROGTXT "file"
#endif

static void
_backtrace(int pid, char *btarg) {
	char *proc, *spid, *gdbcmd;
	int fd[3], p[2];
	int status, cmdfd;

	gdbcmd = estrdup("/tmp/gdbcmd.XXXXXX");
	if(pipe(p) < 0)
		goto done;
	closeexec(p[0]);

	cmdfd = mkstemp(gdbcmd);
	if(cmdfd < 0)
		goto done;

	fprint(cmdfd, "bt %s\n", btarg);
	fprint(cmdfd, "detach\n");
	close(cmdfd);

	fd[0] = open("/dev/null", O_RDONLY);
	fd[1] = p[1];
	fd[2] = dup(2);

	proc = sxprint("/proc/%d/" PROGTXT, pid);
	spid = sxprint("%d", pid);
	if(spawn3l(fd, "gdb", "gdb", "-batch", "-x", gdbcmd, proc, spid, nil) < 0) {
		unlink(gdbcmd);
		goto done;
	}

	Biobuf bp;
	char *s;

	Binit(&bp, p[0], OREAD);
	while((s = Brdstr(&bp, '\n', 1))) {
		Dprint(DStack, "%s\n", s);
		free(s);
	}
	unlink(gdbcmd);

done:
	free(gdbcmd);
	kill(pid, SIGKILL);
	waitpid(pid, &status, 0);
}

void
backtrace(char *btarg) {
	int pid;

	/* Fork so we can backtrace the child. Keep this stack
	 * frame minimal, so the trace is fairly clean.
	 */
	Debug(DStack)
		switch(pid = fork()) {
		case -1:
			return;
		case 0:
			kill(getpid(), SIGSTOP);
			_exit(0);
		default:
			_backtrace(pid, btarg);
			break;
		}

}
