/* Copyright ©2009-2010 Kris Maglione <fbsdaemon@gmail.com>
 * See LICENSE file for license details.
 */

#ifndef CLIENTEXTERN
#  define CLIENTEXTERN extern
#endif

char*	readctl(char*);
void	client_init(char*);

CLIENTEXTERN IxpClient*	client;

