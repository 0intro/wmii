/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fmt.h>
#include "util.h"

char*
pathsearch(const char *path, const char *file, bool slashok) {
	char *orig, *p, *s;

	if(!slashok && strchr(file, '/') > file)
		file = sxprint("%s/%s", getcwd(buffer, sizeof buffer), file);
	else if(!strncmp(file, "./",  2))
		file = sxprint("%s/%s", getcwd(buffer, sizeof buffer), file+2);
	if(file[0] == '/') {
		if(access(file, X_OK))
			return strdup(file);
		return nil;
	}

	orig = estrdup(path ? path : getenv("PATH"));
	for(p=orig; (s=strtok(p, ":")); p=nil) {
		s = smprint("%s/%s", s, file);
		if(!access(s, X_OK))
			break;
		free(s);
	}
	free(orig);
	return s;
}
