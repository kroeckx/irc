/*
 *   IRC - Internet Relay Chat, ircd/class.c
 *   Copyright (C) 1990 Darren Reed
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef	lint
static char sccsid[] = "@(#)class.c	1.1 1/21/95 (C) 1990 Darren Reed";
#endif

#include "struct.h"
#include "common.h"
#include "numeric.h"
#include "h.h"

#define BAD_CONF_CLASS		-1
#define BAD_PING		-2
#define BAD_CLIENT_CLASS	-3

aClass	*classes;

int	get_conf_class(aconf)
aConfItem	*aconf;
{
	if ((aconf) && Class(aconf))
		return (ConfClass(aconf));

	Debug((DEBUG_DEBUG,"No Class For %s",
	      (aconf) ? aconf->name : "*No Conf*"));

	return (BAD_CONF_CLASS);

}

static	int	get_conf_ping(aconf)
aConfItem	*aconf;
{
	if ((aconf) && Class(aconf))
		return (ConfPingFreq(aconf));

	Debug((DEBUG_DEBUG,"No Ping For %s",
	      (aconf) ? aconf->name : "*No Conf*"));

	return (BAD_PING);
}



int	get_client_class(acptr)
aClient	*acptr;
{
	Reg	Link	*tmp;
	Reg	aClass	*cl;
	int	retc = BAD_CLIENT_CLASS;

	if (acptr && !IsMe(acptr)  && (acptr->confs))
		for (tmp = acptr->confs; tmp; tmp = tmp->next)
		    {
			if (!tmp->value.aconf ||
			    !(cl = tmp->value.aconf->class))
				continue;
			if (Class(cl) > retc)
				retc = Class(cl);
		    }

	Debug((DEBUG_DEBUG,"Returning Class %d For %s",retc,acptr->name));

	return (retc);
}

int	get_client_ping(acptr)
aClient	*acptr;
{
	int	ping = 0, ping2;
	aConfItem	*aconf;
	Link	*link;

	link = acptr->confs;

	if (link)
		while (link)
		    {
			aconf = link->value.aconf;
			if (aconf->status & (CONF_CLIENT|CONF_CONNECT_SERVER|
					     CONF_NOCONNECT_SERVER|
					     CONF_ZCONNECT_SERVER))
			    {
				ping2 = get_conf_ping(aconf);
				if ((ping2 != BAD_PING) && ((ping > ping2) ||
				    !ping))
					ping = ping2;
			     }
			link = link->next;
		    }
	else
	    {
		ping = PINGFREQUENCY;
		Debug((DEBUG_DEBUG,"No Attached Confs"));
	    }
	if (ping <= 0)
		ping = PINGFREQUENCY;
	Debug((DEBUG_DEBUG,"Client %s Ping %d", acptr->name, ping));
	return (ping);
}

int	get_con_freq(clptr)
aClass	*clptr;
{
	if (clptr)
		return (ConFreq(clptr));
	else
		return (CONNECTFREQUENCY);
}

/*
 * When adding a class, check to see if it is already present first.
 * if so, then update the information for that class, rather than create
 * a new entry for it and later delete the old entry.
 * if no present entry is found, then create a new one and add it in
 * immeadiately after the first one (class 0).
 */
void	add_class(class, ping, confreq, maxli, sendq)
int	class, ping, confreq, maxli;
long	sendq;
{
	aClass *t, *p;

	t = find_class(class);
	if ((t == classes) && (class != 0))
	    {
		p = (aClass *)make_class();
		NextClass(p) = NextClass(t);
		NextClass(t) = p;
		MaxSendq(p) = QUEUELEN;
		istat.is_class++;
	    }
	else
		p = t;
	Debug((DEBUG_DEBUG,
		"Add Class %d: p %x t %x - cf: %d pf: %d ml: %d sq: %l",
		class, p, t, confreq, ping, maxli, QUEUELEN));
	Class(p) = class;
	ConFreq(p) = confreq;
	PingFreq(p) = ping;
	MaxLinks(p) = maxli;
	if (sendq)
		MaxSendq(p) = sendq;
	if (p != t)
		Links(p) = 0;
}

aClass	*find_class(cclass)
int	cclass;
{
	aClass *cltmp;

	for (cltmp = FirstClass(); cltmp; cltmp = NextClass(cltmp))
		if (Class(cltmp) == cclass)
			return cltmp;
	return classes;
}

void	check_class()
{
	Reg	aClass	*cltmp, *cltmp2;

	Debug((DEBUG_DEBUG, "Class check:"));

	for (cltmp2 = cltmp = FirstClass(); cltmp; cltmp = NextClass(cltmp2))
	    {
		Debug((DEBUG_DEBUG,
			"Class %d : CF: %d PF: %d ML: %d LI: %d SQ: %ld",
			Class(cltmp), ConFreq(cltmp), PingFreq(cltmp),
			MaxLinks(cltmp), Links(cltmp), MaxSendq(cltmp)));
		if (MaxLinks(cltmp) < 0)
		    {
			NextClass(cltmp2) = NextClass(cltmp);
			if (Links(cltmp) <= 0)
			{
				free_class(cltmp);
				istat.is_class--;
			}
		    }
		else
			cltmp2 = cltmp;
	    }
}

void	initclass()
{
	classes = (aClass *)make_class();
	istat.is_class++;

	Class(FirstClass()) = 0;
	ConFreq(FirstClass()) = CONNECTFREQUENCY;
	PingFreq(FirstClass()) = PINGFREQUENCY;
	MaxLinks(FirstClass()) = MAXIMUM_LINKS;
	MaxSendq(FirstClass()) = QUEUELEN;
	Links(FirstClass()) = 0;
	NextClass(FirstClass()) = NULL;
}

void	report_classes(sptr, to)
aClient	*sptr;
char	*to;
{
	Reg	aClass	*cltmp;

	for (cltmp = FirstClass(); cltmp; cltmp = NextClass(cltmp))
		sendto_one(sptr, rpl_str(RPL_STATSYLINE, to), 'Y',
			   Class(cltmp), PingFreq(cltmp), ConFreq(cltmp),
			   MaxLinks(cltmp), MaxSendq(cltmp));
}

long	get_sendq(cptr)
aClient	*cptr;
{
	Reg	int	sendq = QUEUELEN, retc = BAD_CLIENT_CLASS;
	Reg	Link	*tmp;
	Reg	aClass	*cl;

	if (cptr->serv)
		sendq = MaxSendq(cptr->serv->nline->class);
	else if (cptr && !IsMe(cptr)  && (cptr->confs))
		for (tmp = cptr->confs; tmp; tmp = tmp->next)
		    {
			if (!tmp->value.aconf ||
			    !(cl = tmp->value.aconf->class))
				continue;
			if (Class(cl) > retc)
				sendq = MaxSendq(cl);
		    }
	return sendq;
}
