/* Copyright ©2008-2010 Kris Maglione <maglione.k at Gmail>
 * See LICENSE file for license details.
 */
#include <string.h>
#include <stuff/util.h>

char*
strend(char *s, int n) {
	int len;

	len = strlen(s);
	return s + max(0, len - n);
}
