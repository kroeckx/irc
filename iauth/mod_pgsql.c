/************************************************************************
 *   IRC - Internet Relay Chat, iauth/mod_pgsql.c
 *   Copyright (C) 1998 Christophe Kalt
 *   Copyright (C) 2004 Piotr Kucharski
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
static const volatile char rcsid[] = "@(#)$Id: mod_pgsql.c,v 1.1 2014/09/03 10:50:02 bif Exp $";
#endif

#include "os.h"

#ifdef USE_PGSQL

#include "a_defines.h"
#define MOD_PGSQL_C
#include "a_externs.h"
#undef MOD_PGSQL_C

#include "libpq-fe.h"

/****************************** PRIVATE *************************************/

static PGconn *conn = NULL;
static char connstring[1024];

#define OPT_LOG         0x001
#define OPT_DENY        0x002

struct pgsql_private
{
	u_char options;
	/* stats */
	u_int logged, denied;
};

/******************************** PUBLIC ************************************/

/*
 * pgsql_init
 *
 *	This procedure is called when a particular module is loaded.
 *	Returns NULL if everything went fine,
 *	an error message otherwise.
 */
char *pgsql_init(AnInstance *self)
{
	struct pgsql_private *mydata;
	char tmpbuf[80];
	static char txtbuf[80];
	char *ch;
	int myopt = 0;

	if (!self->opt)
	{
		return "Aie! unknown option(s): nothing to be done!";
	}

	if ((ch = strstr(self->opt, "conn=")))
	{
		sprintf(connstring, "%s", ch+5);
		/* so "log" or "reject" will not be found within conn= */
		*ch = '\0';
	}
	else
	{
		return "Aie! set connection info";
	}

	tmpbuf[0] = txtbuf[0] = '\0';

	if (strstr(self->opt, "log"))
	{
		myopt |= OPT_LOG;
		strcat(tmpbuf, "log ");
		strcat(txtbuf, "Log ");
	}
	if (strstr(self->opt, "reject"))
	{
		myopt |= OPT_DENY;
		strcat(tmpbuf, "reject ");
		strcat(txtbuf, "Reject ");
	}
	
	if (myopt == 0)
	{
		return "Aie! unknown option(s): nothing to be done!";
	}

	conn = PQconnectdb(connstring);
	if (!conn || PQstatus(conn) != CONNECTION_OK)
	{
		return "pgsql_init: connection could not be established";
	}

	mydata = (struct pgsql_private *) malloc(sizeof(struct pgsql_private));
	bzero((char *) mydata, sizeof(struct pgsql_private));
	mydata->options = myopt;
	self->popt = mystrdup(tmpbuf);
	self->data = mydata;

	return txtbuf+2;
}

/*
 * pgsql_release
 *
 *	This procedure is called when a particular module is unloaded.
 */
void pgsql_release(AnInstance *self)
{
	struct pgsql_private *mydata = self->data;
	
	PQfinish(conn);
	free(mydata);
	free(self->popt);
}

/*
 * pgsql_stats
 *
 *	This procedure is called regularly to update statistics sent to ircd.
 */
void pgsql_stats(AnInstance *self)
{
	struct pgsql_private *mydata = self->data;

	sendto_ircd("S pgsql logged %u denied %u", mydata->logged,
		mydata->denied);
}

/*
 * pgsql_start
 *
 *	This procedure is called to start the socks check procedure.
 *	Returns 0 if everything went fine,
 *	-1 otherwise (nothing to be done, or failure)
 *
 *	It is responsible for sending error messages where appropriate.
 *	In case of failure, it's responsible for cleaning up (e.g. pgsql_clean
 *	will NOT be called)
 */
int pgsql_start(u_int cl)
{
	struct pgsql_private *mydata = cldata[cl].instance->data;
	char message[1024];
	char messagetmp[1024];
	PGresult *res;
	ExecStatusType retc;
	char *reason = cldata[cl].instance->reason;

	if (cldata[cl].state & A_DENY)
	{
		DebugLog((ALOG_DPGSQL, 0,
                        "pgsql_start(%d): A_DENY already set ", cl));
                return -1;
	}

	if (PQstatus(conn) == CONNECTION_BAD)
	{
		char *s = PQerrorMessage(conn);

		sendto_log(ALOG_IRCD, LOG_ERR, "pgsql_start(%d) conn bad %s",
			cl, BadPtr(s) ? "?" : s);

		/* If connection to db is broken somehow, restarting
		 * iauth will revive it, will it not? --B. */
       		exit(1);
	}

	/*
	 * Clean interface would be two blocks of { new message, PQexec(),
	 * PQresultStatus(), PQclear(), parsing result }, one doing logging,
	 * another checking for klines. By using little trick of concatenation
	 * I save on one PQexec(), but only due to fact we don't need output
	 * from inserting into database. Quoting libpq manual:
	 *     "PQexec can return only one PGresult structure. If the submitted
	 *      query string contains multiple SQL commands, all but the last
	 *      PGresult are discarded by PQexec."
	 *
	 * When extending this module, write another block of code. --B.
	 */

	/* Logging of IP and date into database:
	 * 	CREATE TABLE ircd_conn (ip cidr, date timestamp); */
	if (mydata->options & OPT_LOG)
	{
		mydata->logged++;
		sprintf(message,
			"INSERT INTO ircd_conn (ip,date) VALUES ('%s',now());",
			cldata[cl].itsip);
		retc = PGRES_COMMAND_OK;
	}
	/* Querying for klined IPs/nets from database:
	 * 	CREATE TABLE ircd_kline (ip cidr); */
	if (mydata->options & OPT_DENY)
	{
		sprintf((mydata->options & OPT_LOG) ? messagetmp : message,
			"SELECT ip FROM ircd_kline WHERE '%s'<<=ip LIMIT 1;",
			cldata[cl].itsip);
		if (mydata->options & OPT_LOG)
		{
			strcat(message, messagetmp);
		}
		retc = PGRES_TUPLES_OK;
	}

	DebugLog((ALOG_DPGSQL, 0, "pgsql_start(%d): %s", cl, message));
	res = PQexec(conn, message);
	if (!res || PQresultStatus(res) != retc)
	{
		sendto_log(ALOG_IRCD, LOG_NOTICE,
			"pgsql_start(%d) PQexec failed: %s", cl,
			PQresultErrorMessage(res));
		PQclear(res);
		return -1;
	}
	if ((mydata->options & OPT_DENY) && PQntuples(res) == 1)
	{
		mydata->denied++;
		if (!reason)
		{
			reason = "Denied access (SQL)";
		}
		cldata[cl].state |= A_DENY;
		sendto_ircd("k %d %s %u :%s", cl, cldata[cl].itsip,
			cldata[cl].itsport, reason);
	}
	PQclear(res);

	return -1;
}

/*
 * pgsql_work
 *
 *	This procedure is called whenever there's new data in the buffer.
 *	Returns 0 if everything went fine, and there is more work to be done,
 *	Returns -1 if the module has finished its work (and cleaned up).
 *
 *	It is responsible for sending error messages where appropriate.
 */
int pgsql_work(u_int cl)
{
	DebugLog((ALOG_DPGSQL, 0, "pgsql_work(%d): doing nothing", cl));
	/* Since we hazardously (and in a blocking way) do all work
	** in pgsql_start(), we have nothing to do here. In fact it
	** should never be even called. */
	return -1;
}

/*
 * pgsql_clean
 *
 *	This procedure is called whenever the module should interrupt its work.
 *	It is responsible for cleaning up any allocated data, and in particular
 *	closing file descriptors.
 */
void pgsql_clean(u_int cl)
{
	DebugLog((ALOG_DPGSQL, 0, "pgsql_clean(%d): cleaning up", cl));
	/* so far we don't have anything to clean */
}

/*
 * pgsql_timeout
 *
 *	This procedure is called whenever the timeout set by the module is
 *	reached.
 *
 *	Returns 0 if things are okay, -1 if check was aborted.
 */
int pgsql_timeout(u_int cl)
{
	DebugLog((ALOG_DPGSQL, 0, "pgsql_timeout(%d): calling pgsql_clean ", cl));
	pgsql_clean(cl);
	/* Since it hardly can timeout, let's assume things are okay --B. */
	return 0;
}

aModule Module_pgsql =
	{ "pgsql", pgsql_init, pgsql_release, pgsql_stats,
	  pgsql_start, pgsql_work, pgsql_timeout, pgsql_clean };

#endif  /* USE_PGSQL */
