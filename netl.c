/*==============================================================================
| netl
|   optimized (and debugged) by Graham THE Ollis <ollisg@ns.arizona.edu>
|
|   Copyright (C) 1997 Graham THE Ollis <ollisg@ns.arizona.edu>
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
|  01 Feb 97  G. Ollis	modified, commented (and debugged)
|  08 Feb 97  G. Ollis	added IP address resolving.
|  23 Feb 97  G. Ollis	combined all network monitoring in to single program
|  28 Feb 97  G. Ollis	.92 added the -z option [ and the log() function to
|			replace syslog()]
|  05 Mar 97  G. Ollis	.93 added run time comunication.
|			took all direct syslog stuff out of this module.
|  07 Mar 97  G. Ollis	changed dump name to /tmp/netl/name-pid-time.dg.
|=============================================================================*/

char	*id = "@(#)netl by graham the ollis <ollisg@ns.arizona.edu>";

#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <netdb.h>

#include "global.h"
#include "ether.h"
#include "ip.h"

#include "netl.h"
#include "sighandle.h"
#include "io.h"
#include "options.h"
#include "config.h"
#include "resolve.h"
#include "dcp.h"

/*==============================================================================
| Globals
|=============================================================================*/

struct ifreq oldifr, ifr;
char *prog;
u8 localhardware[6] = {0, 0, 0, 0, 0, 0};
u8 localip[4] = {127, 0, 0, 1};

/*==============================================================================
| it's the clean up function!  it really doesn't need to do much so...
|=============================================================================*/

void cleanup()
{
  clo();
}

/*==============================================================================
| int main(int, char **)
|=============================================================================*/

int
main(int argc, char *argv[])
{
#ifndef NO_SYSLOGD
  pid_t		temp;
#endif

  prog = argv[0];

  setservent(TRUE);
  parsecmdline(argc, argv); 
  if(displayVersion) {
    fputs("netl ", stdout);
    puts(COPYVER);
  }

  if(getuid() != 0) {
    fprintf(stderr, "%s: must be run as root\n", argv[0]);
    return 1;
  }

  preconfig();
  if(argc != 1) 
    while(--argc > 0) {
      argv++;
      if(argv[0][0] != '-') {
        parseconfigline(argv[0]);
        configfile = NULL;
      }
    }

  if(configfile != NULL)
#ifdef NO_SYSLOGD
    readconfig(configfile);
#endif
#ifndef NO_SYSLOGD
    readconfig(configfile, TRUE);
#endif
  postconfig();

#ifndef NO_SYSLOGD
  if(noBackground)
#endif
    return netl(netdevice);
#ifndef NO_SYSLOGD
  else {
    if((temp = fork()) == 0) 
      return netl(netdevice);

    if(temp == -1) {
      fprintf(stderr, "%s: unable to fork\n", argv[0]);
      return 1;
    }
  }
#endif

  return 0;
}

/*==============================================================================
| return the string or "" if it points to null
|=============================================================================*/

char *
string(char *s)
{
  if(s == NULL)
    return "";
  else
    return s;
}

/*==============================================================================
| void netl(char *)
|=============================================================================*/

int
netl(char *dev) {
  int		l;
  int		sock, length;
  struct	sockaddr_in name;
  unsigned char buf[4096];
  unsigned int	fromlen;

  ope("netl");
  log("starting netl, logging %s", dev);
  handle();

  /*============================================================================
  | Get a socket which will collect all packets
  |===========================================================================*/

  if((sock = socket(AF_INET, SOCK_PACKET, htons(ETH_P_ALL))) < 0) {
    err("cannot open raw socket, die");
    return 1;
  }

  /*============================================================================
  | Configure ethernet device
  |===========================================================================*/

  strcpy(ifr.ifr_name, dev);
  strcpy(oldifr.ifr_name, dev);

  /*============================================================================
  | Get flags and place them in ifr structure
  |===========================================================================*/

  if(ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
    err("unable to get %s flags, die", dev);
    return 1;
  }

  /*============================================================================
  | Get flags and place them in oldifr structure
  | This will be used later to change ether device characteristics back
  | to their original value
  |===========================================================================*/

  if(ioctl(sock, SIOCGIFFLAGS, &oldifr) < 0) {
    err("unable to get %s flags, die", dev);
    return 1;
  }

  /*============================================================================
  | Set the promiscous flag
  |===========================================================================*/

  ifr.ifr_flags |= IFF_PROMISC;

  /*============================================================================
  | Set the device flags
  |===========================================================================*/

  if(ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
    err("Unable to set %s flags, die", dev);
    return 1;
  } 

  /*============================================================================
  | Set up sockaddr
  |===========================================================================*/

  name.sin_family = AF_INET;
  name.sin_addr.s_addr = INADDR_ANY;
  name.sin_port = 0;

  length = sizeof(name);

  if(getsockname(sock, (struct sockaddr *) &name, &length) < 0) {
    err("Error: Can't get socket name, die");
    return 1;
  }

  /*============================================================================
  | Entering the data collection loop
  |===========================================================================*/

  while(1) {
    if((l = recvfrom(sock, buf, 1024, 0, 
		     (struct sockaddr *) &name, &fromlen)) < 0)
      err("Error receiving RAW packet");
    else 
      parsedg(buf, l);
  }

  return 0;
}

/*==============================================================================
| dump ip datagram to disk
|=============================================================================*/

void dgdump(u8 *dg, char *name, size_t len)
{
  char		fn[1024];
  FILE		*fp;

  sprintf(fn, "/tmp/netl/%s-%d-%d.dg", name, getpid(), (unsigned) time(NULL));
  if((fp=fopen(fn, "w"))==NULL) {
    err("unable to open dump file %s", fn);
    return;
  }
  if(fwrite(dg, 1, len, fp) != len)
    err("error writing to dump file %s", fn);
  fclose(fp);
}

/*==============================================================================
| check/log icmp
|=============================================================================*/

void
checkicmp(u8 *dg, iphdr ip, icmphdr *h, size_t len)
{
  int i;
  int logged=FALSE,dumped=FALSE;
  struct configitem *c;

  for(i=0; i<icmp_req.index; i++) {

    c = &icmp_req.c[i];

    if(

       /*=======================================================================
       | if we already logged or dumped it, we may not have to check it
       |======================================================================*/

       (c->action == ACTION_LOG && logged)			||
       (c->action == ACTION_DUMP && dumped)			||

       /*=======================================================================
       | must be the correct type
       |======================================================================*/

       (c->check_icmp_type && c->icmp_type != h->type)		||
       (c->check_icmp_code && c->icmp_code != h->code)		||

       /*=======================================================================
       | addresses must be correct
       |======================================================================*/

       (c->check_src_ip && c->src_ip != ip.saddr)		||
       (c->check_dst_ip && c->dst_ip != ip.daddr)		||
       (c->check_src_ip_not && c->src_ip_not == ip.saddr)	||
       (c->check_dst_ip_not && c->dst_ip_not == ip.daddr) 
      )
      continue;

    switch(c->action) {
      case ACTION_LOG:
        log(	"%s %s => %s",
                string(c->logname),
                ip2string(ip.saddr),
                ip2string(ip.daddr));
        logged = TRUE;
        break;

      case ACTION_DUMP:
        dgdump(dg, string(c->logname), len);
        dumped = TRUE;
        break;

      case ACTION_IGNORE:
	return;

      default:
	break;
    }
  }
}

/*==============================================================================
| check/log tcp
|=============================================================================*/

void
checktcp(u8 *dg, iphdr ip, tcphdr *h, size_t len)
{
  int i;
  int logged=FALSE,dumped=FALSE;
  u8 flags=*(((char *) h) + 13);
  struct configitem *c;
  u16 source=ntohs(h->source), dest=ntohs(h->dest);

  for(i=0; i<tcp_req.index; i++) {

    c = &tcp_req.c[i];

    if(

       /*=======================================================================
       | if we already logged or dumped it, we may not have to check it
       |======================================================================*/

       (c->action == ACTION_LOG && logged)			||
       (c->action == ACTION_DUMP && dumped)			||

       /*=======================================================================
       | port must be correct
       |======================================================================*/

       (c->check_src_prt_not && c->src_prt_not == source)	||
       (c->check_src_prt_not && c->dst_prt_not == dest)		||

       (c->check_src_prt && (source < c->src_prt1 ||
                              source > c->src_prt2))		||
       (c->check_dst_prt && (dest < c->dst_prt1 ||
                              dest > c->dst_prt2))		||

       /*=======================================================================
       | flags must be correct
       |======================================================================*/

       (c->check_tcp_flags_on && 
          (flags & c->tcp_flags_on) != c->tcp_flags_on)		||
       (c->check_tcp_flags_off && 
          (~flags & c->tcp_flags_off) != c->tcp_flags_off)	||

       /*=======================================================================
       | addresses must be correct
       |======================================================================*/

       (c->check_src_ip && c->src_ip != ip.saddr)		||
       (c->check_dst_ip && c->dst_ip != ip.daddr)		||
       (c->check_src_ip_not && c->src_ip_not == ip.saddr)	||
       (c->check_dst_ip_not && c->dst_ip_not == ip.daddr) 
      )
      continue;

    switch(c->action) {
      case ACTION_LOG:
        log(	"%s %s:%d => %s:%d",
                string(c->logname),
                ip2string(ip.saddr),
		ntohs(h->source),
                ip2string(ip.daddr),
                ntohs(h->dest));
        logged = TRUE;
        break;

      case ACTION_DUMP:
        dgdump(dg, string(c->logname), len);
        dumped = TRUE;
        break;

      case ACTION_IGNORE:
	return;

      default:
        break;
    }

  }
}

/*==============================================================================
| check/log udp
|=============================================================================*/

void
checkudp(u8 *dg, iphdr ip, udphdr *h, size_t len)
{
  int i;
  int logged=FALSE,dumped=FALSE;
  struct configitem *c;
  u16 source=ntohs(h->source), dest=ntohs(h->dest);

  /*============================================================================
  | check to see if this is a comunication request.
  | but first, check to see if we are even listening.
  |===========================================================================*/

  if(listenport != -1			&&	/* fast! */
     dest == listenport			&&	/* speedy! */
     ip.saddr == LOCALHOST_IP		&&	/* zap! */
     ip.daddr == LOCALHOST_IP		&&	/* zoom! */
     !memcmp(dg, localhardware, 6)	&&	/* kind of slow... */
     !memcmp(dg + 6, localhardware, 6))		/* sigh */
    hear(dg, h, len);

  /*============================================================================
  | process the datagram, even if it is a valid comunication request.
  |===========================================================================*/

  for(i=0; i<udp_req.index; i++) {

    c = &udp_req.c[i];

    if(

       /*=======================================================================
       | if we already logged or dumped it, we may not have to check it
       |======================================================================*/

       (c->action == ACTION_LOG && logged)			||
       (c->action == ACTION_DUMP && dumped)			||

       /*=======================================================================
       | port must be correct
       |======================================================================*/

       (c->check_src_prt_not && c->src_prt_not == source)	||
       (c->check_src_prt_not && c->dst_prt_not == dest)		||

       (c->check_src_prt && (source < c->src_prt1 ||
                              source > c->src_prt2))		||
       (c->check_dst_prt && (dest < c->dst_prt1 ||
                              dest > c->dst_prt2))		||

       /*=======================================================================
       | addresses must be correct
       |======================================================================*/

       (c->check_src_ip && c->src_ip != ip.saddr)		||
       (c->check_dst_ip && c->dst_ip != ip.daddr)		||
       (c->check_src_ip_not && c->src_ip_not == ip.saddr)	||
       (c->check_dst_ip_not && c->dst_ip_not == ip.daddr) 
      )
      continue;

    switch(c->action) {
      case ACTION_LOG:
        log(	"%s %s:%d => %s:%d",
                string(c->logname),
                ip2string(ip.saddr),
		source,
                ip2string(ip.daddr),
                dest);
        logged = TRUE;
        break;

      case ACTION_DUMP:
        dgdump(dg, string(c->logname), len);
        dumped = TRUE;
        break;

      case ACTION_IGNORE:
	return;

      default:
	break;
    }

  }
}

/*==============================================================================
| void parsedg(u8 *buff);
|=============================================================================*/

void
parsedg(u8 *dg, size_t len)
{
  machdr	*mac = (machdr*) dg;
  iphdr		*ip = (iphdr*) &dg[14];

  /*============================================================================
  | check that this is a ip datagram.
  | check the version number.  should be ip version 4.
  |===========================================================================*/

  if(mac->type != MACTYPE_IPDG || ip->version != IP_VERSION) 
    return;

  /*============================================================================
  | locate the subprotocol header and point correct header pointer in the
  | right place.
  |===========================================================================*/

  switch(ip->protocol) {

    case PROTOCOL_ICMP:
      checkicmp(dg, 
          *ip,
          (icmphdr *) &dg[sizeof(machdr) + (ip->ihl << 2)],
          len);
      break;

    case PROTOCOL_TCP:
      checktcp(dg,
          *ip,
          (tcphdr *) &dg[sizeof(machdr) + (ip->ihl << 2)],
          len);
      break;

    case PROTOCOL_UDP:
      checkudp(dg,
          *ip,
          (udphdr *) &dg[sizeof(machdr) + (ip->ihl << 2)],
          len);
      break;

    default:
      /* I don't know this subprotocol */
      break;
  }

}

