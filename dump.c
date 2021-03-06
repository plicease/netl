/*==============================================================================
| dump.c
|   code by Graham THE Ollis <ollisg@netl.org>
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
|  23 Feb 97  G. Ollis	.92 created network analysis software
|  28 Feb 97  G. Ollis  took read() and dump() out of neta.c and put them
|			here.
|  27 Mar 99  G. Ollis	renamed read() to readentire() so as not to overwrite
|			the system call read() oops!
|=============================================================================*/

#include <stdio.h>
#include <stdlib.h>

#include "netl/dump.h"
#include "netl/io.h"

/*==============================================================================
| readentire()
| + read the whole file *fn into a allocated buffer, and return it.
| + *prog is a pointer to argv[0];
|=============================================================================*/

unsigned char *
readentire(char *fn, size_t *size, size_t max, char *prog)
{
	FILE		*fp;
	char		*buff, *ptr;
	size_t	br, left;

	if((fp = fopen(fn, "r"))==NULL) {
		fprintf(stderr, "%s: warning: could not open %s, skipping\n", prog, fn);
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	*size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if(max != 0 && *size > max) {
		fprintf(stderr, "%s: warning: file %s is to big skipping\n",
		prog, fn);
		return NULL;
	}

	buff = (char *) malloc(*size);
	if(buff == NULL) {
		die(47, "%s: could not allocate %d bytes, die!", prog, (int) *size);
	}

	/* 
	 | this should fix a bug in the cygwin32 port but it doesn't.
	 | that thing just pisses me off.  of well, it is an improvement over
	 | the old code and doesn't break anything -- i'll keep it.
	 */

	ptr = buff; left = *size;
	while(left > 0 && !feof(fp)) {
		if((br=fread(ptr, 1, left, fp)) == 0) {
			fprintf(stderr, "%s: warning: error reading %s, skipping\n", prog, fn);
			fprintf(stderr, "culd not read %d bytes while not at EOF!\n", (int) left);
			return NULL;
		}
		ptr+=br;
		left-=br;
	}

	fclose(fp);

	return buff;
}

/*==============================================================================
| dumpdata
| + given a buffer *data, output it in a sort of "od -x" kind of way, with
|   an extra ASCII representation on the far right.
|=============================================================================*/

void dumpdata(unsigned char *data, size_t size)
{
	dumpdatafile(data, size, stdout);
}

void dumpdatafile(unsigned char *data, size_t size, FILE *fd)
{
	size_t offset = 0;
	int i;

	fprintf(fd, "data:\n");
	while(offset < size) {
		fprintf(fd, "  %04x ", (int) offset);
		for(i=0; i<16 && offset+i < size; i++)
			fprintf(fd, "%02x ", data[offset+i]);
		while(i++<16)
			fputs("   ", fd);
		for(i=0; i<16 && offset+i < size; i++)
			if(data[offset+i] > 31 && data[offset+i] <127)
				putc(data[offset+i], fd);
			else
				putc('.', fd);
		putc('\n', fd);
		offset+=i;
	}
}
