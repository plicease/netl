/*==============================================================================
| hwpassive
|   passively listen for hardware addresses, keep a database of them,
|   and dump them to a file if we get the appropriate dcp(1) signal.
|
|   optimized (and debugged) by Graham THE Ollis <ollisg@netl.org>
|
|   Copyright (C) 1999 Graham THE Ollis <ollisg@netl.org>
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
|  02 jul 99  G. Ollis	finally wrote this module, from a stub module.
|  06 jul 99  G. Ollis	hash the database, rather than a out right linked list
|=============================================================================*/

#include "netl/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef NO_NETDB_H
#include <netdb.h>
#endif

#include "netl/global.h"

#include "netl/ether.h"
#include "netl/ip.h"
#include "netl/filter.h"
#include "netl/action.h"
#include "netl/config.h"
#include "netl/io.h"
#include "netl/resolve.h"
#include "netl/hwpassive.h"

#ifdef BOOL_THREADED
#include <pthread.h>

static pthread_mutex_t write_mutex;
static pthread_mutex_t read_mutex;
static pthread_mutex_t time_mutex;
static int num_readers = 0;

/* we don't prevent starvation here at all.  and frankly, i don't give a damn */

static void
open_read(void)
{
	pthread_mutex_lock(&read_mutex);
	if(num_readers == 0) {
		pthread_mutex_lock(&write_mutex);
	}
	num_readers++;
	pthread_mutex_unlock(&read_mutex);
}

static void
close_read(void)
{
	pthread_mutex_lock(&read_mutex);
	if(num_readers == 1) {
		pthread_mutex_unlock(&write_mutex);
	}
	num_readers--;
	pthread_mutex_unlock(&read_mutex);
}

#define open_write() pthread_mutex_lock(&write_mutex)
#define close_write() pthread_mutex_unlock(&write_mutex)
#define open_time() pthread_mutex_lock(&time_mutex)
#define close_time() pthread_mutex_unlock(&time_mutex)

#else
#define open_read() 
#define close_read()
#define open_write()
#define close_write() 
#define open_time()
#define close_time()
#endif

static char *db_file = NETL_LIB_PATH "/hwpassive";

fun_prefix struct configlist req;

#define HASH_SIZE 0x100
#define hash(hw) (hw[0] ^ hw[1] ^ hw[2] ^ hw[3] ^ hw[4] ^ hw[5])

static hwpassive_entry *db[HASH_SIZE];
static int updated = FALSE;

static int
recurse(FILE *fp, hwpassive_entry *e)
{
	if(e==NULL)
		return TRUE;
	if(!recurse(fp, e->next))
		return FALSE;
	if(fwrite(e, sizeof(hwpassive_entry), 1, fp) == 0) {
		err("hwpassive: warning: error fwrite()");
		return FALSE;
	}
	return TRUE;
}

static void
write_database()
{
	FILE *fp;
	int i;

	log("hwpassive: writting database to file %s", db_file);
	fp = fopen(db_file, "w");
	if(fp == NULL) {
		err("hwpassive: warning: error writing to %s", db_file);
		return;
	} 
	open_read();
	for(i=0; i<HASH_SIZE; i++)
		recurse(fp, db[i]);
	close_read();
	open_write();
	updated = TRUE;
	close_write();
	fclose(fp);
}

static char *
time2str1(time_t t)
{
	static char buffer[40];
	strcpy(buffer, ctime(&t));
	*strchr(buffer, '\n') = 0;
	return buffer;
}

static char *
time2str2(time_t t)
{
	static char buffer[40];
	strcpy(buffer, ctime(&t));
	*strchr(buffer, '\n') = 0;
	return buffer;
}

static void
print_database()
{
	hwpassive_entry *tmp;
	int i;

	log("hwpassive: listing content of database");
	log("xx:xx:xx:xx:xx:xx => x.x.x.x [first] [last]");
	open_read();
	for(i=0; i<HASH_SIZE; i++) {
		tmp = db[i];
		while(tmp != NULL) {
			open_time();
			log("%02x:%02x:%02x:%02x:%02x:%02x => %s [%s] [%s]",
				tmp->hw[0], tmp->hw[1], tmp->hw[2], 
				tmp->hw[3], tmp->hw[4], tmp->hw[5], 
				ip2string(tmp->ip),
				time2str1(tmp->first),
				time2str2(tmp->last));
			close_time();
			tmp = tmp->next;
		}
	}
	close_read();
	log("hwpassive: listing complete");
}


static void
lookup_ip(char *key)
{
	u32			ip;
	struct hostent *	herhost;
	int			i;
	hwpassive_entry *	tmp;
	int			found = 0;

	if((ip = searchbyname(key)) == 0) {
		if((herhost = gethostbyname(key)) != NULL) {
				ip = *((u32 *)herhost->h_addr_list[0]);
		} else {
			log("hwpassive: DNS lookup on %s failed", key);
			return;
		}
	}

	open_read();
	for(i=0; i<HASH_SIZE; i++) {
		tmp = db[i];
		while(tmp != NULL) {
			if(tmp->ip == ip) {
				open_time();
				log("hwpassive:lookup: %s => %02x:%02x:%02x:%02x:%02x:%02x [%s] [%s]",
					ip2string(tmp->ip),
					tmp->hw[0], tmp->hw[1], tmp->hw[2], 
					tmp->hw[3], tmp->hw[4], tmp->hw[5], 
					time2str1(tmp->first),
					time2str2(tmp->last));
				close_time();
				found = 1;
			}
			tmp = tmp->next;
		}
	}
	close_read();

	if(found == 0) {
		log("hwpassive:lookup: not found");
	}
}

static void
lookup_hw(char *key)
{
	char *s;
	u8 hw[6];
	int n=0;
	hwpassive_entry *tmp;
	int found=0;

	/*======================================================================
	| the strtok() man page says:
	|
	| Never use this function.  This function modifies its first
	| argument.   The  identity  of  the delimiting character is
	| lost.  This function cannot be used on constant strings.
	|
	| amusing, but in the Graham New Republic we say, 
	| "Never say never."
	|
	|=====================================================================*/

	s = strtok(key, ":");
	while(s != NULL && n<6) {
		hw[n] = ahextoi(s);
		/* log("%d:%s:%02x", n, s, hw[n]); */
		s = strtok(NULL, ":");
		n++;
	}

	open_read();
	tmp = db[hash(hw)];
	while(tmp != NULL) {
		if(!memcmp(hw, tmp->hw, 6)) {	/* we have a match! */
			open_time();
			log("hwpassive:lookup: %02x:%02x:%02x:%02x:%02x:%02x => %s [%s] [%s]",
				hw[0], hw[1], hw[2], hw[3], hw[4], hw[5],
				ip2string(tmp->ip),
				time2str1(tmp->first),
				time2str2(tmp->last));
			close_time();
			found = 1;
		}
		tmp = tmp->next;
	}
	close_read();
	if(found == 0) {
		log("hwpassive:lookup: not found");
	}
}

void 
destroy(void)
{
	open_read();
	if(updated) {
		close_read();
		write_database();
	} else {
		close_read();
		log("hwpassive: warning: database is unchanged, not writing");
	}
}

void
construct(void)
{
	FILE *fp;
	hwpassive_entry *tmp;
	int i;

	log("hwpassive: startup");
	atexit(destroy);

	#ifdef BOOL_THREADED
		pthread_mutex_init(&read_mutex, NULL);
		pthread_mutex_init(&write_mutex, NULL);
		pthread_mutex_init(&time_mutex, NULL);
	#endif

	for(i=0; i<HASH_SIZE; i++)
		db[i] = NULL;

	fp=fopen(db_file, "r");
	if(fp == NULL) {
		log("hwpassive: no existing database, starting from scratch.");
	} else {
		log("hwpassive: existing database, reading.");
		tmp = allocate(sizeof(hwpassive_entry));
		while(fread(tmp, sizeof(hwpassive_entry), 1, fp) != 0) {
			hwpassive_entry *fred = db[hash(tmp->hw)];
			int b = TRUE;
			while(fred != NULL) {
				if(!memcmp(fred->hw, tmp->hw, 6) &&
				   fred->ip == tmp->ip) {
					err("hwpassive: duplicate entry for "
						"%02x:%02x:%02x:%02x:%02x:%02x"
						" => %s",
						tmp->hw[0], tmp->hw[1], tmp->hw[2],
						tmp->hw[3], tmp->hw[4], tmp->hw[5],
						ip2string(tmp->ip));
					err("hwpassive: updating existing entry");
					if(tmp->first < fred->first)
						fred->first = tmp->first;
					if(tmp->last > fred->last)
						fred->last = tmp->last;
					b = FALSE;
					updated = TRUE;
				}
				fred = fred->next;
			}

			if(b) {
				tmp->next = db[hash(tmp->hw)];
				db[hash(tmp->hw)] = tmp;
				tmp = allocate(sizeof(hwpassive_entry));
			}
		}
		free(tmp);
	}
}

static u8 hwbroadcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static u8 hwloop[6] = { 0, 0, 0, 0, 0, 0 };

static void
scan(u8 *hw, u32 ip)
{
	hwpassive_entry *tmp;
	u8 *tmp2;

	if(!memcmp(hw, hwbroadcast, 6) || !memcmp(hw, hwloop, 6))
		return;

	open_read();
	tmp =db[hash(hw)];

	while(tmp != NULL) {
		if(!memcmp(hw, tmp->hw, 6)) {	/* we have a match! */
			if(ip == tmp->ip) {	/* it's already there, don't need it. */
				close_read();
				open_write();
				tmp->last = time(NULL);
				updated = 1;
				close_write();
				return;
			}
			tmp2 = (char *) &tmp->ip;
			log("found %02x:%02x:%02x:%02x:%02x:%02x => %u.%u.%u.%u in database (old)",
				hw[0], hw[1], hw[2], hw[3], hw[4], hw[5], 
				tmp2[0], tmp2[1], tmp2[2], tmp2[3]);
			break;
		}
		tmp = tmp->next;
	}
	close_read();
	open_write();
	updated = TRUE;
	tmp2 = (char *) &ip;
	log("adding %02x:%02x:%02x:%02x:%02x:%02x => %u.%u.%u.%u (%s)",
		hw[0], hw[1], hw[2], hw[3], hw[4], hw[5], 
		tmp2[0], tmp2[1], tmp2[2], tmp2[3], ip2string(ip));
	tmp = allocate(sizeof(hwpassive_entry));
	tmp->next = db[hash(hw)];
	memcpy(tmp->hw, hw, 6);
	tmp->ip = ip;
	tmp->first = tmp->last = time(NULL);
	db[hash(hw)] = tmp;
	close_write();
}

/*==============================================================================
| check
|=============================================================================*/

static u32 lasthearid = 0;

fun_prefix void
check(u8 *dg, size_t len)
{
	machdr *mh = (machdr *) dg;
        iphdr *ip = (iphdr *) &dg[14];
	udphdr *h = (udphdr *) &dg[14 + (IPIHL(ip->ihl_version) << 2)];
	int size, offset;
	u32 id;
	u16 nsize;
	char message[255];

        if(((machdr*)dg)->type != MACTYPE_IPDG)		/* although we are looking at raw data, it's completely useless, */
                return;					/* unless it has an IP address in it. */

        if(IPVER(ip->ihl_version) == 4) {
		scan(mh->src, ip->saddr);
		scan(mh->dst, ip->daddr);

		/* now, we check to see if this is a dcp packet */
	        if(ip->protocol != PROTOCOL_UDP || ntohs(h->dest) != 47 || 
			ip->saddr != LOCALHOST_IP || ip->daddr != LOCALHOST_IP)
                	return;

		if(memcmp(dg, hwloop, 6) || memcmp(&dg[6], hwloop, 6))
			return;

		offset = sizeof(machdr) + sizeof(iphdr) + sizeof(udphdr);

		id = ntohl(*((u32 *) &dg[offset]));             offset += 4;
		if(id == lasthearid)
			return;
		lasthearid = id;
		nsize = ntohs(*((u16 *) &dg[offset]));  offset += 2;

		size = len - offset;
		if(nsize < size)
			size = nsize;
		if(size > 254)
			size = 254;
		memcpy(message, &dg[offset], size);
			message[size] = '\0';

		if(!strcmp(message, "hwpassive:write")) {
			write_database();
		}
		if(!strcmp(message, "hwpassive:print")) {
			print_database();
		}
		if(!memcmp(message, "hwpassive:lookhw:", 17)) {
			lookup_hw(&message[17]);
		}
		if(!memcmp(message, "hwpassive:lookip:", 17)) {
			lookup_ip(&message[17]);
		}
	}
}

#if BOOL_DYNAMIC_MODULES == 0
void
filt_hwpassive_register_symbols(void)
{
	register_symbol("filt/hwpassive.so", "req", &req);
	register_symbol("filt/hwpassive.so", "check", check);
	register_symbol("filt/hwpassive.so", "construct", construct);
	register_symbol("filt/hwpassive.so", "destroy", destroy);
}
#endif

