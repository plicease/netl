/*==============================================================================
| io.c
|   optimized (and debugged) by Graham THE Ollis <ollisg@netl.org>
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
|  28 Feb 97  G. Ollis	.92 created module
|  05 Mar 97  G. Ollis	.93 added ope so that all io comunication is handled
|			in this module.  syslog.h should not be handled in
|			any other module.  dump data is an exception to this
|			rule.  maybe some day i'll move that stuff in to here.
|			replaced putchar() with a couple of putc()s
|=============================================================================*/

#include <dlfcn.h>
#include <syslog.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "netl/global.h"
#include "netl/io.h"
#include "netl/options.h"

#ifndef NO_SYSLOGD
int noBackground = FALSE;
#endif

#ifndef NO_TEEOUT
FILE *teefile = NULL;
#endif

char *prog="[unassigned]";

/*==============================================================================
| log()
| + send to syslogd() or to stdout, depending on configuration.
|=============================================================================*/

void
netl_log(char *cp,...)
{
	char buff[2048];	/* this should be enough memory */

	va_list vararg;
	if(cp && *cp) {
		va_start(vararg, cp);
		vsnprintf(buff, 2040, cp, vararg);
		va_end(vararg);
	}

#ifndef NO_SYSLOGD
	if(noBackground) {
#endif
		puts(buff);
#ifndef NO_TEEOUT
		if(teefile != NULL) {
			fputs(buff, teefile);
			fputc('\n', teefile);
			fflush(teefile);
		}
#endif
#ifndef NO_SYSLOGD
	} else {
		syslog(LOG_INFO, buff);
	}
#endif
}

/*==============================================================================
| err()
| + send to syslogd() or to stderr, depending on configuration.
|=============================================================================*/

void
netl_err(char *cp,...)
{
	char buff[2048];	/* this should be enough memory */

	va_list vararg;
	if(cp && *cp) {
		va_start(vararg, cp);
		vsnprintf(buff, 2040, cp, vararg);
		va_end(vararg);
	}

#ifndef NO_SYSLOGD
	if(noBackground) {
#endif
		fputs(prog, stderr);
		putc(':', stderr);
		fputs(buff, stderr);
		putc('\n', stderr);
#ifndef NO_TEEOUT
		if(teefile != NULL) {
			fputs("error:", teefile);
			fputs(buff, teefile);
			fputc('\n', teefile);
			fflush(teefile);
		}
#endif
#ifndef NO_SYSLOGD
	} else
		syslog(LOG_ERR, buff);
#endif
}

/*==============================================================================
| allocate memory, and die if we don't have enough.
| the fact that malloc() returns NULL is non helpful.
|=============================================================================*/

void *
netl_allocate(size_t size)
{
	void *tmp;

	/*log("netl_allocate(%d)", size); */

	if((tmp = malloc(size)) == NULL) {
		die(2, "error: could not malloc(), die");
	}

	return tmp;
}

/*==============================================================================
| open syslog if necessary
| this is a little silly at the moment, but does serve to better modularize
| netl.
|=============================================================================*/

#ifndef NO_SYSLOGD
void
netl_ope(char *s)
{
	if(!noBackground)
		openlog(s, 0, NETL_LOG_FACILITY);
}

void
netl_clo()
{
	if(!noBackground)
		closelog();
}
#endif

/*==============================================================================
| netl module open.
| + open the given module, run construct() if it exists and return the handle
|   to the new module.
|=============================================================================*/

int netl_nmopen_pretend = 0;

void *
netl_nmopen(char *name)
{
	void *handle;
	void (*f)(void);

	if(debug_mode)
		log("loading module: %s", name);

	if(netl_nmopen_pretend) {
		FILE *fp;
		fp = fopen(name, "r");
		if(fp == NULL) {
			die(1, "could not (pretend to) load %s; reason:file does not exist", name);
		}
		fclose(fp);
		handle = allocate(strlen(name)+1);
		strcpy(handle, name);
		
	} else {
		handle = dlopen(name, RTLD_NOW);
		if(handle == NULL) {
			die(1, "could not load %s; reason:%s", name, dlerror());
		}

		f = dlsym(handle, "construct");
		if(f != NULL) 
			f();
	}

	return handle;
}


/*==============================================================================
| + call destroy()
| + deallocate the module.
|=============================================================================*/

int 
netl_nmclose(void *handle)
{
	void (*f)(void);

	if(netl_nmopen_pretend) {
		free(handle);
		return 0;
	}

	if(debug_mode)
		log("destroying module: ??");

	f = dlsym(handle, "destroy");
	if(f != NULL)
		f();

	return dlclose(handle);
}


/*==============================================================================
| wrapper around dlsym for netl modules.
|=============================================================================*/

void *
netl_nmsym(void *handle, char *symbol)
{
	void *sym;

	if(netl_nmopen_pretend)
		return NULL;

	sym = dlsym(handle, symbol);
	if(sym == NULL) {
		die(1, "could not resolve unknown::%s; reason:%s", symbol, dlerror());
	}
	return sym;
}


/*==============================================================================
| ahextoi() 
| + convert a hex string 0xa1f9 into its machine representation.
|=============================================================================*/

int
netl_ahextoi(char *s)
{
	int val;

	for(val=0; *s; s++) {
		val *= 16;
		if('0' <= *s && *s <= '9') {
			val += (*s) - '0';
		} else if('A' <= *s && *s <= 'F') {
			val += (*s) - 'A' + 10;
		} else {
			val += (*s) - 'a' + 10;
		}
	}
	return val;
}
