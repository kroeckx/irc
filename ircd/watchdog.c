/************************************************************************
 *   IRC - Internet Relay Chat, ircd/watchdog.c
 *   Copyright (C) 1999 Christophe Kalt
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
static  char rcsid[] = "@(#)$Id: watchdog.c,v 1.1.2.1 1999/09/30 16:38:43 kalt Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_WATCHDOG_C
#include "s_externs.h"
#undef S_WATCHDOG_C

static u_char	pain = 0;
static time_t	time_origin;
static int	skip;

/*
** dog_set
**	used to set the time origin, has to be called before dog_check
*/
void
dog_set()
{
    time_origin = timeofday;
    skip = 0;
}

/*
** dog_check
**	is the server "slow" ? e.g. did too much time go by since dog_set()
**	was called?  the single parameter defines a watchdog number, allowing
**	for several which have different reactions to the slowness.
*/
void
dog_check(d)
int d;
{
    static int last = -1;
    int slowness = timeofday - time_origin;

    if (d == 0)
	/* this dog exists for debugging purposes only. */
	{
	    if (slowness > 1)
		    sendto_flag(SCH_DEBUG, "dog #0: %d", slowness);
	    return;
	}
    if (pain)
	    sendto_flag(SCH_DEBUG, "dog awareness: %d", pain);
    if (slowness > 0)
	{
	    if (slowness > 1 || pain)
		    sendto_flag(SCH_DEBUG, "dog #%d: %d", d, slowness);
	    if (slowness == 1)
		    ;
	    else if (slowness == 2)
		    pain = MAX(pain, 1);
	    else if (slowness < 5)
		    pain = MIN(5, MAX(pain+1, 2));
	    else
		    pain = 5;
	}
    else if (pain > 0 && skip == 0)
	    pain -= 1;
    time_origin = timeofday;
    last = d;
}

/*
** dog_done
**	someone got bitten by the dog.
*/
void
dog_done()
{
    skip = 1;
}

/*
** dog_incr
**	returns the number of clients that can be dealt with in read_message()
**	without worrying of the time.  the values can and should probably be
**	tuned.  it's also questionnable whether values other than 50 really
**	make a difference (e.g. probably not).
*/
int
dog_incr()
{
    if (bootopt & BOOT_PROT == 0)
	    return MAXCONNECTIONS;
    switch (pain)
	{
	case 0: return 500;
	case 1: return 500;
	case 2: return 400;
	case 3: return 300;
	case 4: return 100;
	default: return 50;
	}
}
