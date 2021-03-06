/*==============================================================================
| pipe output module for netl
|   by Graham THE Ollis <ollisg@netl.org>
|
|   Copyright (C) 1997 Graham THE Ollis <ollisg@netl.org>
|
|   This program is free software; you can redistribute it and/or modify
|   it under the terms of the GNU General Public License as published by
|   the Free Software Foundation; either version 2 of the License, or
|   (at your option) any later version.
|
|   This program is distributed in the hope that it will be useful,
|   but WITHOUT ANY WARRANTY; without even the implied warranty of
|   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
|   GNU General Public License for more details.
|
|   You should have received a copy of the GNU General Public License
|   along with this program; if not, write to the Free Software
|   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
|
|  Date       Name	Revision
|  ---------  --------  --------
|  26 sep 97  G. Ollis	took this code out of the main module and put it here
|			for safe keeping.
|=============================================================================*/

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>

#include "netl/global.h"

#include "netl/action.h"
#include "netl/filter.h"
#include "netl/ether.h"
#include "netl/ip.h"
#include "netl/config.h"
#include "netl/io.h"

int action_done;	/* indicates weather or not the specified action has been taken
			   on the current datagram */

static int readfd, writefd;
static int pid;

typedef struct {
	char id[4];
	size_t str_len;
	size_t packet_len;
} header;

static header h;

static char *pipeprog = "pipeprog";

void
construct(void)
{
	char *s;
	int fd[2];
	char buffer[10];

	memcpy(h.id, "NETL", 4);

	if((s = getenv("NETL_PIPE_FD")) != NULL) {
		/* we are the callee rather than the caller */
		readfd = -1;
		writefd = atoi(s);
		if(writefd == -1) {
			netl_die(1, "pipe.c:\"%s\" is not a valid fd", s);
		}
		log("pipe.c: using pipefd \"%s\"(%d)", s, writefd);
		return;
	}


	if(pipe(fd) == -1) {
		netl_die(1, "pipe module could not open a pipe!  bad.\n");
	}
	readfd = fd[0];
	writefd = fd[1];

	if((pid = fork()) == 0) {
		if((s = getenv("NETL_PIPE_PROG")) != NULL) {
			/*log("pipe.c:found NETL_PIPE_PROG \"%s\"", s);*/
			pipeprog = s;
		}
		log("pipe.c: using pipeprog \"%s\"", pipeprog);
		sprintf(buffer, "%d", readfd);
		execl(pipeprog, pipeprog, buffer, NULL);
		netl_die(1, "pipe.c: execl(%s, %s, %s, %s) failed!", pipeprog, pipeprog, buffer, NULL);
	}

	if(pid == -1) {
		netl_die(1, "pipe.c: could not fork()!  bad.\n");
	}
}

#undef die
void
die(int retval, char *message)
{
	if(message != NULL) {
		memcpy(h.id, "NDIE", 4);
		h.str_len = strlen(message)+1;
		h.packet_len = 0;
		write(writefd, &h, sizeof(header));
		write(writefd, message, h.str_len);
		//sleep(5);
	}
}

void
destroy(void)
{
	close(writefd);
	kill(pid, SIGTERM);
}

/*==============================================================================
| stub
|=============================================================================*/

void
action(u8 *dg, struct configitem *cf, size_t len)
{
	action_done = TRUE;

	h.str_len = strlen(cf->logname)+1;
	h.packet_len = len;

	write(writefd, &h, sizeof(header));
	write(writefd, cf->logname, h.str_len);
	write(writefd, dg, len);
}

