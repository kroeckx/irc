/************************************************************************
 *   IRC - Internet Relay Chat, common/parse.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

/* -- Jto -- 03 Jun 1990
 * Changed the order of defines...
 */

#ifndef lint
static  char sccsid[] = "%W% %G% (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif
#include "struct.h"
#include "common.h"
#define MSGTAB
#include "msg.h"
#undef MSGTAB
#include "sys.h"
#include "numeric.h"
#include "h.h"

/*
 * NOTE: parse() should not be called recursively by other functions!
 */
static	char	*para[MAXPARA+1];

#ifdef	CLIENT_COMPILE
static	char	sender[NICKLEN+USERLEN+HOSTLEN+3];
char	userhost[USERLEN+HOSTLEN+2];
#define	timeofday	time(NULL)
#else
static	char	sender[HOSTLEN+1];
static	int	cancel_clients __P((aClient *, aClient *, char *));
static	void	remove_unknown __P((aClient *, char *));
#endif

/*
**  Find a client (server or user) by name.
**
**  *Note*
**	Semantics of this function has been changed from
**	the old. 'name' is now assumed to be a null terminated
**	string and the search is the for server and user.
*/
#ifndef CLIENT_COMPILE
aClient *find_client(name, cptr)
char	*name;
Reg	aClient *cptr;
    {
	aClient *acptr = cptr;

	if (name && *name)
		acptr = hash_find_client(name, cptr);

	return acptr;
    }

aClient *find_service(name, cptr)
char	*name;
Reg	aClient *cptr;
    {
	aClient *acptr = cptr;
	char	*serv;

	if (name && *name)
		if ((serv = (char *)index(name, '@'))) {
			if (*name == '@' || !*(serv+1))	/* NULL@ or @NULL */
				return acptr;
			*serv++ = '\0';
			acptr = hash_find_server(serv, cptr);
			if (acptr && IsMe(acptr))
				acptr = hash_find_client(name, cptr);
			*--serv = '@';
		} else
			acptr = hash_find_client(name, cptr);
	return acptr;
    }

/* Obsoleted as of 2.9.3/970114/Vesa by find_service() */
aClient	*find_nickserv(name, cptr)
char	*name;
Reg	aClient *cptr;
    {
	aClient *acptr = cptr;

	if (name && *name)
		acptr = hash_find_nickserv(name, cptr);

	return acptr;
    }

#else
aClient *find_client(name, cptr)
char *name;
aClient *cptr;
    {
	Reg	aClient	*c2ptr = cptr;

	if (!name || !*name)
		return c2ptr;

	for (c2ptr = client; c2ptr; c2ptr = c2ptr->next) 
		if (mycmp(name, c2ptr->name) == 0)
			return c2ptr;
	return cptr;
    }
#endif

/*
**  Find a user@host (server or user).
**
**  *Note*
**	Semantics of this function has been changed from
**	the old. 'name' is now assumed to be a null terminated
**	string and the search is the for server and user.
*/
aClient *find_userhost(user, host, cptr, count)
char	*user, *host;
aClient *cptr;
int	*count;
    {
	Reg	aClient	*c2ptr;
	Reg	aClient	*res = cptr;

	*count = 0;
	if (user)
		for (c2ptr = client; c2ptr; c2ptr = c2ptr->next) 
		    {
			if (!MyClient(c2ptr)) /* implies mine and a user */
				continue;
			if ((!host || !matches(host, c2ptr->user->host)) &&
			     mycmp(user, c2ptr->user->username) == 0)
			    {
				(*count)++;
				res = c2ptr;
			    }
		    }
	return res;
    }

/*
**  Find server by name.
**
**	This implementation assumes that server and user names
**	are unique, no user can have a server name and vice versa.
**	One should maintain separate lists for users and servers,
**	if this restriction is removed.
**
**  *Note*
**	Semantics of this function has been changed from
**	the old. 'name' is now assumed to be a null terminated
**	string.
*/
#ifndef CLIENT_COMPILE
/*
** Find a server from hash table, given its name
*/
aClient *find_server(name, cptr)
char	*name;
Reg	aClient *cptr;
{
	aClient *acptr = cptr;

	if (name && *name)
		acptr = hash_find_server(name, cptr);
	return acptr;
}

/*
** Given a server name, find the server mask behind which the server
** is hidden.
*/
aClient *find_mask(name, cptr)
char	*name;
aClient *cptr;
{
	static	char	servermask[HOSTLEN+1];
	Reg	aClient	*c2ptr = cptr;
	Reg	char	*mask = servermask;

	if (!name || !*name)
		return c2ptr;
	if ((c2ptr = hash_find_server(name, cptr)))
		return (c2ptr);
	if (index(name, '*'))
		return c2ptr;
	strcpy (servermask, name);
	while (*mask)
	{
		if (*(mask+1) == '.')
		{
			*mask = '*';
			if ((c2ptr = hash_find_server(mask, cptr)))
				return (c2ptr);
		}
		mask++;
	}
	return (c2ptr ? c2ptr : cptr);
}

/*
** Find a server from hash table, given its token
*/
aServer	*find_tokserver(token, cptr, c2ptr)
int	token;
aClient	*cptr, *c2ptr;
{
	return hash_find_stoken(token, cptr, c2ptr);
}

/*
** Find a server, given its name (which might contain *'s, in which case
** the first match will be return [not the best one])
*/
aClient *find_name(name, cptr)
char	*name;
aClient *cptr;
{
	Reg	aClient	*c2ptr = cptr;
	Reg	aServer	*sp = NULL;

	if (!name || !*name)
		return c2ptr;

	if ((c2ptr = hash_find_server(name, cptr)))
		return (c2ptr);
	if (!index(name, '*'))
		return c2ptr;
	for (sp = svrtop; sp; sp = sp->nexts)
	    {
		/*
		** A server present in the list necessarily has a non NULL
		** bcptr pointer.
		*/
		if (matches(name, sp->bcptr->name) == 0)
			break;
		if (index(sp->bcptr->name, '*'))
			if (matches(sp->bcptr->name, name) == 0)
					break;
	    }
	return (sp ? sp->bcptr : cptr);
}
#else
aClient	*find_server(name, cptr)
char	*name;
aClient	*cptr;
{
	Reg	aClient *c2ptr = cptr;

	if (!name || !*name)
		return c2ptr;

	for (c2ptr = client; c2ptr; c2ptr = c2ptr->next)
	    {
		if (!IsServer(c2ptr) && !IsMe(c2ptr))
			continue;
		if (matches(c2ptr->name, name) == 0 ||
		    matches(name, c2ptr->name) == 0)
			break;
	    }
	return (c2ptr ? c2ptr : cptr);
}
#endif /* CLIENT_COMPILE */

/*
**  Find person by (nick)name.
*/
aClient *find_person(name, cptr)
char	*name;
aClient *cptr;
    {
	Reg	aClient	*c2ptr = cptr;

	c2ptr = find_client(name, c2ptr);

	if (c2ptr && IsClient(c2ptr) && c2ptr->user)
		return c2ptr;
	else
		return cptr;
    }

/*
 * parse a buffer.
 * Return values:
 *  errors: -3 for unknown origin/sender, -2 for FLUSH_BUFFER, -1 for bad cptr
 *
 * NOTE: parse() should not be called recusively by any other fucntions!
 */
int	parse(cptr, buffer, bufend)
aClient *cptr;
char	*buffer, *bufend;
    {
	Reg	aClient *from = cptr;
	Reg	char	*ch, *s;
	Reg	int	len, i, numeric = 0, paramcount;
	Reg	struct	Message *mptr = NULL;
	int	ret;

#ifndef	CLIENT_COMPILE
	Debug((DEBUG_DEBUG, "Parsing %s: %s",
		get_client_name(cptr, FALSE), buffer));
	if (IsDead(cptr))
		return -1;
#endif

	s = sender;
	*s = '\0';
	for (ch = buffer; *ch == ' '; ch++)
		;
	para[0] = from->name;
	if (*ch == ':')
	    {
		/*
		** Copy the prefix to 'sender' assuming it terminates
		** with SPACE (or NULL, which is an error, though).
		*/
		for (++ch, i = 0; *ch && *ch != ' '; ++ch )
			if (s < (sender + sizeof(sender)-1))
				*s++ = *ch; /* leave room for NULL */
		*s = '\0';
#ifdef CLIENT_COMPILE
		if ((s = index(sender, '!')))
		    {
			*s++ = '\0';
			strncpyzt(userhost, s, sizeof(userhost));
		    }
		else if ((s = index(sender, '@')))
		    {
			*s++ = '\0';
			strncpyzt(userhost, s, sizeof(userhost));
		    }
#endif
		/*
		** Actually, only messages coming from servers can have
		** the prefix--prefix silently ignored, if coming from
		** a user client...
		**
		** ...sigh, the current release "v2.2PL1" generates also
		** null prefixes, at least to NOTIFY messages (e.g. it
		** puts "sptr->nickname" as prefix from server structures
		** where it's null--the following will handle this case
		** as "no prefix" at all --msa  (": NOTICE nick ...")
		*/
		if (*sender && IsServer(cptr))
		    {
 			from = find_client(sender, (aClient *) NULL);
			if (!from || matches(from->name, sender))
				from = find_server(sender, (aClient *)NULL);
#ifndef	CLIENT_COMPILE
			/* Is there svc@server prefix ever? -Vesa */
			else if (!from && index(sender, '@'))
				from = find_service(sender, (aClient *)NULL);
			if (!from)
				from = find_mask(sender, (aClient *) NULL);
#endif

			para[0] = sender;

			/* Hmm! If the client corresponding to the
			 * prefix is not found--what is the correct
			 * action??? Now, I will ignore the message
			 * (old IRC just let it through as if the
			 * prefix just wasn't there...) --msa
			 * Since 2.9 we pick them up and .. --Vesa
			 */
			if (!from)
			    {
				Debug((DEBUG_ERROR,
					"Unknown prefix (%s)(%s) from (%s)",
					sender, buffer, cptr->name));
				ircstp->is_unpf++;
#ifndef	CLIENT_COMPILE
				remove_unknown(cptr, sender);
#endif
				return -3;	/* Grab it in read_message() */
			    }
			if (from->from != cptr)
			    {
				ircstp->is_wrdi++;
				Debug((DEBUG_ERROR,
					"Message (%s) coming from (%s)",
					buffer, cptr->name));
#ifndef	CLIENT_COMPILE
				return cancel_clients(cptr, from, buffer);
#else
				return -1;
#endif
			    }
		    }
		while (*ch == ' ')
			ch++;
	    }
	if (*ch == '\0')
	    {
		ircstp->is_empt++;
		Debug((DEBUG_NOTICE, "Empty message from host %s:%s",
		      cptr->name, from->name));
		return -1;
	    }
	/*
	** Extract the command code from the packet.  Point s to the end
	** of the command code and calculate the length using pointer
	** arithmetic.  Note: only need length for numerics and *all*
	** numerics must have paramters and thus a space after the command
	** code. -avalon
	*/
	s = (char *)index(ch, ' '); /* s -> End of the command code */
	len = (s) ? (s - ch) : 0;
	if (len == 3 &&
	    isdigit(*ch) && isdigit(*(ch + 1)) && isdigit(*(ch + 2)))
	    {
		numeric = (*ch - '0') * 100 + (*(ch + 1) - '0') * 10
			+ (*(ch + 2) - '0');
		paramcount = MAXPARA;
		ircstp->is_num++;
	    }
	else
	    {
		if (s)
			*s++ = '\0';
		for (mptr = msgtab; mptr->cmd; mptr++) 
			if (mycmp(mptr->cmd, ch) == 0)
				break;

		if (!mptr->cmd)
		    {
			/*
			** Note: Give error message *only* to recognized
			** persons. It's a nightmare situation to have
			** two programs sending "Unknown command"'s or
			** equivalent to each other at full blast....
			** If it has got to person state, it at least
			** seems to be well behaving. Perhaps this message
			** should never be generated, though...  --msa
			** Hm, when is the buffer empty -- if a command
			** code has been found ?? -Armin
			*/
			if (buffer[0] != '\0')
			    {
				cptr->flags |= FLAGS_UNKCMD;
				if (IsPerson(from))
					sendto_one(from,
					    ":%s %d %s %s :Unknown command",
					    me.name, ERR_UNKNOWNCOMMAND,
					    from->name, ch);
#ifdef	CLIENT_COMPILE
				Debug((DEBUG_ERROR,"Unknown (%s) from %s[%s]",
					ch, cptr->name, cptr->sockhost));
#else
				else if (IsServer(cptr))
					sendto_flag(SCH_ERROR,
					    "Unknown command from %s:%s",
					    get_client_name(cptr, TRUE), ch);
				Debug((DEBUG_ERROR,"Unknown (%s) from %s",
					ch, get_client_name(cptr, TRUE)));
#endif
			    }
			ircstp->is_unco++;
			return -1;
		    }
		paramcount = mptr->parameters;
		i = bufend - ((s) ? s : ch);
		mptr->bytes += i;
#ifndef	CLIENT_COMPILE
		if ((mptr->flags & MSG_LAG) &&
		    !(IsServer(cptr) || IsService(cptr)))
		    {	/* Flood control partly migrated into penalty */
			cptr->since += (1 + i / 100);
			/* Allow only 1 msg per 2 seconds
			 * (on average) to prevent dumping.
			 * to keep the response rate up,
			 * bursts of up to 5 msgs are allowed
			 * -SRB
			 */
			if (mptr->func != m_ison && mptr->func != m_mode)
				cptr->ract += (2 + i /120);
		    }
#endif
	    }
	/*
	** Must the following loop really be so devious? On
	** surface it splits the message to parameters from
	** blank spaces. But, if paramcount has been reached,
	** the rest of the message goes into this last parameter
	** (about same effect as ":" has...) --msa
	*/

	/* Note initially true: s==NULL || *(s-1) == '\0' !! */

#ifdef	CLIENT_COMPILE
	if (me.user)
		para[0] = sender;
#endif
	i = 0;
	if (s)
	    {
		if (paramcount > MAXPARA)
			paramcount = MAXPARA;
		for (;;)
		    {
			/*
			** Never "FRANCE " again!! ;-) Clean
			** out *all* blanks.. --msa
			*/
			while (*s == ' ')
				*s++ = '\0';

			if (*s == '\0')
				break;
			if (*s == ':')
			    {
				/*
				** The rest is single parameter--can
				** include blanks also.
				*/
				para[++i] = s + 1;
				break;
			    }
			para[++i] = s;
			if (i >= paramcount)
				break;
			for (; *s != ' ' && *s; s++)
				;
		    }
	    }
	para[++i] = NULL;
	if (mptr == NULL)
		return (do_numeric(numeric, cptr, from, i, para));
	mptr->count++;
	if (IsRegisteredUser(cptr) &&
#ifdef	IDLE_FROM_MSG
	    mptr->func == m_private)
#else
	    mptr->func != m_ping && mptr->func != m_pong)
#endif
		from->user->last = timeofday;
	Debug((DEBUG_DEBUG, "Function: %#x = %s parc %d parv %#x",
		mptr->func, mptr->cmd, i, para));
#ifndef	CLIENT_COMPILE
/* replaced by penalty
	if ((mptr->flags & MSG_PP) && !(IsServer(cptr) || IsService(cptr)) &&
	    i > 2)
		cptr->since += (i - 2);
*/
	if ((mptr->flags & MSG_REGU) && check_registered_user(from))
		return -1;
	if ((mptr->flags & MSG_SVC) && check_registered_service(from))
		return -1;
	if ((mptr->flags & MSG_REG) && check_registered(from))
		return -1;
	if ((mptr->flags & MSG_NOU) && MyPerson(from))
	    {
		sendto_one(from, err_str(ERR_ALREADYREGISTRED, para[0]));
		return-1;
	    }
	if (MyConnect(from) && !IsPrivileged(from) &&
	    (mptr->flags & (MSG_LOP|MSG_OP)))
	    {
		sendto_one(from, err_str(ERR_NOPRIVILEGES, para[0]));
		return -1;
	    }
#endif
	/*
	** ALL m_functions return now UNIFORMLY:
	**   -2  old FLUSH_BUFFER return value (unchanged).
	**   -1  if parsing of a protocol message leads in a syntactic/semantic
	**       error and NO penalty scoring should be applied.
	**   >=0 if protocol message processing was successful. The return
	**       value indicates the penalty score.
	*/
	ret = (*mptr->func)(cptr, from, i, para);

#ifndef       CLIENT_COMPILE
	/*
        ** Add penalty score for sucessfully parsed command if issued by
	** a LOCAL user client.
	*/
	if ((ret > 0) && IsRegisteredUser(cptr))
	    {
		cptr->since += ret;
/* only to lurk
		sendto_one(cptr,
			   ":%s NOTICE %s :*** Penalty INCR [%s] +%d",
			   me.name, cptr->name, ch, ret);
*/
	    }
#endif
	return (ret != FLUSH_BUFFER) ? 2 : FLUSH_BUFFER;
}

/*
 * field breakup for ircd.conf file.
 */
char	*getfield(newline)
char	*newline;
{
	static	char *line = NULL;
	char	*end, *field;
	
	if (newline)
		line = newline;
	if (line == NULL)
		return(NULL);

	field = line;
	if ((end = (char *)index(line,':')) == NULL)
	    {
		line = NULL;
		if ((end = (char *)index(field,'\n')) == NULL)
			end = field + strlen(field);
	    }
	else
		line = end + 1;
	*end = '\0';
	return(field);
}

#ifndef	CLIENT_COMPILE
static	int	cancel_clients(cptr, sptr, cmd)
aClient	*cptr, *sptr;
char	*cmd;
{
	/*
	 * kill all possible points that are causing confusion here,
	 * I'm not sure I've got this all right...
	 * - avalon
	 */
	sendto_flag(SCH_NOTICE, "Message (%s) for %s[%s!%s@%s] from %s",
		    cmd, sptr->name, sptr->from->name, sptr->from->username,
		    sptr->from->sockhost, get_client_name(cptr, TRUE));
	/*
	 * Incorrect prefix for a server from some connection.  If it is a
	 * client trying to be annoying, just QUIT them, if it is a server
	 * then the same deal.
	 */
	if (IsServer(sptr) || IsMe(sptr))
	    {
		sendto_flag(SCH_NOTICE, "Dropping server %s",cptr->name);
		return exit_client(cptr, cptr, &me, "Fake Direction");
	    }
	/*
	 * Ok, someone is trying to impose as a client and things are
	 * confused.  If we got the wrong prefix from a server, send out a
	 * kill, else just exit the lame client.
	 */
	if (IsServer(cptr))
	    {
		sendto_serv_butone(NULL, ":%s KILL %s :%s (%s[%s] != %s)",
				   me.name, sptr->name, me.name,
				   sptr->name, sptr->from->name,
				   get_client_name(cptr, TRUE));
		sptr->flags |= FLAGS_KILLED;
		return exit_client(cptr, sptr, &me, "Fake Prefix");
	    }
	return exit_client(cptr, cptr, &me, "Fake prefix");
}

static	void	remove_unknown(cptr, sender)
aClient	*cptr;
char	*sender;
{
	if (!IsRegistered(cptr) || IsClient(cptr))
		return;
	/*
	 * Not from a server so don't need to worry about it.
	 */
	if (!IsServer(cptr))
		return;
	/*
	 * Do kill if it came from a server because it means there is a ghost
	 * user on the other server which needs to be removed. -avalon
	 * it can simply be caused by lag (among other things), so just
	 * drop it if it is not a server. -krys
	 */
	sendto_flag(SCH_LOCAL, "Dropping unknown %s brought by %s.",
		    sender, get_client_name(cptr, FALSE));
	/*
	 * squit if it is a server because it means something is really
	 * wrong.
	 */
	if (index(sender, '.') /* <- buggy, it could be a service! */
	    && !index(sender, '@')) /* better.. */
		sendto_one(cptr, ":%s SQUIT %s :(Unknown from %s)",
			   me.name, sender, get_client_name(cptr, FALSE));
}
#endif
