/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_serv.c (formerly ircd/s_msg.c)
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
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

#ifndef lint
static  char rcsid[] = "@(#)$Id: s_serv.c,v 1.101 2002/06/01 22:11:02 chopin Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_SERV_C
#include "s_externs.h"
#undef S_SERV_C

static	char	buf[BUFSIZE];

static	int	check_link __P((aClient *));
static	int	get_version __P((char *version, char *id));
static	void	trace_one __P((aClient *sptr, aClient *acptr));
const	char	*check_servername_errors[3][2] = {
	{ "too long", "Bogus servername - too long" },
	{ "invalid", "Bogus servername - invalid hostname" },
	{ "bogus", "Bogus servername - no dot"}};

/*
** m_functions execute protocol messages on this server:
**
**	cptr	is always NON-NULL, pointing to a *LOCAL* client
**		structure (with an open socket connected!). This
**		identifies the physical socket where the message
**		originated (or which caused the m_function to be
**		executed--some m_functions may call others...).
**
**	sptr	is the source of the message, defined by the
**		prefix part of the message if present. If not
**		or prefix not found, then sptr==cptr.
**
**		(!IsServer(cptr)) => (cptr == sptr), because
**		prefixes are taken *only* from servers...
**
**		(IsServer(cptr))
**			(sptr == cptr) => the message didn't
**			have the prefix.
**
**			(sptr != cptr && IsServer(sptr) means
**			the prefix specified servername. (?)
**
**			(sptr != cptr && !IsServer(sptr) means
**			that message originated from a remote
**			user (not local).
**
**		combining
**
**		(!IsServer(sptr)) means that, sptr can safely
**		taken as defining the target structure of the
**		message in this server.
**
**	*Always* true (if 'parse' and others are working correct):
**
**	1)	sptr->from == cptr  (note: cptr->from == cptr)
**
**	2)	MyConnect(sptr) <=> sptr == cptr (e.g. sptr
**		*cannot* be a local connection, unless it's
**		actually cptr!). [MyConnect(x) should probably
**		be defined as (x == x->from) --msa ]
**
**	parc	number of variable parameter strings (if zero,
**		parv is allowed to be NULL)
**
**	parv	a NULL terminated list of parameter pointers,
**
**			parv[0], sender (prefix string), if not present
**				this points to an empty string.
**			parv[1]...parv[parc-1]
**				pointers to additional parameters
**			parv[parc] == NULL, *always*
**
**		note:	it is guaranteed that parv[0]..parv[parc-1] are all
**			non-NULL pointers.
*/

/*
** m_version
**	parv[0] = sender prefix
**	parv[1] = remote server
*/
int	m_version(cptr, sptr, parc, parv)
aClient *sptr, *cptr;
int	parc;
char	*parv[];
{
	if (hunt_server(cptr,sptr,":%s VERSION :%s",1,parc,parv)==HUNTED_ISME)
		sendto_one(sptr, replies[RPL_VERSION], ME, BadTo(parv[0]),
			   version, debugmode, ME, serveropts);
	return 2;
}

/*
** m_squit
**	parv[0] = sender prefix
**	parv[1] = server name
**	parv[2] = comment
*/
int	m_squit(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg	aConfItem *aconf;
	char	*server;
	Reg	aClient	*acptr = NULL;
	char	*comment = (parc > 2 && parv[2]) ? parv[2] : cptr->name;

	if (parc > 1)
	    {
		server = parv[1];
		/*
		** To accomodate host masking, a squit for a masked server
		** name is expanded if the incoming mask is the same as
		** the server name for that link to the name of link.
		*/
		while ((*server == '*') && IsServer(cptr))
		    {
			aconf = cptr->serv->nline;
			if (!aconf)
				break;
			if (!mycmp(server,
				   my_name_for_link(ME, aconf->port)))
				server = cptr->name;
			break; /* WARNING is normal here */
		    }
		/*
		** Find server matching (compatibility) SID
		*/
		if (server[0]=='$')
		{
			aServer *servptr;

			servptr = find_tokserver(idtol(server + 1, SIDLEN - 1),
				cptr, NULL);
			if (servptr)
			{
				acptr = servptr->bcptr;
			}
		}
		else	/* if (strlen(server)==SIDLEN) perhaps? --Beeth */
		{
			acptr = find_sid(server, NULL);
		}
		/*
		** The following allows wild cards in SQUIT. Only useful
		** when the command is issued by an oper.
		*/
		if (!acptr)
		{
			for (acptr = client;
				(acptr = next_client(acptr, server));
				acptr = acptr->next)
			{
				if (IsServer(acptr) || IsMe(acptr))
				{
					break;
				}
			}
		}
		if (acptr && IsMe(acptr))
		    {
			acptr = cptr;
			server = cptr->sockhost;
		    }
	    }
	else
	    {
		/*
		** This is actually protocol error. But, well, closing
		** the link is very proper answer to that...
		*/
		server = cptr->name;
		acptr = cptr;
	    }

	/*
	** SQUIT semantics is tricky, be careful...
	**
	** The old (irc2.2PL1 and earlier) code just cleans away the
	** server client from the links (because it is never true
	** "cptr == acptr".
	**
	** This logic here works the same way until "SQUIT host" hits
	** the server having the target "host" as local link. Then it
	** will do a real cleanup spewing SQUIT's and QUIT's to all
	** directions, also to the link from which the orinal SQUIT
	** came, generating one unnecessary "SQUIT host" back to that
	** link.
	**
	** One may think that this could be implemented like
	** "hunt_server" (e.g. just pass on "SQUIT" without doing
	** nothing until the server having the link as local is
	** reached). Unfortunately this wouldn't work in the real life,
	** because either target may be unreachable or may not comply
	** with the request. In either case it would leave target in
	** links--no command to clear it away. So, it's better just
	** clean out while going forward, just to be sure.
	**
	** ...of course, even better cleanout would be to QUIT/SQUIT
	** dependant users/servers already on the way out, but
	** currently there is not enough information about remote
	** clients to do this...   --msa
	*/
	if (!acptr)
	    {
		sendto_one(sptr, replies[ERR_NOSUCHSERVER], ME, BadTo(parv[0]), server);
		return 1;
	    }
	if (MyConnect(sptr) && !MyConnect(acptr) && parc < 3)
	    {
                sendto_one(sptr, replies[ERR_NEEDMOREPARAMS], ME, BadTo(parv[0]), "SQUIT");
		return 0;
            }
	if (IsLocOp(sptr) && !MyConnect(acptr))
	    {
		sendto_one(sptr, replies[ERR_NOPRIVILEGES], ME, BadTo(parv[0]));
		return 1;
	    }
	if (!MyConnect(acptr) && (cptr != acptr->from))
	    {
		/*
		** The following is an awful kludge, but I don't see any other
		** way to change the pre 2.10.3 behaviour.  I'm probably going
		** to regret it.. -kalt
		*/
		if ((acptr->from->serv->version & SV_OLDSQUIT) == 0)
		    {
			/* better server: just propagate upstream */
			sendto_one(acptr->from, ":%s SQUIT %s :%s", parv[0],
				   acptr->name, comment);
			sendto_flag(SCH_SERVER,
				    "Forwarding SQUIT %s from %s (%s)",
				    acptr->name, parv[0], comment);
			sendto_flag(SCH_DEBUG,
				    "Forwarding SQUIT %s to %s from %s (%s)",
				    acptr->name, acptr->from->name, 
				    parv[0], comment);
			return 1;
		    }
		/*
		** ack, bad server encountered!
		** must send back to other good servers which were trying to
		** do the right thing, and fake the yet to come SQUIT which
		** will never be received from the bad servers.
		*/
		if (IsServer(cptr) && 
		    (cptr->serv->version & SV_OLDSQUIT) == 0)
		    {
			sendto_one(cptr, ":%s SQUIT %s :%s (Bounced for %s)",
				   ME, acptr->name, comment, parv[0]);
			sendto_flag(SCH_DEBUG, "Bouncing SQUIT %s back to %s",
				    acptr->name, acptr->from->name);
		    }
	    }
	/*
	**  Notify all opers, if my local link is remotely squitted
	*/
	if (MyConnect(acptr) && !IsAnOper(cptr))
	    {
		sendto_ops_butone(NULL, &me,
			":%s WALLOPS :Received SQUIT %s from %s (%s)",
			ME, server, parv[0], comment);
#if defined(USE_SYSLOG) && defined(SYSLOG_SQUIT)
		syslog(LOG_DEBUG,"SQUIT From %s : %s (%s)",
		       parv[0], server, comment);
#endif
	    }
	if (MyConnect(acptr))
	    {
		int timeconnected = timeofday - acptr->firsttime;
		sendto_flag(SCH_NOTICE, 
			    "Closing link to %s (%d, %2d:%02d:%02d)",
			    get_client_name(acptr, FALSE),
			    timeconnected / 86400,
			    (timeconnected % 86400) / 3600,
			    (timeconnected % 3600)/60, 
			    timeconnected % 60);
	    }
	if (!IsMasked(acptr))
	{
		sendto_flag(SCH_SERVER, "Received SQUIT %s from %s (%s)",
			acptr->name, parv[0], comment);
	}

	return exit_client(cptr, acptr, sptr, comment);
}

/*
** get_version()
** Tries to guess what version or, rather, what
** capabilities remote server has.
*/
static int	get_version(version, id)
char	*version;
char	*id;
{
	int result = 0;

	if (!strncmp(version, "0210", 4))
	{
		int vers;

		vers = atoi(version+4);
 		if (vers < 20000)
		{
			/* earlier than 2.10.2 would kill saved users */
			result = SV_OLD;
		}
		else if (vers >= 990000)
		{
			/* alpha/beta of 2.11 */
			result = SV_2_11;
		}
		else
		{
			/* plain 2.10 */
			result = SV_2_10;
		}
	}
	else if (!strncmp(version, "021", 3))
	{
		/* 2.11 or newer (you wish!) */
		/* btw, it will work until we do 2.20+ or 3.0+ version */
		result = SV_2_11;
	}
	else
	{
		/* if it doesn't match above, it is too old
		   to coexist with us, sorry! */
		result = SV_OLD;
	}

	if ((!id || !strcmp("IRC", id))
		&& !strncmp(version, "02100", 5)
		&& atoi(version+5) < 20600)
	{
		/* before 2.10.3a6 ( 2.10.3a5 is just broken ) */
		result |= SV_OLDSQUIT;
	}

	return result;
}

/*
** check_version
**      The PASS command delivers additional information about incoming
**	connection. The data is temporarily stored to info/name/username
**	in m_pass() and processed here before the fields are natively used.
** Return: < 1: exit/error, > 0: no error
*/
int	check_version(cptr)
aClient	*cptr;
{
	char *id, *misc = NULL, *link = NULL;

	Debug((DEBUG_INFO,"check_version: %s", cptr->info));

	if (cptr->info == DefInfo)
	    {
		cptr->hopcount = SV_OLD;
		return 1; /* no version checked (e.g. older than 2.9) */
	    }
	if (id = index(cptr->info, ' '))
	    {
		*id++ = '\0';
		if (link = index(id, ' '))
			*link++ = '\0';
		if (misc = index(id, '|'))
			*misc++ = '\0';
		else
		    {
			misc = id;
			id = "";
		    }
	    }
	else
		id = "";

	cptr->hopcount = get_version(cptr->info, id);

	/* Check version number/mask from conf */
	sprintf(buf, "%s/%s", id, cptr->info);
	if (find_two_masks(cptr->name, buf, CONF_VER))
	    {
		sendto_flag(SCH_ERROR, "Bad version %s %s from %s", id,
			    cptr->info, get_client_name(cptr, TRUE));
		return exit_client(cptr, cptr, &me, "Bad version");
	    }

	if (misc)
	    {
		sprintf(buf, "%s/%s", id, misc);
		/* Check version flags from conf */
		if (find_conf_flags(cptr->name, buf, CONF_VER))
		    {
			sendto_flag(SCH_ERROR, "Bad flags %s (%s) from %s",
				    misc, id, get_client_name(cptr, TRUE));
			return exit_client(cptr, cptr, &me, "Bad flags");
		    }
	    }

	/* right now, I can't code anything good for this */
	/* Stop whining, and do it! ;) */
	if (link && strchr(link, 'Z'))	/* Compression requested */
                cptr->flags |= FLAGS_ZIPRQ;
	/*
	 * If server was started with -p strict, be careful about the
	 * other server mode.
	 */
	if (link && strncmp(cptr->info, "020", 3) &&
	    (bootopt & BOOT_STRICTPROT) && !strchr(link, 'P'))
		return exit_client(cptr, cptr, &me, "Unsafe mode");

	return 2;
}


/* 
** send_server()
** Sends server server to cptr.
*/
static void send_server(aClient *cptr, aClient *server)
{
	aConfItem *aconf;

	if (cptr->from == server->from)
	{
		return;
	}
	aconf = cptr->serv->nline;

	if (!aconf)
	{
		return;
	}

	if (!match(my_name_for_link(ME, aconf->port), server->name) ||
		IsMasked(server))
	{
		/* I'm masking this server, or it was introduced to us with
		** SMASK. */
		if (ST_UID(cptr))
		{
			/* Introduce it to 2.11 with SMASK. */
			sendto_one(cptr, ":%s SMASK %s %s",
				server->serv->up->serv->sid, server->serv->sid,
				server->serv->verstr);
		}
		/* We send nothing at all for a 2.10. */
		return;
	}
	if (ST_UID(cptr))
	{
		/* 2.11 */
		sendto_one(cptr,":%s SERVER %s %d %s %s :%s",
			server->serv->up->serv->sid, server->name,
			server->hopcount + 1, server->serv->sid,
			server->serv->verstr, server->info);
	}
	else
	{
		/* 2.10 */
		sendto_one(cptr,":%s SERVER %s %d %s :%s",
			server->serv->up->serv->maskedby->name,
			server->name, server->hopcount + 1,
			server->serv->tok, server->info);
	}

	return;
}


/*
** introduce_server()
** Introduces a server I got from cptr to all my other servers.
*/
static int introduce_server(aClient *cptr, aClient *server)
{
	int i;
	aClient *acptr;

	for (i = fdas.highest; i >= 0; i--)
	{
		if (!(acptr = local[fdas.fd[i]]) || !IsServer(acptr) ||
			(acptr == cptr) || IsMe(acptr))
		{
			continue;
		}
		send_server(acptr, server);
	}

	return;
}


/* 
** send_server_burst()
** Sends all servers I know to cptr.
** Acptr is the current to send.
*/
static void send_server_burst(aClient *cptr, aClient *acptr)
{
	for (; acptr; acptr = acptr->serv->right)
	{
		if (cptr == acptr)
		{
			continue;
		}
		send_server(cptr, acptr);
		send_server_burst(cptr, acptr->serv->down);
	}

	return;
}


/*
** m_smask - Introduction of servers behind mask
**     parv[0] = sender prefix
**     parv[1] = sid
**     parv[2] = version
*/
int    m_smask(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
       aClient *acptr;
       int i;

	if (parc < 3)
	{
		return exit_client(cptr, cptr, &me,
			"Not enough parameters to SMASK");
	}

	if (!sid_valid(parv[1]))
	{
		sendto_flag(SCH_ERROR,
			"Invalid SID (%s) in SMASK from %s, dropping link",
			parv[1],sptr->name);
		return exit_client(cptr, cptr, &me, "Invalid SID");
	}

	if (find_sid(parv[1],NULL))
	{
		char ecbuf[BUFSIZE];
		sendto_flag(SCH_NOTICE,
			"SID collision (SMASK) on %s brought by %s, "
			"dropping link", parv[1],sptr->name);
		SPRINTF(ecbuf, "SID collision (%s)", parv[1]);
		return exit_client(cptr, cptr, &me, ecbuf);
	}

	acptr = make_client(cptr);
	if (!make_server(acptr))
	{
		return exit_client(cptr, cptr, &me, "No more tokens");
	}
	acptr->hopcount = sptr->hopcount + 1;
	strncpyzt(acptr->name, sptr->name, sizeof(acptr->name));
	acptr->info = mystrdup("Masked Server");
	acptr->serv->up = sptr;
	acptr->serv->snum = sptr->serv->maskedby->serv->snum;
	strncpyzt(acptr->serv->verstr, parv[2], sizeof(acptr->serv->verstr));
	acptr->serv->version = get_version(parv[2], NULL);
	acptr->serv->maskedby = sptr->serv->maskedby;
	SetServer(acptr);
	istat.is_masked++;

	/* We add this server to client list, but *only* to SID hash. */
	add_client_to_list(acptr);

	if (*parv[1] == '$')
	{
		acptr->serv->stok = idtol(parv[1] + 1, SIDLEN - 1);
		SPRINTF(acptr->serv->sid, "$%s",
			ltoid(acptr->serv->ltok, SIDLEN - 1));
		add_to_server_hash_table(acptr->serv, cptr);
	}
	else
	{
		acptr->serv->stok = idtol(parv[3], SIDLEN);
		strncpyzt(acptr->serv->sid, parv[1], SIDLEN + 1);
		add_to_sid_hash_table(parv[1], acptr);
	}

	add_server_to_tree(acptr);

	/* And introduce the server to others. */
	introduce_server(cptr, acptr);
}


/*
** m_server
**	parv[0] = sender prefix
**	parv[1] = servername
**	parv[2] = hopcount
**	parv[3] = sid(2.11)/token(2.10)/serverinfo(local 2.10)
**	parv[4] = server version (remote 2.11)/serverinfo(remote 2.10+local2.11)
**	parv[5] = serverinfo (remote 2.11)
*/
int	m_server(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg	char	*ch;
	Reg	int	i;
	char	info[REALLEN+1], *inpath, *host, *stok;
	char	versionbuf[11];		/* At least PATCHLEVEL size! */
	aClient *acptr, *bcptr;
	aConfItem *aconf;
	int	hop = 0, token = 0;
	int	tmperr;

	if (sptr->user) /* in case NICK hasn't been received yet */
            {
                sendto_one(sptr, replies[ERR_ALREADYREGISTRED], ME, BadTo(parv[0]));
                return 1;
            }
	info[0] = info[REALLEN] = '\0';	/* strncpy() doesn't guarantee NULL */
	inpath = get_client_name(cptr, FALSE);
	if (parc < 2 || *parv[1] == '\0')
	    {
			sendto_one(cptr,"ERROR :No servername");
			return 1;
	    }

	if (parc < 3 || (IsServer(cptr) && parc < 4))
	{
		return exit_client(cptr, cptr, &me, "Server version too old");
	}

	if (ST_UID(cptr) && parc < 6)
	{
		sendto_flag(SCH_ERROR,
			"Not enough parameters to SERVER (%d < 6), "
			"dropping link to %s",	parc, 
			get_client_name(cptr,TRUE));
		return exit_client(cptr, cptr, &me,
			"Not enough parameters to SERVER");
	}

	host = parv[1];
	hop = atoi(parv[2]);
	strncpyzt(info, parv[parc-1], REALLEN);

	/* check if the servername is valid */
	if ((tmperr = check_servername(host)))
	{
		tmperr--;
		sendto_flag(SCH_ERROR,
			"Rejected server with %s server name (%s) from %s",
			check_servername_errors[tmperr][0],
			host, get_client_name(cptr, TRUE));
		return exit_client(cptr, cptr, &me,
			(char *)check_servername_errors[tmperr][1]);
	}
	
	/* *WHEN* can it be that "cptr != sptr" ????? --msa */
	/* When SERVER command (like now) has prefix. -avalon */

	if (IsRegistered(cptr) && ((acptr = find_name(host, NULL))
				   || (acptr = find_mask(host, NULL))))
	    {
		char tbuf[BUFSIZE];
		SPRINTF(tbuf, "Server %s Exists",host);
		/*
		** This link is trying feed me a server that I already have
		** access through another path -- multiple paths not accepted
		** currently, kill this link immeatedly!!
		**
		** Rather than KILL the link which introduced it, KILL the
		** youngest of the two links. -avalon
		*/
		bcptr = (cptr->firsttime > acptr->from->firsttime) ? cptr :
			acptr->from;
		sendto_one(bcptr, "ERROR :Server %s already exists", host);
		/* in both cases the bcptr (the youngest is killed) */
		if (bcptr == cptr)
		    {
			sendto_flag(SCH_ERROR,
			    "Link %s cancelled, server %s already exists",
				    get_client_name(bcptr, TRUE), host);
			return exit_client(bcptr, bcptr, &me, tbuf);
		    }
		else
		    {
			/*
			** in this case, we are not dropping the link from
			** which we got the SERVER message.  Thus we canNOT
			** `return' yet! -krys
			*/
			strcpy(buf, get_client_name(bcptr, TRUE));
			sendto_flag(SCH_ERROR,
			    "Link %s cancelled, server %s reintroduced by %s",
				    buf, host, get_client_name(cptr, TRUE));
			(void) exit_client(bcptr, bcptr, &me, tbuf);
		    }
	    }

	if (IsServer(cptr))
	    {
		/* A server can only be introduced by another server. */
		if (!IsServer(sptr))
		    {
			sendto_flag(SCH_LOCAL,
			    "Squitting %s brought by %s (introduced by %s)",
				    host, get_client_name(cptr, FALSE),
				    sptr->name);
			sendto_one(cptr,
				   ":%s SQUIT %s :(Introduced by %s from %s)",
				   me.name, host, sptr->name,
				   get_client_name(cptr, FALSE));
	  		return 1;
		    }
		/*
		** Server is informing about a new server behind
		** this link. Create REMOTE server structure,
		** add it to list and propagate word to my other
		** server links...
		*/
		if (parc == 1 || info[0] == '\0')
		    {
	  		sendto_one(cptr,
				   "ERROR :No server info specified for %s",
				   host);
			sendto_flag(SCH_ERROR, "No server info for %s from %s",
				    host, get_client_name(cptr, TRUE));
	  		return 1;
		    }

		/*
		** See if the newly found server is behind a guaranteed
		** leaf (L-line). If so, close the link.
		*/
		if ((aconf = find_conf_host(cptr->confs, host, CONF_LEAF)) &&
		    (!aconf->port || (hop > aconf->port)))
		    {
	      		sendto_flag(SCH_ERROR,
				    "Leaf-only link %s->%s - Closing",
				    get_client_name(cptr, TRUE),
				    aconf->host ? aconf->host : "*");
	      		sendto_one(cptr, "ERROR :Leaf-only link, sorry.");
      			return exit_client(cptr, cptr, &me, "Leaf Only");
		    }
		/*
		**
		*/
		if (!(aconf = find_conf_host(cptr->confs, host, CONF_HUB)) ||
		    (aconf->port && (hop > aconf->port)) )
		    {
			sendto_flag(SCH_ERROR,
				    "Non-Hub link %s introduced %s(%s).",
				    get_client_name(cptr, TRUE), host,
				   aconf ? (aconf->host ? aconf->host : "*") :
				   "!");
			return exit_client(cptr, cptr, &me,
					   "Too many servers");
		    }
		/*
		** See if the newly found server has a Q line for it in
		** our conf. If it does, lose the link that brought it
		** into our network. Format:
		**
		** Q:<unused>:<reason>:<servername>
		**
		** Example:  Q:*:for the hell of it:eris.Berkeley.EDU
		*/
		if ((aconf = find_conf_name(host, CONF_QUARANTINED_SERVER)))
		    {
			sendto_ops_butone(NULL, &me,
				":%s WALLOPS * :%s brought in %s, %s %s",
				ME, get_client_name(cptr, TRUE),
				host, "closing link because",
				BadPtr(aconf->passwd) ? "reason unspecified" :
				aconf->passwd);

			sendto_one(cptr,
				   "ERROR :%s is not welcome: %s. %s",
				   host, BadPtr(aconf->passwd) ?
				   "reason unspecified" : aconf->passwd,
				   "Go away and get a life");

			return exit_client(cptr, cptr, &me, "Q-Lined Server");
		    }

		acptr = make_client(cptr);
		if (!make_server(acptr))
		{
			return exit_client(cptr, cptr, &me, "No more tokens");
		}
		acptr->hopcount = hop;
		strncpyzt(acptr->name, host, sizeof(acptr->name));
		if (acptr->info != DefInfo)
			MyFree(acptr->info);
		acptr->info = mystrdup(info);
		acptr->serv->up = sptr;
		acptr->serv->snum = find_server_num(acptr->name);
		acptr->serv->maskedby = acptr;

		if (ST_UID(cptr))
		{
			/* remote is 2.11+ */
			if (!sid_valid(parv[3]))
			{
				sendto_flag(SCH_ERROR,
					"Invalid sid %s from %s, dropping link",
					parv[3], cptr->name);
				return exit_client(cptr, cptr, &me,
					"Invalid SID");
			}
			if (*parv[3] == '$')
			{
				/* compatibility SID */
				acptr->serv->stok = idtol(parv[3] + 1,
					SIDLEN - 1);
				SPRINTF(acptr->serv->sid, "$%s",
					ltoid(acptr->serv->ltok, SIDLEN - 1));
			}
			else
			{
				/* check for SID collision */
				if (find_sid(parv[3],NULL))
				{
					char ecbuf[BUFSIZE];

					sendto_flag(SCH_NOTICE,
						"SID collision on %s brought"
						" by %s, dropping link",
						parv[3], sptr->name);
					SPRINTF(ecbuf, "SID collision (%s)",
						parv[3]);
					return exit_client(cptr, cptr, &me,
						ecbuf);
				}

				strncpyzt(acptr->serv->sid, parv[3], SIDLEN+1);
				acptr->serv->stok = idtol(parv[3], SIDLEN);
				acptr->serv->version |= SV_UID;
				add_to_sid_hash_table(parv[3], acptr);
			}

			strncpyzt(acptr->serv->verstr,
				parv[4], sizeof(acptr->serv->verstr));
			acptr->serv->version = get_version(parv[4],NULL);
		}
		else
		{
			/* remote is 2.10 */
			acptr->serv->stok = atoi(parv[3]);
			strcpy(acptr->serv->verstr,"0");

			SPRINTF(acptr->serv->sid, "$%s",
				ltoid(acptr->serv->ltok, SIDLEN - 1));
			
			aconf = cptr->serv->nline;
			/* Send PING for EOB emulation */
			sendto_one(cptr, ":%s PING %s :%s",
				my_name_for_link(ME, aconf->port),
				my_name_for_link(ME, aconf->port),
				acptr->name);
		}

		SetServer(acptr);
		istat.is_serv++;
		add_client_to_list(acptr);
		add_server_to_tree(acptr);
		(void)add_to_client_hash_table(acptr->name, acptr);
		if (!ST_UID(acptr))
		{
			(void)add_to_server_hash_table(acptr->serv, cptr);
		}

		introduce_server(cptr, acptr);
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_SERVER, acptr->name, acptr,
				      ":%s SERVER %s %d %s :%s", parv[0],
				      acptr->name, hop+1, acptr->serv->tok,
				      acptr->info);
#endif
		sendto_flag(SCH_SERVER, "Received SERVER %s from %s (%d %s)",
			    acptr->name, parv[0], hop+1, acptr->info);
		return 0;
	    }

	if ((!IsUnknown(cptr) && !IsHandshake(cptr)) ||
	    (cptr->flags & FLAGS_UNKCMD))
		return 1;
	/*
	** A local link that is still in undefined state wants
	** to be a SERVER. Check if this is allowed and change
	** status accordingly...
	*/
	strncpyzt(cptr->name, host, sizeof(cptr->name));
	/* cptr->name has to exist before check_version(), and cptr->info
	 * may not be filled before check_version(). */
	if ((hop = check_version(cptr)) < 1)
		return hop;	/* from exit_client() */
	if (cptr->info != DefInfo)
	{
		strncpyzt(versionbuf, cptr->info, sizeof(versionbuf));
		MyFree(cptr->info);
	}
	cptr->info = mystrdup(info[0] ? info : ME);

	switch (check_server_init(cptr))
	{
	case 0 :
		return m_server_estab(cptr, (parc > 3) ? parv[3] : NULL,
			versionbuf);
	case 1 :
		sendto_flag(SCH_NOTICE, "Checking access for %s",
			    get_client_name(cptr,TRUE));
		return 1;
	default :
		ircstp->is_ref++;
		sendto_flag(SCH_NOTICE, "Unauthorized server from %s.",
			    get_client_host(cptr));
		return exit_client(cptr, cptr, &me, "No C/N conf lines");
	}
}

int	m_server_estab(aClient *cptr, char *sid, char *versionbuf)
{
	Reg	aClient	*acptr;
	Reg	aConfItem	*aconf, *bconf;
	char	mlname[HOSTLEN+1], *inpath, *host, *s, *encr, *stok;
	int	i;

	host = cptr->name;
	inpath = get_client_name(cptr,TRUE); /* "refresh" inpath with host */

	if (!(aconf = find_conf(cptr->confs, host, CONF_NOCONNECT_SERVER)))
	    {
		ircstp->is_ref++;
		sendto_one(cptr,
			   "ERROR :Access denied. No N line for server %s",
			   inpath);
		sendto_flag(SCH_ERROR,
			    "Access denied. No N line for server %s", inpath);
		return exit_client(cptr, cptr, &me, "No N line for server");
	    }
	if (!(bconf = find_conf(cptr->confs, host, CONF_CONNECT_SERVER|
				CONF_ZCONNECT_SERVER)))
	    {
		ircstp->is_ref++;
		sendto_one(cptr, "ERROR :Only N (no C) field for server %s",
			   inpath);
		sendto_flag(SCH_ERROR,
			    "Only N (no C) field for server %s",inpath);
		return exit_client(cptr, cptr, &me, "No C line for server");
	    }

	if (cptr->hopcount == SV_OLD) /* lame test, should be == 0 */
	    {
		sendto_one(cptr, "ERROR :Server version is too old.");
		sendto_flag(SCH_ERROR, "Old version for %s", inpath);
		return exit_client(cptr, cptr, &me, "Old version");
	    }

#ifdef CRYPT_LINK_PASSWORD
	/* pass whole aconf->passwd as salt, let crypt() deal with it */

	if (*cptr->passwd)
	    {
		extern  char *crypt();

		encr = crypt(cptr->passwd, aconf->passwd);
		if (encr == NULL)
		    {
			ircstp->is_ref++;
			sendto_one(cptr, "ERROR :No Access (crypt failed) %s",
			  	inpath);
			sendto_flag(SCH_ERROR,
			    	"Access denied (crypt failed) %s", inpath);
			return exit_client(cptr, cptr, &me, "Bad Password");
		    }
	    }
	else
		encr = "";
#else
	encr = cptr->passwd;
#endif  /* CRYPT_LINK_PASSWORD */
	if (*aconf->passwd && !StrEq(aconf->passwd, encr))
	    {
		ircstp->is_ref++;
		sendto_one(cptr, "ERROR :No Access (passwd mismatch) %s",
			   inpath);
		sendto_flag(SCH_ERROR,
			    "Access denied (passwd mismatch) %s", inpath);
		return exit_client(cptr, cptr, &me, "Bad Password");
	    }
	bzero(cptr->passwd, sizeof(cptr->passwd));

#ifndef	HUB
	for (i = 0; i <= highest_fd; i++)
		if (local[i] && IsServer(local[i]))
		    {
			ircstp->is_ref++;
			sendto_flag(SCH_ERROR, "I'm a leaf, cannot link %s",
				    get_client_name(cptr, TRUE));
			return exit_client(cptr, cptr, &me, "I'm a leaf");
		    }
#endif
	(void) strcpy(mlname, my_name_for_link(ME, aconf->port));

	if (!match(cptr->name,mlname))
	{
		sendto_flag(SCH_NOTICE, "Mask of server %s is matching "
			"my name %s, dropping link", cptr->name, mlname);
		return exit_client(cptr, cptr, &me, 
			"Server exists (mask matches)");
	}
	if (!match(mlname,cptr->name))
	{
		sendto_flag(SCH_NOTICE, "Server %s matches my name for link "
			"%s, dropping link", cptr->name, mlname);
		return exit_client(cptr, cptr, &me,
			"Server exists (matches my mask)");
	}

	if (IsUnknown(cptr))
	    {
		if (bconf->passwd[0])
#ifndef	ZIP_LINKS
			sendto_one(cptr, "PASS %s %s IRC|%s %s", bconf->passwd,
				   pass_version, serveropts,
				   (bootopt & BOOT_STRICTPROT) ? "P" : "");
#else
			sendto_one(cptr, "PASS %s %s IRC|%s %s%s",
				   bconf->passwd, pass_version, serveropts,
			   (bconf->status == CONF_ZCONNECT_SERVER) ? "Z" : "",
				   (bootopt & BOOT_STRICTPROT) ? "P" : "");
#endif
		/*
		** Pass my info to the new server
		*/
		sendto_one(cptr, "SERVER %s 1 %s :%s",
			mlname, me.serv->sid, me.info);

		/*
		** If we get a connection which has been authorized to be
		** an already existing connection, remove the already
		** existing connection if it has a sendq else remove the
		** new and duplicate server. -avalon
		** Remove existing link only if it has been linked for longer
		** and has sendq higher than a threshold. -Vesa
		*/
		if ((acptr = find_name(host, NULL))
		    || (acptr = find_mask(host, NULL)))
		    {
			if (MyConnect(acptr) &&
			    DBufLength(&acptr->sendQ) > CHREPLLEN &&
			    timeofday - acptr->firsttime > TIMESEC)
				(void) exit_client(acptr, acptr, &me,
						   "New Server");
			else
				return exit_client(cptr, cptr, &me,
						   "Server Exists");
		    }
	    }
	else
	    {
		s = (char *)index(aconf->host, '@');
		*s = '\0'; /* should never be NULL */
		Debug((DEBUG_INFO, "Check Usernames [%s]vs[%s]",
			aconf->host, cptr->username));
		if (match(aconf->host, cptr->username))
		    {
			*s = '@';
			ircstp->is_ref++;
			sendto_flag(SCH_ERROR,
				    "Username mismatch [%s]v[%s] : %s",
				    aconf->host, cptr->username,
				    get_client_name(cptr, TRUE));
			sendto_one(cptr, "ERROR :No Username Match");
			return exit_client(cptr, cptr, &me, "Bad User");
		    }
		*s = '@';
	    }

#ifdef	ZIP_LINKS
	if ((cptr->flags & FLAGS_ZIPRQ) &&
	    (bconf->status == CONF_ZCONNECT_SERVER))
	    {
		if (zip_init(cptr) == -1)
		    {
			zip_free(cptr);
			sendto_flag(SCH_ERROR,
			    "Unable to setup compressed link for %s",
				    get_client_name(cptr, TRUE));
			return exit_client(cptr, cptr, &me,
					   "zip_init() failed");
		    }
		cptr->flags |= FLAGS_ZIP|FLAGS_ZIPSTART;
	    }
#endif

	/* version has been temporarily stored in ->hopcount */
	if (cptr->hopcount & SV_UID)
	{
		if (!sid)
		{
			sendto_flag(SCH_ERROR, "Invalid SID from %s",
				get_client_name(cptr, TRUE));
			return exit_client(cptr, cptr, &me, "Invalid SID");
		}
		if (!sid_valid(sid))
		{
			sendto_flag(SCH_ERROR, "Invalid SID %s from %s",
				sid, get_client_name(cptr, TRUE));
			return exit_client(cptr, cptr, &me, "Invalid SID");
		}
		if (find_sid(sid, NULL))
		{
			sendto_flag(SCH_NOTICE,	"Link %s tried to introduce"
				" already existing SID (%s), dropping link",
				get_client_name(cptr,TRUE),sid);
			return exit_client(cptr, cptr, &me,"SID collision");
		}
	}

	if (!(cptr->hopcount & SV_2_10))
	{
		/* remote is older than 2.10.2 */
		sendto_flag(SCH_ERROR, "Remote server %s is too old "
			"(<2.10.2), dropping link", get_client_name(cptr,TRUE));
		return exit_client(cptr,cptr,&me,"Too old version");
	}

	det_confs_butmask(cptr, CONF_LEAF|CONF_HUB|CONF_NOCONNECT_SERVER);
	/*
	** *WARNING*
	** 	In the following code in place of plain server's
	**	name we send what is returned by get_client_name
	**	which may add the "sockhost" after the name. It's
	**	*very* *important* that there is a SPACE between
	**	the name and sockhost (if present). The receiving
	**	server will start the information field from this
	**	first blank and thus puts the sockhost into info.
	**	...a bit tricky, but you have been warned, besides
	**	code is more neat this way...  --msa
	*/
	SetServer(cptr);
	istat.is_unknown--;
	istat.is_serv++;
	istat.is_myserv++;
	nextping = timeofday;
	sendto_flag(SCH_NOTICE, "Link with %s established. (%X%s)", inpath,
		    cptr->hopcount, (cptr->flags & FLAGS_ZIP) ? "z" : "");
	(void)add_to_client_hash_table(cptr->name, cptr);
	/* doesnt duplicate cptr->serv if allocted this struct already */
	if (!make_server(cptr))
	{
		return exit_client(cptr, cptr, &me, "No more tokens");
	}
	cptr->serv->up = &me;
	cptr->serv->maskedby = cptr;
	cptr->serv->nline = aconf;
	cptr->serv->version = cptr->hopcount;   /* temporary location */
	cptr->hopcount = 1;			/* local server connection */
	cptr->serv->snum = find_server_num(cptr->name);
	cptr->serv->stok = 1;

	strncpyzt(cptr->serv->verstr, versionbuf, sizeof(cptr->serv->verstr));
	if (ST_UID(cptr))
	{
		strcpy(cptr->serv->sid, sid);
		add_to_sid_hash_table(sid, cptr);
	}
	else
	{
		SPRINTF(cptr->serv->sid,"$%s", 
			ltoid(cptr->serv->ltok, SIDLEN-1));
	}

	cptr->flags |= FLAGS_CBURST;
	add_server_to_tree(cptr);
	if (!ST_UID(cptr))
	{
		(void) add_to_server_hash_table(cptr->serv, cptr);
	}
	Debug((DEBUG_NOTICE, "Server link established with %s V%X %d",
		cptr->name, cptr->serv->version, cptr->serv->stok));
	add_fd(cptr->fd, &fdas);
#ifdef	USE_SERVICES
	check_services_butone(SERVICE_WANT_SERVER, cptr->name, cptr,
			      ":%s SERVER %s %d %s :%s", ME, cptr->name,
			      cptr->hopcount+1, cptr->serv->tok, cptr->info);
#endif
	sendto_flag(SCH_SERVER, "Sending SERVER %s (%d %s)", cptr->name,
		    1, cptr->info);
	introduce_server(cptr, cptr);
	
	/*
	** Pass on my client information to the new server
	**
	** First, pass only servers (idea is that if the link gets
	** cancelled beacause the server was already there,
	** there are no NICK's to be cancelled...). Of course,
	** if cancellation occurs, all this info is sent anyway,
	** and I guess the link dies when a read is attempted...? --msa
	** 
	** Note: Link cancellation to occur at this point means
	** that at least two servers from my fragment are building
	** up connection this other fragment at the same time, it's
	** a race condition, not the normal way of operation...
	**
	** ALSO NOTE: using the get_client_name for server names--
	**	see previous *WARNING*!!! (Also, original inpath
	**	is destroyed...)
	*/
	send_server_burst(cptr, me.serv->down);

	for (acptr = &me; acptr; acptr = acptr->prev)
	    {
		/* acptr->from == acptr for acptr == cptr */
		if (acptr->from == cptr)
			continue;
		if (IsPerson(acptr))
		    {
			/*
			** IsPerson(x) is true only when IsClient(x) is true.
			** These are only true when *BOTH* NICK and USER have
			** been received. -avalon
			*/
			if (!ST_UID(cptr) && *mlname == '*' &&
			    match(mlname, acptr->user->server) == 0)
				stok = me.serv->tok;
			else
				stok = acptr->user->servp->tok;
			send_umode(NULL, acptr, 0, SEND_UMODES, buf);
			if (ST_UID(cptr) && *acptr->user->uid)
				sendto_one(cptr,
					   ":%s UNICK %s %s %s %s %s %s :%s",
					   acptr->user->servp->sid,
					   acptr->name, acptr->user->uid,
					   acptr->user->username,
					   acptr->user->host,
					   acptr->user->sip,
					   (*buf) ? buf : "+", acptr->info);
			else
				sendto_one(cptr,"NICK %s %d %s %s %s %s :%s",
					   acptr->name, acptr->hopcount + 1,
					   acptr->user->username,
					   acptr->user->host, stok,
					   (*buf) ? buf : "+", acptr->info);
		    }
		else if (IsService(acptr) &&
			 match(acptr->service->dist, cptr->name) == 0)
		    {
			if (!ST_UID(cptr) && *mlname == '*' &&
			    match(mlname, acptr->service->server) == 0)
				stok = me.serv->tok;
			else
				stok = acptr->service->servp->tok;
			sendto_one(cptr, "SERVICE %s %s %s %d %d :%s",
				   acptr->name, stok, acptr->service->dist,
				   acptr->service->type, acptr->hopcount + 1,
				   acptr->info);
		    }
		/* the previous if does NOT catch all services.. ! */
	    }

	flush_connections(cptr->fd);

	/*
	** Last, pass all channels modes
	** only sending modes for LIVE channels.
	*/
	    {
		Reg	aChannel *chptr;
		for (chptr = channel; chptr; chptr = chptr->nextch)
			if (chptr->users)
			    {
				send_channel_members(cptr, chptr);
				send_channel_modes(cptr, chptr);
			    }
	    }
	if (ST_UID(cptr))
	{
		if (istat.is_myserv == 1)
		{ /* remote server is the only we have connected */
			sendto_one(cptr, ":%s EOB", me.serv->sid);
		}
		else
		{
			aServer *asptr;
			char eobbuf[BUFSIZE];
			char *e;
			int eobmaxlen;
			int flag;

			e = eobbuf;
			eobmaxlen = BUFSIZE
					- 1		/*    ":"     */
					- SIDLEN 	/*  my SID    */
					- 6 		/*   " EOB :" */
					- 2;		/*   "\r\n"   */

			/* space for last comma and SID (calculation moved
			 * from "if (e - eobbuf > eobmaxlen)" inside following
			 * loop)
			 */ 
			eobmaxlen -= SIDLEN + 1;
			
			/* send EOBs */
			for (asptr = svrtop; asptr; asptr = asptr->nexts)
			{
				/* Send EOBs only for servers which already
				 * finished bursting */
				if (!IsBursting(asptr->bcptr))
				{
					if ((int) (e  - eobbuf) > eobmaxlen)
					{
						*e = '\0';
						/* eobbuf always starts with
						 * comma, so +1 gets rid of it
						 */
						sendto_one(cptr, ":%s EOB :%s",
							me.serv->sid,
							eobbuf + 1);
						e = eobbuf;
					}

					*e++ = ',';
					memcpy(e, asptr->sid, SIDLEN);
					e += SIDLEN;
				}
			}
			/* Send the rest, if any */
			if (e > eobbuf)
			{
				*e = '\0';
				sendto_one(cptr, ":%s EOB :%s", me.serv->sid,
						eobbuf+1);
			}
			else
			{
				sendto_one(cptr, ":%s EOB", me.serv->sid);
			}
		}
	}
	else
	{
		/* Send PING for EOB emulation */
		sendto_one(cptr, ":%s PING %s :%s", mlname, mlname,
			   cptr->name);
	}
	cptr->flags &= ~FLAGS_CBURST;
#ifdef	ZIP_LINKS
 	/*
 	** some stats about the connect burst,
 	** they are slightly incorrect because of cptr->zip->outbuf.
 	*/
 	if ((cptr->flags & FLAGS_ZIP) && cptr->zip->out->total_in)
	  sendto_flag(SCH_NOTICE,
		      "Connect burst to %s: %lu, compressed: %lu (%3.1f%%)",
		      get_client_name(cptr, TRUE),
		      cptr->zip->out->total_in,cptr->zip->out->total_out,
 	    (float) 100*cptr->zip->out->total_out/cptr->zip->out->total_in);
#endif
	return 0;
}

/*
** m_info
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_info(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	char **text = infotext;

	if (IsServer(cptr) && check_link(cptr))
	    {
		sendto_one(sptr, replies[RPL_TRYAGAIN], ME, BadTo(parv[0]),
			   "INFO");
		return 5;
	    }
	if (hunt_server(cptr,sptr,":%s INFO :%s",1,parc,parv) == HUNTED_ISME)
	    {
		while (*text)
			sendto_one(sptr, replies[RPL_INFO], ME, BadTo(parv[0]), *text++);

		sendto_one(sptr, replies[RPL_INFO], ME, BadTo(parv[0]), "");
		sendto_one(sptr,
			   ":%s %d %s :Birth Date: %s, compile # %s",
			   ME, RPL_INFO, parv[0], creation, generation);
		sendto_one(sptr, ":%s %d %s :On-line since %s",
			   ME, RPL_INFO, parv[0],
			   myctime(me.firsttime));
		sendto_one(sptr, replies[RPL_ENDOFINFO], ME, BadTo(parv[0]));
		return 5;
	    }
	else
		return 10;
}

/*
** m_links
**	parv[0] = sender prefix
**	parv[1] = servername mask
** or
**	parv[0] = sender prefix
**	parv[1] = server to query 
**      parv[2] = servername mask
*/
int	m_links(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg	aServer	*asptr;
	char	*mask;
	aClient	*acptr;

	if (parc > 2)
	    {
		if (IsServer(cptr) && check_link(cptr) && !IsOper(sptr))
		    {
			sendto_one(sptr, replies[RPL_TRYAGAIN], ME, BadTo(parv[0]),
				   "LINKS");
			return 5;
		    }
		if (hunt_server(cptr, sptr, ":%s LINKS %s :%s", 1, parc, parv)
				!= HUNTED_ISME)
			return 5;
		mask = parv[2];
	    }
	else
		mask = parc < 2 ? NULL : parv[1];

	for (asptr = svrtop, (void)collapse(mask); asptr; asptr = asptr->nexts) 
	    {
		acptr = asptr->bcptr;
		if (IsMasked(acptr))
		{
			continue;
		}
		if (!BadPtr(mask) && match(mask, acptr->name))
			continue;
		sendto_one(sptr, replies[RPL_LINKS], ME, BadTo(parv[0]),
			   acptr->name, acptr->serv->up->name,
			   acptr->hopcount, (acptr->info[0] ? acptr->info :
			   "(Unknown Location)"));
	    }

	sendto_one(sptr, replies[RPL_ENDOFLINKS], ME, BadTo(parv[0]),
		   BadPtr(mask) ? "*" : mask);
	return 2;
}

/*
** m_summon should be redefined to ":prefix SUMMON host user" so
** that "hunt_server"-function could be used for this too!!! --msa
** As of 2.7.1e, this was the case. -avalon
**
**	parv[0] = sender prefix
**	parv[1] = user
**	parv[2] = server
**	parv[3] = channel (optional)
*/
int	m_summon(cptr, sptr, parc, parv)
aClient *sptr, *cptr;
int	parc;
char	*parv[];
{
	char	*host, *user, *chname;
#ifdef	ENABLE_SUMMON
	char	hostbuf[17], namebuf[10], linebuf[10];
#  ifdef LEAST_IDLE
	char	linetmp[10], ttyname[15]; /* Ack */
	struct	stat stb;
	time_t	ltime = (time_t)0;
#  endif
	int	fd, flag = 0;
#endif

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, replies[ERR_NORECIPIENT], ME, BadTo(parv[0]), "SUMMON");
		return 1;
	    }
	user = parv[1];
	host = (parc < 3 || BadPtr(parv[2])) ? ME : parv[2];
	chname = (parc > 3) ? parv[3] : "*";
	/*
	** Summoning someone on remote server, find out which link to
	** use and pass the message there...
	*/
	parv[1] = user;
	parv[2] = host;
	parv[3] = chname;
	parv[4] = NULL;
	if (hunt_server(cptr, sptr, ":%s SUMMON %s %s %s", 2, parc, parv) ==
	    HUNTED_ISME)
	    {
#ifdef ENABLE_SUMMON
		if ((fd = utmp_open()) == -1)
		    {
			sendto_one(sptr, replies[ERR_FILEERROR], ME, BadTo(parv[0]),
				   "open", UTMP);
			return 1;
		    }
#  ifndef LEAST_IDLE
		while ((flag = utmp_read(fd, namebuf, linebuf, hostbuf,
					 sizeof(hostbuf))) == 0) 
			if (StrEq(namebuf,user))
				break;
#  else
		/* use least-idle tty, not the first
		 * one we find in utmp. 10/9/90 Spike@world.std.com
		 * (loosely based on Jim Frost jimf@saber.com code)
		 */
		
		while ((flag = utmp_read(fd, namebuf, linetmp, hostbuf,
					 sizeof(hostbuf))) == 0)
		    {
			if (StrEq(namebuf,user))
			    {
				SPRINTF(ttyname,"/dev/%s",linetmp);
				if (stat(ttyname,&stb) == -1)
				    {
					sendto_one(sptr,
						   replies[ERR_FILEERROR],
						   ME, BadTo(sptr->name),
						   "stat", ttyname);
					return 1;
				    }
				if (!ltime)
				    {
					ltime= stb.st_mtime;
					(void)strcpy(linebuf,linetmp);
				    }
				else if (stb.st_mtime > ltime) /* less idle */
				    {
					ltime= stb.st_mtime;
					(void)strcpy(linebuf,linetmp);
				    }
			    }
		    }
#  endif
		(void)utmp_close(fd);
#  ifdef LEAST_IDLE
		if (ltime == 0)
#  else
		if (flag == -1)
#  endif
			sendto_one(sptr, replies[ERR_NOLOGIN], ME, BadTo(parv[0]), user);
		else
			summon(sptr, user, linebuf, chname);
#else
		sendto_one(sptr, replies[ERR_SUMMONDISABLED], ME, BadTo(parv[0]));
#endif /* ENABLE_SUMMON */
	    }
	else
		return 3;
	return 2;
}


/*
** m_stats
**	parv[0] = sender prefix
**	parv[1] = statistics selector (defaults to Message frequency)
**	parv[2] = server name (current server defaulted, if omitted)
**
**	Currently supported are:
**		M = Message frequency (the old stat behaviour)
**		L = Local Link statistics
**		C = Report C and N configuration lines
*/
/*
** m_stats/stats_conf
**    Report N/C-configuration lines from this server. This could
**    report other configuration lines too, but converting the
**    status back to "char" is a bit akward--not worth the code
**    it needs...
**
**    Note:   The info is reported in the order the server uses
**	      it--not reversed as in ircd.conf!
*/

static int report_array[17][3] = {
		{ CONF_ZCONNECT_SERVER,	  RPL_STATSCLINE, 'c'},
		{ CONF_CONNECT_SERVER,	  RPL_STATSCLINE, 'C'},
		{ CONF_NOCONNECT_SERVER,  RPL_STATSNLINE, 'N'},
		{ CONF_CLIENT,		  RPL_STATSILINE, 'I'},
		{ CONF_RCLIENT,		  RPL_STATSILINE, 'i'},
		{ CONF_OTHERKILL,	  RPL_STATSKLINE, 'k'},
		{ CONF_KILL,		  RPL_STATSKLINE, 'K'},
		{ CONF_QUARANTINED_SERVER,RPL_STATSQLINE, 'Q'},
		{ CONF_LEAF,		  RPL_STATSLLINE, 'L'},
		{ CONF_OPERATOR,	  RPL_STATSOLINE, 'O'},
		{ CONF_HUB,		  RPL_STATSHLINE, 'H'},
		{ CONF_LOCOP,		  RPL_STATSOLINE, 'o'},
		{ CONF_SERVICE,		  RPL_STATSSLINE, 'S'},
		{ CONF_VER,		  RPL_STATSVLINE, 'V'},
		{ CONF_BOUNCE,		  RPL_STATSBLINE, 'B'},
		{ CONF_DENY,		  RPL_STATSDLINE, 'D'},
		{ 0, 0, 0}
	};

static	void	report_configured_links(sptr, to, mask)
aClient *sptr;
char	*to;
int	mask;
{
	static	char	null[] = "<NULL>";
	aConfItem *tmp;
	int	*p, port;
	char	c, *host, *pass, *name;
	
	for (tmp = (mask & (CONF_KILL|CONF_OTHERKILL)) ? kconf : conf;
	     tmp; tmp = tmp->next)
		if (tmp->status & mask)
		    {
			for (p = &report_array[0][0]; *p; p += 3)
				if (*p == tmp->status)
					break;
			if (!*p)
				continue;
			c = (char)*(p+2);
			host = BadPtr(tmp->host) ? null : tmp->host;
			pass = BadPtr(tmp->passwd) ? NULL : tmp->passwd;
			name = BadPtr(tmp->name) ? null : tmp->name;
			port = (int)tmp->port;
			/*
			 * On K/V lines the passwd contents can be
			 * displayed on STATS reply. 	-Vesa
			 */
			if (tmp->status == CONF_KILL
			    || tmp->status == CONF_OTHERKILL
			    || tmp->status == CONF_VER)
				sendto_one(sptr, replies[p[1]], ME, BadTo(to), c, host,
					   (pass) ? pass : null,
					   name, port, get_conf_class(tmp));
			else
				sendto_one(sptr, replies[p[1]], ME, BadTo(to), c, host,
					   (pass) ? "*" : null,
					   name, port, get_conf_class(tmp));
		    }
	return;
}

static	void	report_ping(sptr, to)
aClient	*sptr;
char	*to;
{
	aConfItem *tmp;
	aCPing	*cp;

	for (tmp = conf; tmp; tmp = tmp->next)
		if ((cp = tmp->ping) && cp->lseq)
		    {
			if (mycmp(tmp->name, tmp->host))
				SPRINTF(buf,"%s[%s]",tmp->name, tmp->host);
			else
				(void)strcpy(buf, tmp->name);
			sendto_one(sptr, replies[RPL_STATSPING], ME, BadTo(to),
				   buf, cp->lseq, cp->lrecvd,
				   cp->ping / (cp->recvd ? cp->recvd : 1),
				   tmp->pref);
			sendto_flag(SCH_DEBUG, "%s: %d", buf, cp->seq);
		    }
	return;
}

int	m_stats(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	static	char	Lformat[]  = ":%s %d %s %s %u %lu %llu %lu %llu :%u";
	struct	Message	*mptr;
	aClient	*acptr;
	char	stat = parc > 1 ? parv[1][0] : '\0';
	Reg	int	i;
	int	wilds, doall;
	char	*name, *cm;

	if (IsServer(cptr) &&
	    (stat != 'd' && stat != 'p' && stat != 'q' && stat != 's' &&
	     stat != 'u' && stat != 'v') &&
	    !((stat == 'o' || stat == 'c') && IsOper(sptr)))
	    {
		if (check_link(cptr))
		    {
			sendto_one(sptr, replies[RPL_TRYAGAIN], ME, BadTo(parv[0]),
				   "STATS");
			return 5;
		    }
	    }
	if (parc == 3)
	    {
		if (hunt_server(cptr, sptr, ":%s STATS %s %s",
				2, parc, parv) != HUNTED_ISME)
			return 5;
	    }
	else if (parc >= 3)
	    {
		if (hunt_server(cptr, sptr, ":%s STATS %s %s %s",
				2, parc, parv) != HUNTED_ISME)
			return 5;
	    }

	name = (parc > 2) ? parv[2] : ME;
	cm = (parc > 3) ? parv[3]: name;
	doall = !match(name, ME) && !match(cm, ME);
	wilds = index(cm, '*') || index(cm, '?');

	switch (stat)
	{
	case 'L' : case 'l' :
		/*
		 * send info about connections which match, or all if the
		 * mask matches ME.  Only restrictions are on those who
		 * are invisible not being visible to 'foreigners' who use
		 * a wild card based search to list it.
		 */
		if (doall || wilds)
		    {
			for (i = 0; i <= highest_fd; i++)
			    {
				if (!(acptr = local[i]))
					continue;
				if (IsPerson(acptr) && !(MyConnect(sptr) 
				    && IsAnOper(sptr)) && acptr != sptr)
					continue;
				if (wilds && match(cm, acptr->name))
					continue;
				sendto_one(cptr, Lformat, ME,
					RPL_STATSLINKINFO, parv[0],
					get_client_name(acptr, isupper(stat)),
					(int)DBufLength(&acptr->sendQ),
					acptr->sendM, acptr->sendB,
					acptr->receiveM, acptr->receiveB,
					timeofday - acptr->firsttime);
			    }
		    }
		else
		    {
			if ((acptr = find_client(cm, NULL)))
				sendto_one(cptr, Lformat, ME,
					RPL_STATSLINKINFO, parv[0],
					get_client_name(acptr, isupper(stat)),
					(int)DBufLength(&acptr->sendQ),
					acptr->sendM, acptr->sendB,
					acptr->receiveM, acptr->receiveB,
					timeofday - acptr->firsttime);
			
		    }
		break;
#if defined(USE_IAUTH)
	case 'a' : case 'A' : /* iauth configuration */
		report_iauth_conf(sptr, parv[0]);
		break;
#endif
	case 'B' : case 'b' : /* B conf lines */
		report_configured_links(cptr, parv[0], CONF_BOUNCE);
		break;
	case 'c' : case 'C' : /* C and N conf lines */
		report_configured_links(cptr, parv[0], CONF_CONNECT_SERVER|
					CONF_ZCONNECT_SERVER|
					CONF_NOCONNECT_SERVER);
		break;
	case 'd' : case 'D' : /* defines */
		send_defines(cptr, parv[0]);
		break;
	case 'H' : case 'h' : /* H, L and D conf lines */
		report_configured_links(cptr, parv[0],
					CONF_HUB|CONF_LEAF|CONF_DENY);
		break;
	case 'I' : case 'i' : /* I (and i) conf lines */
		report_configured_links(cptr, parv[0],
					CONF_CLIENT|CONF_RCLIENT);
		break;
	case 'K' : case 'k' : /* K lines */
		report_configured_links(cptr, parv[0],
					(CONF_KILL|CONF_OTHERKILL));
		break;
	case 'M' : case 'm' : /* commands use/stats */
		for (mptr = msgtab; mptr->cmd; mptr++)
			if (mptr->count)
				sendto_one(cptr, replies[RPL_STATSCOMMANDS],
					   ME, BadTo(parv[0]), mptr->cmd,
					   mptr->count, mptr->bytes,
					   mptr->rcount);
		break;
	case 'o' : case 'O' : /* O (and o) lines */
		report_configured_links(cptr, parv[0], CONF_OPS);
		break;
	case 'p' : case 'P' : /* ircd ping stats */
		report_ping(sptr, parv[0]);
		break;
	case 'Q' : case 'q' : /* Q lines */
		report_configured_links(cptr,parv[0],CONF_QUARANTINED_SERVER);
		break;
	case 'R' : case 'r' : /* usage */
		send_usage(cptr, parv[0]);
		break;
	case 'S' : case 's' : /* S lines */
		report_configured_links(cptr, parv[0], CONF_SERVICE);
		break;
	case 'T' : case 't' : /* various statistics */
		tstats(cptr, parv[0]);
		break;
	case 'U' : case 'u' : /* uptime */
	    {
		register time_t now;

		now = timeofday - me.since;
		sendto_one(sptr, replies[RPL_STATSUPTIME], ME, BadTo(parv[0]),
			   now/86400, (now/3600)%24, (now/60)%60, now%60);
		break;
	    }
	case 'V' : case 'v' : /* V conf lines */
		report_configured_links(cptr, parv[0], CONF_VER);
		break;
	case 'X' : case 'x' : /* lists */
#ifdef	DEBUGMODE
		send_listinfo(cptr, parv[0]);
#endif
		break;
	case 'Y' : case 'y' : /* Y lines */
		report_classes(cptr, parv[0]);
		break;
	case 'Z' : 	      /* memory use (OPER only) */
		if (MyOper(sptr))
			count_memory(cptr, parv[0], 1);
		else
			sendto_one(sptr, replies[ERR_NOPRIVILEGES], ME, BadTo(parv[0]));
		break;
	case 'z' :	      /* memory use */
		count_memory(cptr, parv[0], 0);
		break;
	default :
		stat = '*';
		break;
	}
	sendto_one(cptr, replies[RPL_ENDOFSTATS], ME, BadTo(parv[0]), stat);
	return 2;
    }

/*
** m_users
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_users(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
#ifdef ENABLE_USERS
	char	namebuf[10],linebuf[10],hostbuf[17];
	int	fd, flag = 0;
#endif

	if (hunt_server(cptr,sptr,":%s USERS :%s",1,parc,parv) == HUNTED_ISME)
	    {
#ifdef ENABLE_USERS
		if ((fd = utmp_open()) == -1)
		    {
			sendto_one(sptr, replies[ERR_FILEERROR], ME, BadTo(parv[0]),
				   "open", UTMP);
			return 1;
		    }

		sendto_one(sptr, replies[RPL_USERSSTART], ME, BadTo(parv[0]));
		while (utmp_read(fd, namebuf, linebuf,
				 hostbuf, sizeof(hostbuf)) == 0)
		    {
			flag = 1;
			sendto_one(sptr, replies[RPL_USERS], ME, BadTo(parv[0]),
				   namebuf, linebuf, hostbuf);
		    }
		if (flag == 0) 
			sendto_one(sptr, replies[RPL_NOUSERS], ME, BadTo(parv[0]));

		sendto_one(sptr, replies[RPL_ENDOFUSERS], ME, BadTo(parv[0]));
		(void)utmp_close(fd);
#else
		sendto_one(sptr, replies[ERR_USERSDISABLED], ME, BadTo(parv[0]));
#endif
	    }
	else
		return 3;
	return 2;
}

/*
** Note: At least at protocol level ERROR has only one parameter,
** although this is called internally from other functions
** --msa
**
**	parv[0] = sender prefix
**	parv[*] = parameters
*/
int	m_error(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	Reg	char	*para;

	para = (parc > 1 && *parv[1] != '\0') ? parv[1] : "<>";

	Debug((DEBUG_ERROR,"Received ERROR message from %s: %s",
	      sptr->name, para));
	/*
	** Ignore error messages generated by normal user clients
	** (because ill-behaving user clients would flood opers
	** screen otherwise). Pass ERROR's from other sources to
	** the local operator...
	*/
	if (IsPerson(cptr) || IsUnknown(cptr) || IsService(cptr))
		return 2;
	if (cptr == sptr)
		sendto_flag(SCH_ERROR, "from %s -- %s",
			   get_client_name(cptr, FALSE), para);
	else
		sendto_flag(SCH_ERROR, "from %s via %s -- %s",
			   sptr->name, get_client_name(cptr,FALSE), para);
	return 2;
    }

/*
** m_help
**	parv[0] = sender prefix
*/
int	m_help(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	int i;

	for (i = 0; msgtab[i].cmd; i++)
	  sendto_one(sptr,":%s NOTICE %s :%s",
		     ME, parv[0], msgtab[i].cmd);
	return 2;
    }

/*
 * parv[0] = sender
 * parv[1] = host/server mask.
 * parv[2] = server to query
 */
int	 m_lusers(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	int		s_count = 0,	/* server */
			c_count = 0,	/* client (visible) */
			u_count = 0,	/* unknown */
			i_count = 0,	/* invisible client */
			o_count = 0,	/* operator */
			v_count = 0;	/* service */
	int		m_clients = 0,	/* my clients */
			m_servers = 0,	/* my server links */
			m_services = 0;	/* my services */
	
	if (parc > 2)
	{
		if (hunt_server(cptr, sptr, ":%s LUSERS %s :%s", 2, parc, parv)
		    != HUNTED_ISME)
			return 3;
	}

	if (parc > 1)
	{
		(void)collapse(parv[1]);
	}

	if (parc == 1 || (parv[1][0] == '*' && parv[1][1] == '\0'))
	{
		s_count = istat.is_serv;
		c_count = istat.is_user[0];
		i_count = istat.is_user[1];
		u_count = istat.is_unknown;
		o_count = istat.is_oper;
		v_count = istat.is_service;
		m_clients = istat.is_myclnt;
		m_servers = istat.is_myserv;
		m_services = istat.is_myservice;
	}
	else
	{	
		aClient 	*acptr;
        	aServer 	*asptr;
		aService 	*svcp;
		
		if ((acptr = find_client(parv[1], NULL)) && IsServer(acptr))
		{
			/* LUSERS <server> */
			s_count = 1;
			c_count = acptr->serv->usercnt[0];
			i_count = acptr->serv->usercnt[1];
			o_count = acptr->serv->usercnt[2];
			
			if (IsMe(acptr))
			{
				m_clients = istat.is_myclnt;
				m_servers = istat.is_myserv;
				m_services = istat.is_myservice;
				u_count = istat.is_unknown;

			}
		}
		else
		{
			/* LUSERS <mask> */
			for (asptr = svrtop; asptr; asptr = asptr->nexts)
			{
				if (!match(parv[1], asptr->bcptr->name))
				{
					s_count++;
					
					c_count += asptr->usercnt[0];
					i_count += asptr->usercnt[1];
					o_count += asptr->usercnt[2];
					
					if (IsMe(asptr->bcptr))
					{
						m_clients = istat.is_myclnt;
						m_servers = istat.is_myserv;
						m_services = istat.is_myservice;
						u_count = istat.is_unknown;

					}
				}
			}
		}
		/* Count services, but only if we found some matching server
		 * before (to prevent wrong matches on masks like *irc*).
		 */
		if (s_count)
		{
			for (svcp = svctop; svcp; svcp = svcp->nexts)
			{
				if (!match(parv[1], svcp->servp->bcptr->name))
				{
					v_count++;
				}
			}
		}
	}
	
#ifndef SHOW_INVISIBLE_LUSERS
	if (!IsOper(sptr))
	{
		i_count = 0;
	}
#endif
	sendto_one(sptr, replies[RPL_LUSERCLIENT], ME, BadTo(parv[0]),
		   c_count + i_count, v_count, s_count);
	if (o_count)
	{
		sendto_one(sptr, replies[RPL_LUSEROP], ME, BadTo(parv[0]),
			   o_count);
	}
	if (u_count)
	{
		sendto_one(sptr, replies[RPL_LUSERUNKNOWN], ME, parv[0],
			   u_count);
	}
	sendto_one(sptr, replies[RPL_LUSERCHANNELS], ME, BadTo(parv[0]),
		   istat.is_chan);
	
	sendto_one(sptr, replies[RPL_LUSERME], ME, BadTo(parv[0]), m_clients,
		   m_services, m_servers);
	return 2;
    }

  
/***********************************************************************
 * m_connect() - Added by Jto 11 Feb 1989
 ***********************************************************************/

/*
** m_connect
**	parv[0] = sender prefix
**	parv[1] = servername
**	parv[2] = port number
**	parv[3] = remote server
*/
int	m_connect(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	int	port, tmpport, retval;
	aConfItem *aconf;
	aClient *acptr;

	if (parc > 3 && IsLocOp(sptr))
	    {
		sendto_one(sptr, replies[ERR_NOPRIVILEGES], ME, BadTo(parv[0]));
		return 1;
	    }

	if (hunt_server(cptr,sptr,":%s CONNECT %s %s :%s",
		       3,parc,parv) != HUNTED_ISME)
		return 1;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, replies[ERR_NEEDMOREPARAMS], ME, BadTo(parv[0]),
			   "CONNECT");
		return 0;
	    }

	if ((acptr = find_name(parv[1], NULL))
	    || (acptr = find_mask(parv[1], NULL)))
	    {
		sendto_one(sptr, ":%s NOTICE %s :Connect: Server %s %s %s.",
			   ME, parv[0], parv[1], "already exists from",
			   acptr->from->name);
		return 0;
	    }

	for (aconf = conf; aconf; aconf = aconf->next)
		if ((aconf->status == CONF_CONNECT_SERVER ||
		     aconf->status == CONF_ZCONNECT_SERVER) &&
		    match(parv[1], aconf->name) == 0)
		  break;
	/* Checked first servernames, then try hostnames. */
	if (!aconf)
		for (aconf = conf; aconf; aconf = aconf->next)
			if ((aconf->status == CONF_CONNECT_SERVER ||
			     aconf->status == CONF_ZCONNECT_SERVER) &&
			    (match(parv[1], aconf->host) == 0 ||
			     match(parv[1], index(aconf->host, '@')+1) == 0))
		  		break;

	if (!aconf)
	    {
	      sendto_one(sptr,
			 "NOTICE %s :Connect: Host %s not listed in ircd.conf",
			 parv[0], parv[1]);
	      return 0;
	    }
	/*
	** Get port number from user, if given. If not specified,
	** use the default form configuration structure. If missing
	** from there, then use the precompiled default.
	*/
	tmpport = port = aconf->port;
	if (parc > 2)
	{
		port = atoi(parv[2]);
	}
	
	if (parc < 3 || !port)
	{
		if (tmpport < 0)
		{
			port = 0 - port;
		}
		else
		{
			port = tmpport;
		}

		if (!port)
		{
			sendto_one(sptr,
				":%s NOTICE %s :Connect: missing port number",
				ME, parv[0]);
			return 0;
		}
	}
	if (port < 0)
	{
		sendto_one(sptr, "NOTICE %s :Connect: Illegal port number",
				  parv[0]);
	}

	/*
	** Notify all operators about remote connect requests
	*/
	if (!IsAnOper(cptr))
	    {
		sendto_ops_butone(NULL, &me,
				  ":%s WALLOPS :Remote CONNECT %s %s from %s",
				   ME, parv[1], parv[2] ? parv[2] : "",
				   get_client_name(sptr,FALSE));
#if defined(USE_SYSLOG) && defined(SYSLOG_CONNECT)
		syslog(LOG_DEBUG, "CONNECT From %s : %s %d", parv[0],
		       parv[1], parv[2] ? parv[2] : "");
#endif
	    }
	aconf->port = port;
	switch (retval = connect_server(aconf, sptr, NULL))
	{
	case 0:
		sendto_one(sptr, ":%s NOTICE %s :*** Connecting to %s[%s].",
			   ME, parv[0], aconf->host, aconf->name);
		sendto_flag(SCH_NOTICE, "Connecting to %s[%s] by %s",
			    aconf->host, aconf->name,
			    get_client_name(sptr, FALSE));
		break;
	case -1:
		sendto_one(sptr, ":%s NOTICE %s :*** Couldn't connect to %s.",
			   ME, parv[0], aconf->host);
		sendto_flag(SCH_NOTICE, "Couldn't connect to %s by %s",
			    aconf->host, get_client_name(sptr, FALSE));
		break;
	case -2:
		sendto_one(sptr, ":%s NOTICE %s :*** Host %s is unknown.",
			   ME, parv[0], aconf->host);
		sendto_flag(SCH_NOTICE, "Connect by %s to unknown host %s",
			    get_client_name(sptr, FALSE), aconf->host);
		break;
	default:
		sendto_one(sptr,
			   ":%s NOTICE %s :*** Connection to %s failed: %s",
			   ME, parv[0], aconf->host, strerror(retval));
		sendto_flag(SCH_NOTICE, "Connection to %s by %s failed: %s",
			    aconf->host, get_client_name(sptr, FALSE),
			    strerror(retval));
	}
	aconf->port = tmpport;
	return 0;
    }

/*
** m_wallops (write to *all* opers currently online)
**	parv[0] = sender prefix
**	parv[1] = message text
*/
int	m_wallops(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	char	*message, *pv[4];

	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	    {
		sendto_one(sptr, replies[ERR_NEEDMOREPARAMS], ME, BadTo(parv[0]),
			   "WALLOPS");
		return 1;
	    }

	if (!IsServer(sptr))
	    {
		pv[0] = parv[0];
		pv[1] = "+wallops";
		pv[2] = message;
		pv[3] = NULL;
		return m_private(cptr, sptr, 3, pv);
	    }
	sendto_ops_butone(IsServer(cptr) ? cptr : NULL, sptr,
			":%s WALLOPS :%s", parv[0], message);
#ifdef	USE_SERVICES
	check_services_butone(SERVICE_WANT_WALLOP, NULL, sptr,
			      ":%s WALLOP :%s", parv[0], message);
#endif
	return 2;
    }

/*
** m_time
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_time(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	if (hunt_server(cptr,sptr,":%s TIME :%s",1,parc,parv) == HUNTED_ISME)
		sendto_one(sptr, replies[RPL_TIME], ME, BadTo(parv[0]), ME, date((long)0));
	return 2;
}


/*
** m_admin
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_admin(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	aConfItem *aconf;

	if (IsRegistered(cptr) &&	/* only local query for unregistered */
	    hunt_server(cptr,sptr,":%s ADMIN :%s",1,parc,parv) != HUNTED_ISME)
		return 3;
	if ((aconf = find_admin()) && aconf->host && aconf->passwd
	    && aconf->name)
	    {
		sendto_one(sptr, replies[RPL_ADMINME], ME, BadTo(parv[0]), ME);
		sendto_one(sptr, replies[RPL_ADMINLOC1], ME, BadTo(parv[0]), aconf->host);
		sendto_one(sptr, replies[RPL_ADMINLOC2], ME, BadTo(parv[0]),
			   aconf->passwd);
		sendto_one(sptr, replies[RPL_ADMINEMAIL], ME, BadTo(parv[0]),
			   aconf->name);
	    }
	else
		sendto_one(sptr, replies[ERR_NOADMININFO], ME, BadTo(parv[0]), ME);
	return 2;
    }

#if defined(OPER_REHASH) || defined(LOCOP_REHASH)
/*
** m_rehash
**
*/
int	m_rehash(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	sendto_one(sptr, replies[RPL_REHASHING], ME, BadTo(parv[0]),
		   mybasename(configfile));
	sendto_flag(SCH_NOTICE,
		    "%s is rehashing Server config file", parv[0]);
#ifdef USE_SYSLOG
	syslog(LOG_INFO, "REHASH From %s\n", get_client_name(sptr, FALSE));
#endif
	return rehash(cptr, sptr, (parc > 1) ? ((*parv[1] == 'q')?2:0) : 0);
}
#endif

#if defined(OPER_RESTART) || defined(LOCOP_RESTART)
/*
** m_restart
**
*/
int	m_restart(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg	aClient	*acptr;
	Reg	int	i;
	char	killer[HOSTLEN * 2 + USERLEN + 5];

	strcpy(killer, get_client_name(sptr, TRUE));
	SPRINTF(buf, "RESTART by %s", get_client_name(sptr, TRUE));
	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(acptr = local[i]))
			continue;
		if (IsClient(acptr) || IsService(acptr))
		    {
			sendto_one(acptr,
				   ":%s NOTICE %s :Server Restarting. %s",
				   ME, acptr->name, killer);
			acptr->exitc = EXITC_DIE;
			if (IsClient(acptr))
				exit_client(acptr, acptr, &me,
					    "Server Restarting");
			/* services are kept for logging purposes */
		    }
		else if (IsServer(acptr))
			sendto_one(acptr, ":%s ERROR :Restarted by %s",
				   ME, killer);
	    }
	flush_connections(me.fd);

	restart(buf);
	/*NOT REACHED*/
	return 0;
}
#endif

void trace_one(aClient *sptr, aClient *acptr)
{
	char *name;
	int class;
	char *to;

	to = HasUID(sptr) ? sptr->user->uid : sptr->name;
	
	name = get_client_name(acptr, FALSE);
	class = get_client_class(acptr);
	
	switch (acptr->status)
	{
                case STAT_CONNECTING:
                        sendto_one(sptr, replies[RPL_TRACECONNECTING],ME, to,
				   class, name);
                        break;
			
                case STAT_HANDSHAKE:
                        sendto_one(sptr, replies[RPL_TRACEHANDSHAKE], ME, to,
				   class, name);
                        break;
			
                case STAT_ME:
                        break;
			
		case STAT_UNKNOWN:
			sendto_one(sptr, replies[RPL_TRACEUNKNOWN], ME, to,
				   to, class, name);
			break;
			
		case STAT_CLIENT:
			if (IsAnOper(acptr))
			{
				sendto_one(sptr, replies[RPL_TRACEOPERATOR], ME,
					to, class, name);
			}
			else
			{
				sendto_one(sptr, replies[RPL_TRACEUSER], ME,
					to, class, name);
			}
			break;
			
                case STAT_SERVER:
		{
			/* we need to count servers/users behind this link */
			int servers = 0, users = 0;
			aServer *asptr;
#ifdef HUB
                        for (asptr = svrtop; asptr; asptr = asptr->nexts)
			{
				if (asptr->bcptr->from == acptr)
				{
					servers++;
					users += asptr->usercnt[0];
#ifdef SHOW_INVISIBLE_LUSERS
					users += asptr->usercnt[1];						
#endif
				}
			}
#else /* !HUB */
				/* we can have only one server linked */
				servers = istat.is_serv - 1;
				users = istat.is_user[0];
#ifdef SHOW_INVISIBLE_LUSERS
				users += istat.is_user[1];
				/* We are counting all remote users,
				 * therefore we need to substract our own
				 * users.
				 */
				users -= istat.is_myclnt;

#else
				users -= me.usercnt[0];
#endif
#endif /* HUB */
                        if (acptr->serv->user)
			{
                                sendto_one(sptr, replies[RPL_TRACESERVER], ME,
                                           to, class, servers,
                                           users, name, acptr->serv->by,
                                           acptr->serv->user->username,
                                           acptr->serv->user->host,
                                           acptr->serv->version,
                                           (acptr->flags & FLAGS_ZIP) ?
					   	"z" : "");
			}
                        else
			{
                                sendto_one(sptr, replies[RPL_TRACESERVER], ME,
                                           to, class, servers,
                                           users, name,
                                           *(acptr->serv->by) ?
                                           acptr->serv->by : "*", "*", ME,
                                           acptr->serv->version,
                                           (acptr->flags & FLAGS_ZIP) ?
					   	"z" : "");
			}
			break;
		}
		
		case STAT_SERVICE:
                        sendto_one(sptr, replies[RPL_TRACESERVICE], ME, to,
				   class, name,
				   acptr->service->type, acptr->service->wants);
                        break;

                case STAT_LOG:
                        sendto_one(sptr, replies[RPL_TRACELOG], ME, to, ME,
                                   acptr->port);
                        break;

                default: /* ...we actually shouldn't come here... --msa */
                        sendto_one(sptr, replies[RPL_TRACENEWTYPE], ME, to,
				   name);
                        break;
	}
}

/*
 * m_trace
 * 	parv[0] = sender prefix
 * 	parv[1] = traced nick/server/service
 */
int	m_trace(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	int maskedserv = 0;
	int i = 0;
	
	if (parc > 1)
	{
		/* wildcards now allowed only in server/service names */
		acptr = find_matching_client(parv[1]);
		if (!acptr)
		{
			sendto_one(sptr, replies[ERR_NOSUCHSERVER],
					ME, BadTo(parv[0]), parv[1]);
			return 1;
		}
		if (IsServer(acptr))
		{
			if (!match(acptr->name, parv[1]))
			{
				/* if we are tracing masked server,
				 * we have to send parv[1], not acptr->name
				 */
				maskedserv = 1;
			}
		}
	}
	else
	{
		acptr = &me;
	}
	
	if (!IsMe(acptr))
	{
		if (!MyConnect(acptr) || IsServer(acptr))
		{
			if (acptr->from == cptr)
			{	/* eek ?! */
				return 1;
			}
			/* passthru */
              		sendto_one(sptr, replies[RPL_TRACELINK], ME,
				   BadTo(parv[0]), version, debugmode,
				   (maskedserv) ? parv[1] : acptr->name,
				   acptr->from->name,
				   acptr->from->serv->version,
				   (acptr->from->flags & FLAGS_ZIP) ? "z" : "",
        	               	   timeofday - acptr->from->firsttime,
                	           (int)DBufLength(&acptr->from->sendQ),
                        	   (int)DBufLength(&sptr->from->sendQ));

			sendto_one(acptr, ":%s TRACE :%s", sptr->name,
				(maskedserv) ? parv[1] : acptr->name);
			return 5;
		}
		else
		{
			/* tracing something local */
			trace_one(sptr, acptr);	
		}
	}
	else
	{
		/* report everything */
		aClient *a2cptr;
		
		for (i = 0; i <= highest_fd; i++)
		{
			if (!(a2cptr = local[i]))
			{
				continue;
			}
			if (IsMe(a2cptr))
			{
				continue;
			}
			if (IsPerson(a2cptr)	    /* if client about to
						     * trace is person */
			    && !(a2cptr == sptr)    /* but not user self */
			    && !(IsAnOper(a2cptr))  /* nor some oper */
			    && !(IsAnOper(sptr) && MyConnect(sptr))
						    /* nor it is my oper
						     * doing trace */
			   )
			{
				continue;   /* then don't show the client */
			}
			
			/* Report unknown connections to local users
			 * and remote opers with +w set */
			if (IsUnknown(a2cptr)
			    && !((IsAnOper(sptr) || MyClient(sptr))
				 && SendWallops(sptr)
				)
			    )
			{
				continue;
			}
			trace_one(sptr, a2cptr);	
		}
		
		/* Report Class usage */
		if (IsPerson(sptr) && SendWallops(sptr))
		{
			aClass  *tmp;
		    	for (tmp = FirstClass(); tmp; tmp = NextClass(tmp))
		    	{
				if (Links(tmp) > 0)
				{
					sendto_one(sptr,
						   replies[RPL_TRACECLASS],
						   ME, BadTo(parv[0]),
					   	   Class(tmp), Links(tmp));
				}
	   		}
		}
	}
	sendto_one(sptr, replies[RPL_TRACEEND], ME, BadTo(parv[0]),
		   acptr->name, version, debugmode);
	
	return 2;
}

/*
** m_motd
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_motd(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
#ifdef	CACHED_MOTD
	register aMotd *temp;
	struct tm *tm;
#else
	int	fd;
	char	line[80];
	Reg	char	 *tmp;
	struct	stat	sb;
	struct	tm	*tm;
#endif

	if (check_link(cptr))
	    {
		sendto_one(sptr, replies[RPL_TRYAGAIN], ME, BadTo(parv[0]), "MOTD");
		return 5;
	    }
	if (hunt_server(cptr, sptr, ":%s MOTD :%s", 1,parc,parv)!=HUNTED_ISME)
		return 5;
#ifdef CACHED_MOTD
	tm = &motd_tm;
	if (motd == NULL)
	    {
		sendto_one(sptr, replies[ERR_NOMOTD], ME, BadTo(parv[0]));
		return 1;
	    }
	sendto_one(sptr, replies[RPL_MOTDSTART], ME, BadTo(parv[0]), ME);
	sendto_one(sptr, ":%s %d %s :- %d/%d/%d %d:%02d", ME, RPL_MOTD,
		   parv[0], tm->tm_mday, tm->tm_mon + 1, 1900 + tm->tm_year,
		   tm->tm_hour, tm->tm_min);
	temp = motd;
	for(temp=motd;temp != NULL;temp = temp->next)
		sendto_one(sptr, replies[RPL_MOTD], ME, BadTo(parv[0]), temp->line);
	sendto_one(sptr, replies[RPL_ENDOFMOTD], ME, BadTo(parv[0]));
	return 2;
#else
	/*
	 * stop NFS hangs...most systems should be able to open a file in
	 * 3 seconds. -avalon (curtesy of wumpus)
	 */
	(void)alarm(3);
	fd = open(IRCDMOTD_PATH, O_RDONLY);
	(void)alarm(0);
	if (fd == -1)
	    {
		sendto_one(sptr, replies[ERR_NOMOTD], ME, BadTo(parv[0]));
		return 1;
	    }
	(void)fstat(fd, &sb);
	sendto_one(sptr, replies[RPL_MOTDSTART], ME, BadTo(parv[0]), ME);
	tm = localtime(&sb.st_mtime);
	sendto_one(sptr, ":%s %d %s :- %d/%d/%d %d:%02d", ME, RPL_MOTD,
		   parv[0], tm->tm_mday, tm->tm_mon + 1, 1900 + tm->tm_year,
		   tm->tm_hour, tm->tm_min);
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	while (dgets(fd, line, sizeof(line)-1) > 0)
	    {
		if ((tmp = (char *)index(line,'\n')))
			*tmp = '\0';
		if ((tmp = (char *)index(line,'\r')))
			*tmp = '\0';
		sendto_one(sptr, replies[RPL_MOTD], ME, BadTo(parv[0]), line);
	    }
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	sendto_one(sptr, replies[RPL_ENDOFMOTD], ME, BadTo(parv[0]));
	(void)close(fd);
	return 2;
#endif	/* CACHED_MOTD */
}

/*
** m_close - added by Darren Reed Jul 13 1992.
*/
int	m_close(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg	aClient	*acptr;
	Reg	int	i;
	int	closed = 0;

	for (i = highest_fd; i; i--)
	    {
		if (!(acptr = local[i]))
			continue;
		if (!IsUnknown(acptr) && !IsConnecting(acptr) &&
		    !IsHandshake(acptr))
			continue;
		sendto_one(sptr, replies[RPL_CLOSING], ME, BadTo(parv[0]),
			   get_client_name(acptr, TRUE), acptr->status);
		(void)exit_client(acptr, acptr, &me, "Oper Closing");
		closed++;
	    }
	sendto_one(sptr, replies[RPL_CLOSEEND], ME, BadTo(parv[0]), closed);
	return 1;
}

/* End Of Burst command
** parv[0] - server sending the SQUIT
** parv[1] - optional comma separated list of servers for which this EOB
**           is also valid.
*/
int	m_eob(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	char eobbuf[BUFSIZE];
	char *e;	
	char *sid;
	char *p = NULL;
	int eobmaxlen;
	aClient *acptr;

	if (!IsServer(sptr))
	{
		return 0;
	}
	
	if (IsBursting(sptr))
	{
		if (MyConnect(sptr))
		{
			sendto_flag(SCH_NOTICE,
			       "End of burst from %s after %d seconds.",
			       sptr->name, timeofday - sptr->firsttime);
		}
		else
		{
			if (!IsMasked(sptr))
			{
				sendto_flag(SCH_NOTICE, "End of burst from %s",
					sptr->name);
			}
		}
		SetEOB(sptr);
		istat.is_eobservers++;
		
		if (parc < 2)
		{
			sendto_serv_v(sptr, SV_UID, ":%s EOB", sptr->serv->sid);
			return 1;
		}
	
	}
	else
	{
		if (parc < 2)
		{
			sendto_flag(SCH_ERROR,
				"Received another EOB for server %s (%s)",
				 sid, sptr->name);
			return 1;
		}
	}
	
	eobmaxlen = BUFSIZE
		- 1             /*     ":"     */
		- SIDLEN        /*   my SID    */
		- 6             /*  " EOB :"   */
		- 2;            /*   "\r\n"    */

	/* space for last comma and SID (calculation moved
	 * from "if (e - eobbuf > eobmaxlen)" inside following loop)
	 */
	eobmaxlen -= SIDLEN + 1;

	e = eobbuf;
	
	for (; (sid = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	{
		acptr = NULL;
		
		if (sid[0] == '$')
		{
			aServer *asptr;
			/* Fake sid (comes from EOB emulation */
			asptr = find_tokserver(idtol(sid + 1, SIDLEN - 1),
				cptr, NULL);
			if (asptr)
			{
				acptr = asptr->bcptr;
			}
		}
		else
		{
			acptr = find_sid(sid, NULL);
		}
		if (!acptr)
		{
			sendto_flag(SCH_SERVER,
				"Received EOB for unknown SID (%s) from (%s), "
				"dropping link",
				sid, cptr->name);
			return exit_client(cptr, cptr, &me,
				  "Unknown SID in EOB");
		}
		
		if (acptr->from != cptr)
		{
			sendto_flag(SCH_NOTICE,
				"Received EOB for (%s) from wrong direction "
				"(%s != %s), dropping link",
				sid, sptr->name, acptr->from->name);
			return exit_client(cptr, cptr, &me,
				"Wrong EOB Direction");
		}
		
		if (!IsBursting(acptr))
		{
			sendto_flag(SCH_ERROR,
				"Received another EOB for server %s (%s) from "
				"%s (%s) via %s", sid, acptr->name,
				sptr->serv->sid, sptr->name, cptr->name);

			continue;
		}

	
		sendto_flag(SCH_DEBUG, "Received EOB for %s (mass)",
			 acptr->serv->sid);

		/* SIDLEN + 1 = ",SID" */
		if (((int) (e - eobbuf)) > eobmaxlen)
		{
			*e = '\0';
			/* We have exceeded available space in buf.
			 * eobbuf always starts with comma, * so +1 gets
			 * rid of it */
			sendto_serv_v(sptr, SV_UID, ":%s EOB :%s",
					sptr->serv->sid, eobbuf + 1);
			e = eobbuf;
		}
		
		SetEOB(acptr);
		istat.is_eobservers++;
		/* regenerate buffer - it might be fake sid,
		** which will change at this point - jv
		*/
		*e++ = ',';
		memcpy(e, acptr->serv->sid, SIDLEN);
		e += SIDLEN;

	}
	
	/* Send the rest, if any */
	if (e > eobbuf)
	{
		*e = '\0';
		sendto_serv_v(sptr, SV_UID, ":%s EOB :%s",
			sptr->serv->sid, eobbuf + 1);
	}
	else
	{
		sendto_serv_v(sptr, SV_UID, ":%s EOB");
	}
	
	return 1;
}
#if defined(OPER_DIE) || defined(LOCOP_DIE)
int	m_die(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg	aClient	*acptr;
	Reg	int	i;
	char	killer[HOSTLEN * 2 + USERLEN + 5];

	strcpy(killer, get_client_name(sptr, TRUE));
	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(acptr = local[i]))
			continue;
		if (IsClient(acptr) || IsService(acptr))
		    {
			sendto_one(acptr,
				   ":%s NOTICE %s :Server Terminating. %s",
				   ME, acptr->name, killer);
			acptr->exitc = EXITC_DIE;
			if (IsClient(acptr))
				exit_client(acptr, acptr, &me, "Server died");
			/* services are kept for logging purposes */
		    }
		else if (IsServer(acptr))
			sendto_one(acptr, ":%s ERROR :Terminated by %s",
				   ME, killer);
	    }
	flush_connections(me.fd);
	(void)s_die(0);
	return 0;
}
#endif

/*
** storing server names in User structures is a real waste,
** the following functions change it to only store a pointer.
** A better way might be to store in Server structure and use servp. -krys
*/

static char	**server_name = NULL;
static int	server_max = 0, server_num = 0;

/*
** find_server_string
**
** Given an index, this will return a pointer to the corresponding
** (already allocated) string
*/
char *
find_server_string(snum)
int snum;
{
	if (snum < server_num && snum >= 0)
		return server_name[snum];
	/* request for a bogus snum value, something is wrong */
	sendto_flag(SCH_ERROR, "invalid index for server_name[] : %d (%d,%d)",
		    snum, server_num, server_max);
	return NULL;
}

/*
** find_server_num
**
** Given a server name, this will return the index of the corresponding
** string. This index can be used with find_server_name_from_num().
** If the string doesn't exist already, it will be allocated.
*/
int
find_server_num(sname)
char *sname;
{
	Reg int i = 0;

	while (i < server_num)
	    {
		if (!strcasecmp(server_name[i], sname))
			break;
		i++;
	    }
	if (i < server_num)
		return i;
	if (i == server_max)
	  {
	    /* server_name[] array is full, let's make it bigger! */
	    if (server_name)
		    server_name = (char **) MyRealloc((char *)server_name,
					      sizeof(char *)*(server_max+=50));
	    else
		    server_name = (char **) MyMalloc(sizeof(char *)*(server_max=50));
	  }
	server_name[server_num] = mystrdup(sname);
	return server_num++;
}

/*
** check_link (added 97/12 to prevent abuse)
**	routine which tries to find out how healthy a link is.
**	useful to know if more strain may be imposed on the link or not.
**
**	returns 0 if link load is light, -1 otherwise.
*/
static int
check_link(cptr)
aClient	*cptr;
{
    if (!IsServer(cptr))
	    return 0;
    if (!(bootopt & BOOT_PROT))
	    return 0;

    ircstp->is_ckl++;
    if ((int)DBufLength(&cptr->sendQ) > 65536) /* SendQ is already (too) high*/
	{
	    cptr->serv->lastload = timeofday;
	    ircstp->is_cklQ++;
	    return -1;
	}
    if (timeofday - cptr->firsttime < 60) /* link is too young */
	{
	    ircstp->is_ckly++;
	    return -1;
	}
    if (timeofday - cptr->serv->lastload > 30)
	    /* last request more than 30 seconds ago => OK */
	{
	    cptr->serv->lastload = timeofday;
	    ircstp->is_cklok++;
	    return 0;
	}
    if (timeofday - cptr->serv->lastload > 15
	&& (int)DBufLength(&cptr->sendQ) < CHREPLLEN)
	    /* last request between 15 and 30 seconds ago, but little SendQ */
	{
	    cptr->serv->lastload = timeofday;
	    ircstp->is_cklq++;
	    return 0;
	}
    ircstp->is_cklno++;
    return -1;
}

/*
** check_servername
** 	Simple check for validity of server name.
** Hostlen, valid chars and whether it contains at least one char
** and one dot (dot in servername equals no collision between
** servername and nickname (no more ultimate jupes ;>)). --Beeth
**
** Returns 0 if ok, all else is some kind of error, which serves
** as index in check_servername_errors[] table.
*/
int check_servername(hostname)
char *hostname;
{
	register char *ch;
	int dots, chars, rc;

	dots = 0;
	chars = 0;
	rc = 0;

	if (strlen(hostname) > HOSTLEN)
	{
		rc = 1;
	}
	else
	{
		for (ch = hostname; *ch; ch++)
		{
			if (*ch == '.')
			{
				dots++;
				continue;
			}
			if ((*ch >= 'a' && *ch <= 'z')
				|| (*ch >= 'A' && *ch <= 'Z')
				|| *ch == '-' || *ch == '*')
			{
				chars++;
				continue;
			}
			if (isdigit(*ch))
			{
				continue;
			}
			/* all else is invalid as servername! */
			rc = 2;
			break;
		}
		if (!rc && !dots)
		{
			/* not a single dot? not allowed. */
			rc = 3;
		}
		if (!rc && !chars)
		{
			/* no chars? not allowed. */
			rc = 2;
		}
	}

	return rc;
}

/*
** adds server to server tree
*/
void	add_server_to_tree(aClient *acptr)
{
	acptr->serv->up->serv->servers++;

	if (acptr->serv->up->serv->down)
	{
		acptr->serv->up->serv->down->serv->left = acptr;
	}

	acptr->serv->right = acptr->serv->up->serv->down;
	acptr->serv->up->serv->down = acptr;

	return;
}

/* 
** Removes a server from the server tree.
*/
void	remove_server_from_tree(aClient *cptr)
{
	cptr->serv->up->serv->servers--;

	if (cptr->serv->down)
	{
		/* FIXME - debug abort(). this must be *never* true
		* at this point - jv */
		abort();
	}

	if (cptr->serv->right)
	{
		cptr->serv->right->serv->left = cptr->serv->left;
	}

	if (cptr->serv->left)
	{
		cptr->serv->left->serv->right = cptr->serv->right;
	}

	if (cptr == cptr->serv->up->serv->down)
	{
		cptr->serv->up->serv->down = cptr->serv->right;
	}

	return;
}

/* Process emulated EOB */
void do_emulated_eob(aClient *sptr)
{
	aClient *acptr = sptr;

	SetEOB(sptr);

	if (IsBursting(sptr))
	{
		istat.is_eobservers++;
	}
	
	if (MyConnect(sptr))
	{
		sendto_flag(SCH_SERVER,
			"End of burst from %s after %d seconds.",
			sptr->name, timeofday - sptr->firsttime);
	}
	else
	{
		sendto_flag(SCH_DEBUG, "EOB from %s (PONG)", sptr->name);
	}
	sendto_serv_v(sptr,SV_UID,":%s EOB :%s", me.serv->sid,
		      sptr->serv->sid);

	/* Try to bubble EOB up */
	while ((acptr = acptr->serv->up) && acptr != &me)
	{
		if (IsBursting(acptr))
		{
			SetEOB(acptr);
			istat.is_eobservers++;
			sendto_flag(SCH_DEBUG, "EOB from %s (bubbled)",
				acptr->name);
				
			sendto_serv_v(sptr, SV_UID, ":%s EOB :%s",
				me.serv->sid, sptr->serv->sid);

		}
	}
	return;
}
