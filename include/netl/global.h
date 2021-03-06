/*==============================================================================
| global.h - macros everyone needs
| 
| coded and tested under linux 2.0.23, 2.0.26, stealth kernel 2.0.29
|  by graham the ollis <ollisg@netl.org>
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
==============================================================================*/

#ifndef GLOBAL_H
#define GLOBAL_H

extern char *so_path, *so_path_default;

/*==============================================================================
| start configure
==============================================================================*/

/*==============================================================================
| NO_SYSLOGD
|
| on unix system, output get's sent to syslogd by default.  this can be 
| overridden using the -z option.  on non unix (win32) systems define 
| NO_SYSLOGD to optamize the output code.
==============================================================================*/

#undef NO_SYSLOGD

/*==============================================================================
| NO_TEEOUT
|
| on unix system, you can send out put to a file using redirection and then
| use tail -f to monitor the output.  on windows system, files stay at a size 
| of zero until they are closed.  with NO_TEEOUT you can use the -o option to
| copy a duplicate to a file and monitor stdout.  ok, that was confusing.
|
| unix# netl -z | tee netl.log
|
| is the same as this in win95:
|
| c:> netl -onetl.log
|
| though this only works with NO_TEEOUT.  there is no reason to define it
| unless your windows.  the default for win32 is #undef NO_TEEOUT
==============================================================================*/

/*#define NO_TEEOUT*/

/*==============================================================================
| end configure (hopefully this is far as you need to go)
==============================================================================*/

#include "netl/version.h"

#define TRUE			1
#define FALSE			0

extern char *prog;

/*==============================================================================
| linux 32 bit tested on a modified (stealth) 2.0.29 kernel
|   . currently i test on linux 2.2.x kernel running on i486, i586 and an alpha
|   . does not work on SPARC (am working on this)
==============================================================================*/

#ifdef linux

  #include <netinet/in.h>
  #include <linux/if_ether.h> 
  #include <sys/types.h>
  #include <endian.h>

  typedef u_int8_t	u8;
  typedef u_int16_t	u16;
  typedef u_int32_t	u32;
  typedef u_int64_t	u64;

  #if __BYTE_ORDER == __LITTLE_ENDIAN
    #define NETL_LITTLE_ENDIAN
  #elif __BYTE_ORDER == __BIG_ENDIAN
    #define NETL_BIG_ENDIAN
  #else
    #error "cannot determine byte order!"
  #endif

  #define NETL_LOG_FACILITY	LOG_LOCAL4

  #define OS_UNIX

/*==============================================================================
| the windows port doesn't work yet.  oh well.
==============================================================================*/

#elif __CYGWIN32__

  #include <asm/types.h>

  typedef __u8	u8;
  typedef __u16 u16;
  typedef __u32 u32;

  #define NETL_LITTLE_ENDIAN
/* this doesn't work
  #define ETH_P_ALL		0x0003		// linux/if_ether.h 
  #define SIOCSIFFLAGS		0x8914		// linux/sockios.h
*/

  #define NETL_LOG_FACILITY	LOG_USER

  #define OS_WIN32
  #define NO_SYSLOGD
  #undef  NO_TEEOUT

/*==============================================================================
| DJGPP gnu for 32bit dos.  this is totally unexpected to work
|
| in this case i am atempting to cross compile from linux (same as above)
==============================================================================*/

#elif __DJGPP__

  typedef unsigned char		u8;
  typedef unsigned short	u16;
  typedef unsigned int		u32;

  #define NETL_LITTLE_ENDIAN

  #define NETL_LOG_FACILITY	LOG_USER

  #define OS_WIN32
  #define NO_SYSLOGD
  #undef  NO_TEEOUT

#else
  #error "your operating system isn't defined in global.h"
#endif

#endif /* GLOBAL_H */
