/************************************************************************
 *   IRC - Internet Relay Chat, common/irc_sprintf_ext.h
 *   Copyright (C) 2002 Piotr Kucharski
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

/*  This file contains external definitions for global variables and functions
    defined in common/irc_sprintf.c.
    $Id: irc_sprintf_ext.h,v 1.3 2002/08/23 16:31:32 chopin Exp $
 */

/*  External definitions for global functions.
 */

#include <stdarg.h>
#include <stdio.h>
#include <strings.h>

/* extern void *memmove(void *s1, const void *s2, size_t n); */


#define BUFSIZE 512

#ifdef IRC_SPRINTF_C

#undef IRC_SPRINTF_SNPRINTF
#undef IRC_SPRINTF_DEBUG

#ifdef IRC_SPRINTF_DEBUG
# include <assert.h>
#endif

typedef struct Client aClient;
struct Client {
        char name[9];
        char uid[9];
};

static const char atoo_tab[128] = {
'0','0', '0','1', '0','2', '0','3', '0','4', '0','5', '0','6', '0','7',
'1','0', '1','1', '1','2', '1','3', '1','4', '1','5', '1','6', '1','7',
'2','0', '2','1', '2','2', '2','3', '2','4', '2','5', '2','6', '2','7',
'3','0', '3','1', '3','2', '3','3', '3','4', '3','5', '3','6', '3','7',
'4','0', '4','1', '4','2', '4','3', '4','4', '4','5', '4','6', '4','7',
'5','0', '5','1', '5','2', '5','3', '5','4', '5','5', '5','6', '5','7',
'6','0', '6','1', '6','2', '6','3', '6','4', '6','5', '6','6', '6','7',
'7','0', '7','1', '7','2', '7','3', '7','4', '7','5', '7','6', '7','7'};

static const char atox_tab[512] = {
'0','0', '0','1', '0','2', '0','3', '0','4', '0','5', '0','6', '0','7',
'0','8', '0','9', '0','a', '0','b', '0','c', '0','d', '0','e', '0','f',
'1','0', '1','1', '1','2', '1','3', '1','4', '1','5', '1','6', '1','7',
'1','8', '1','9', '1','a', '1','b', '1','c', '1','d', '1','e', '1','f',
'2','0', '2','1', '2','2', '2','3', '2','4', '2','5', '2','6', '2','7',
'2','8', '2','9', '2','a', '2','b', '2','c', '2','d', '2','e', '2','f',
'3','0', '3','1', '3','2', '3','3', '3','4', '3','5', '3','6', '3','7',
'3','8', '3','9', '3','a', '3','b', '3','c', '3','d', '3','e', '3','f',
'4','0', '4','1', '4','2', '4','3', '4','4', '4','5', '4','6', '4','7',
'4','8', '4','9', '4','a', '4','b', '4','c', '4','d', '4','e', '4','f',
'5','0', '5','1', '5','2', '5','3', '5','4', '5','5', '5','6', '5','7',
'5','8', '5','9', '5','a', '5','b', '5','c', '5','d', '5','e', '5','f',
'6','0', '6','1', '6','2', '6','3', '6','4', '6','5', '6','6', '6','7',
'6','8', '6','9', '6','a', '6','b', '6','c', '6','d', '6','e', '6','f',
'7','0', '7','1', '7','2', '7','3', '7','4', '7','5', '7','6', '7','7',
'7','8', '7','9', '7','a', '7','b', '7','c', '7','d', '7','e', '7','f',
'8','0', '8','1', '8','2', '8','3', '8','4', '8','5', '8','6', '8','7',
'8','8', '8','9', '8','a', '8','b', '8','c', '8','d', '8','e', '8','f',
'9','0', '9','1', '9','2', '9','3', '9','4', '9','5', '9','6', '9','7',
'9','8', '9','9', '9','a', '9','b', '9','c', '9','d', '9','e', '9','f',
'a','0', 'a','1', 'a','2', 'a','3', 'a','4', 'a','5', 'a','6', 'a','7',
'a','8', 'a','9', 'a','a', 'a','b', 'a','c', 'a','d', 'a','e', 'a','f',
'b','0', 'b','1', 'b','2', 'b','3', 'b','4', 'b','5', 'b','6', 'b','7',
'b','8', 'b','9', 'b','a', 'b','b', 'b','c', 'b','d', 'b','e', 'b','f',
'c','0', 'c','1', 'c','2', 'c','3', 'c','4', 'c','5', 'c','6', 'c','7',
'c','8', 'c','9', 'c','a', 'c','b', 'c','c', 'c','d', 'c','e', 'c','f',
'd','0', 'd','1', 'd','2', 'd','3', 'd','4', 'd','5', 'd','6', 'd','7',
'd','8', 'd','9', 'd','a', 'd','b', 'd','c', 'd','d', 'd','e', 'd','f',
'e','0', 'e','1', 'e','2', 'e','3', 'e','4', 'e','5', 'e','6', 'e','7',
'e','8', 'e','9', 'e','a', 'e','b', 'e','c', 'e','d', 'e','e', 'e','f',
'f','0', 'f','1', 'f','2', 'f','3', 'f','4', 'f','5', 'f','6', 'f','7',
'f','8', 'f','9', 'f','a', 'f','b', 'f','c', 'f','d', 'f','e', 'f','f'};
static const char atoxx_tab[512] = {
'0','0', '0','1', '0','2', '0','3', '0','4', '0','5', '0','6', '0','7',
'0','8', '0','9', '0','A', '0','B', '0','C', '0','D', '0','E', '0','F',
'1','0', '1','1', '1','2', '1','3', '1','4', '1','5', '1','6', '1','7',
'1','8', '1','9', '1','A', '1','B', '1','C', '1','D', '1','E', '1','F',
'2','0', '2','1', '2','2', '2','3', '2','4', '2','5', '2','6', '2','7',
'2','8', '2','9', '2','A', '2','B', '2','C', '2','D', '2','E', '2','F',
'3','0', '3','1', '3','2', '3','3', '3','4', '3','5', '3','6', '3','7',
'3','8', '3','9', '3','A', '3','B', '3','C', '3','D', '3','E', '3','F',
'4','0', '4','1', '4','2', '4','3', '4','4', '4','5', '4','6', '4','7',
'4','8', '4','9', '4','A', '4','B', '4','C', '4','D', '4','E', '4','F',
'5','0', '5','1', '5','2', '5','3', '5','4', '5','5', '5','6', '5','7',
'5','8', '5','9', '5','A', '5','B', '5','C', '5','D', '5','E', '5','F',
'6','0', '6','1', '6','2', '6','3', '6','4', '6','5', '6','6', '6','7',
'6','8', '6','9', '6','A', '6','B', '6','C', '6','D', '6','E', '6','F',
'7','0', '7','1', '7','2', '7','3', '7','4', '7','5', '7','6', '7','7',
'7','8', '7','9', '7','A', '7','B', '7','C', '7','D', '7','E', '7','F',
'8','0', '8','1', '8','2', '8','3', '8','4', '8','5', '8','6', '8','7',
'8','8', '8','9', '8','A', '8','B', '8','C', '8','D', '8','E', '8','F',
'9','0', '9','1', '9','2', '9','3', '9','4', '9','5', '9','6', '9','7',
'9','8', '9','9', '9','A', '9','B', '9','C', '9','D', '9','E', '9','F',
'A','0', 'A','1', 'A','2', 'A','3', 'A','4', 'A','5', 'A','6', 'A','7',
'A','8', 'A','9', 'A','A', 'A','B', 'A','C', 'A','D', 'A','E', 'A','F',
'B','0', 'B','1', 'B','2', 'B','3', 'B','4', 'B','5', 'B','6', 'B','7',
'B','8', 'B','9', 'B','A', 'B','B', 'B','C', 'B','D', 'B','E', 'B','F',
'C','0', 'C','1', 'C','2', 'C','3', 'C','4', 'C','5', 'C','6', 'C','7',
'C','8', 'C','9', 'C','A', 'C','B', 'C','C', 'C','D', 'C','E', 'C','F',
'D','0', 'D','1', 'D','2', 'D','3', 'D','4', 'D','5', 'D','6', 'D','7',
'D','8', 'D','9', 'D','A', 'D','B', 'D','C', 'D','D', 'D','E', 'D','F',
'E','0', 'E','1', 'E','2', 'E','3', 'E','4', 'E','5', 'E','6', 'E','7',
'E','8', 'E','9', 'E','A', 'E','B', 'E','C', 'E','D', 'E','E', 'E','F',
'F','0', 'F','1', 'F','2', 'F','3', 'F','4', 'F','5', 'F','6', 'F','7',
'F','8', 'F','9', 'F','A', 'F','B', 'F','C', 'F','D', 'F','E', 'F','F'};
static const char atod_tab[4000] = {
'0','0','0',0, '0','0','1',0, '0','0','2',0, '0','0','3',0, '0','0','4',0, 
'0','0','5',0, '0','0','6',0, '0','0','7',0, '0','0','8',0, '0','0','9',0, 
'0','1','0',0, '0','1','1',0, '0','1','2',0, '0','1','3',0, '0','1','4',0, 
'0','1','5',0, '0','1','6',0, '0','1','7',0, '0','1','8',0, '0','1','9',0, 
'0','2','0',0, '0','2','1',0, '0','2','2',0, '0','2','3',0, '0','2','4',0, 
'0','2','5',0, '0','2','6',0, '0','2','7',0, '0','2','8',0, '0','2','9',0, 
'0','3','0',0, '0','3','1',0, '0','3','2',0, '0','3','3',0, '0','3','4',0, 
'0','3','5',0, '0','3','6',0, '0','3','7',0, '0','3','8',0, '0','3','9',0, 
'0','4','0',0, '0','4','1',0, '0','4','2',0, '0','4','3',0, '0','4','4',0, 
'0','4','5',0, '0','4','6',0, '0','4','7',0, '0','4','8',0, '0','4','9',0, 
'0','5','0',0, '0','5','1',0, '0','5','2',0, '0','5','3',0, '0','5','4',0, 
'0','5','5',0, '0','5','6',0, '0','5','7',0, '0','5','8',0, '0','5','9',0, 
'0','6','0',0, '0','6','1',0, '0','6','2',0, '0','6','3',0, '0','6','4',0, 
'0','6','5',0, '0','6','6',0, '0','6','7',0, '0','6','8',0, '0','6','9',0, 
'0','7','0',0, '0','7','1',0, '0','7','2',0, '0','7','3',0, '0','7','4',0, 
'0','7','5',0, '0','7','6',0, '0','7','7',0, '0','7','8',0, '0','7','9',0, 
'0','8','0',0, '0','8','1',0, '0','8','2',0, '0','8','3',0, '0','8','4',0, 
'0','8','5',0, '0','8','6',0, '0','8','7',0, '0','8','8',0, '0','8','9',0, 
'0','9','0',0, '0','9','1',0, '0','9','2',0, '0','9','3',0, '0','9','4',0, 
'0','9','5',0, '0','9','6',0, '0','9','7',0, '0','9','8',0, '0','9','9',0, 

'1','0','0',0, '1','0','1',0, '1','0','2',0, '1','0','3',0, '1','0','4',0, 
'1','0','5',0, '1','0','6',0, '1','0','7',0, '1','0','8',0, '1','0','9',0, 
'1','1','0',0, '1','1','1',0, '1','1','2',0, '1','1','3',0, '1','1','4',0, 
'1','1','5',0, '1','1','6',0, '1','1','7',0, '1','1','8',0, '1','1','9',0, 
'1','2','0',0, '1','2','1',0, '1','2','2',0, '1','2','3',0, '1','2','4',0, 
'1','2','5',0, '1','2','6',0, '1','2','7',0, '1','2','8',0, '1','2','9',0, 
'1','3','0',0, '1','3','1',0, '1','3','2',0, '1','3','3',0, '1','3','4',0, 
'1','3','5',0, '1','3','6',0, '1','3','7',0, '1','3','8',0, '1','3','9',0, 
'1','4','0',0, '1','4','1',0, '1','4','2',0, '1','4','3',0, '1','4','4',0, 
'1','4','5',0, '1','4','6',0, '1','4','7',0, '1','4','8',0, '1','4','9',0, 
'1','5','0',0, '1','5','1',0, '1','5','2',0, '1','5','3',0, '1','5','4',0, 
'1','5','5',0, '1','5','6',0, '1','5','7',0, '1','5','8',0, '1','5','9',0, 
'1','6','0',0, '1','6','1',0, '1','6','2',0, '1','6','3',0, '1','6','4',0, 
'1','6','5',0, '1','6','6',0, '1','6','7',0, '1','6','8',0, '1','6','9',0, 
'1','7','0',0, '1','7','1',0, '1','7','2',0, '1','7','3',0, '1','7','4',0, 
'1','7','5',0, '1','7','6',0, '1','7','7',0, '1','7','8',0, '1','7','9',0, 
'1','8','0',0, '1','8','1',0, '1','8','2',0, '1','8','3',0, '1','8','4',0, 
'1','8','5',0, '1','8','6',0, '1','8','7',0, '1','8','8',0, '1','8','9',0, 
'1','9','0',0, '1','9','1',0, '1','9','2',0, '1','9','3',0, '1','9','4',0, 
'1','9','5',0, '1','9','6',0, '1','9','7',0, '1','9','8',0, '1','9','9',0, 

'2','0','0',0, '2','0','1',0, '2','0','2',0, '2','0','3',0, '2','0','4',0, 
'2','0','5',0, '2','0','6',0, '2','0','7',0, '2','0','8',0, '2','0','9',0, 
'2','1','0',0, '2','1','1',0, '2','1','2',0, '2','1','3',0, '2','1','4',0, 
'2','1','5',0, '2','1','6',0, '2','1','7',0, '2','1','8',0, '2','1','9',0, 
'2','2','0',0, '2','2','1',0, '2','2','2',0, '2','2','3',0, '2','2','4',0, 
'2','2','5',0, '2','2','6',0, '2','2','7',0, '2','2','8',0, '2','2','9',0, 
'2','3','0',0, '2','3','1',0, '2','3','2',0, '2','3','3',0, '2','3','4',0, 
'2','3','5',0, '2','3','6',0, '2','3','7',0, '2','3','8',0, '2','3','9',0, 
'2','4','0',0, '2','4','1',0, '2','4','2',0, '2','4','3',0, '2','4','4',0, 
'2','4','5',0, '2','4','6',0, '2','4','7',0, '2','4','8',0, '2','4','9',0, 
'2','5','0',0, '2','5','1',0, '2','5','2',0, '2','5','3',0, '2','5','4',0, 
'2','5','5',0, '2','5','6',0, '2','5','7',0, '2','5','8',0, '2','5','9',0, 
'2','6','0',0, '2','6','1',0, '2','6','2',0, '2','6','3',0, '2','6','4',0, 
'2','6','5',0, '2','6','6',0, '2','6','7',0, '2','6','8',0, '2','6','9',0, 
'2','7','0',0, '2','7','1',0, '2','7','2',0, '2','7','3',0, '2','7','4',0, 
'2','7','5',0, '2','7','6',0, '2','7','7',0, '2','7','8',0, '2','7','9',0, 
'2','8','0',0, '2','8','1',0, '2','8','2',0, '2','8','3',0, '2','8','4',0, 
'2','8','5',0, '2','8','6',0, '2','8','7',0, '2','8','8',0, '2','8','9',0, 
'2','9','0',0, '2','9','1',0, '2','9','2',0, '2','9','3',0, '2','9','4',0, 
'2','9','5',0, '2','9','6',0, '2','9','7',0, '2','9','8',0, '2','9','9',0, 

'3','0','0',0, '3','0','1',0, '3','0','2',0, '3','0','3',0, '3','0','4',0, 
'3','0','5',0, '3','0','6',0, '3','0','7',0, '3','0','8',0, '3','0','9',0, 
'3','1','0',0, '3','1','1',0, '3','1','2',0, '3','1','3',0, '3','1','4',0, 
'3','1','5',0, '3','1','6',0, '3','1','7',0, '3','1','8',0, '3','1','9',0, 
'3','2','0',0, '3','2','1',0, '3','2','2',0, '3','2','3',0, '3','2','4',0, 
'3','2','5',0, '3','2','6',0, '3','2','7',0, '3','2','8',0, '3','2','9',0, 
'3','3','0',0, '3','3','1',0, '3','3','2',0, '3','3','3',0, '3','3','4',0, 
'3','3','5',0, '3','3','6',0, '3','3','7',0, '3','3','8',0, '3','3','9',0, 
'3','4','0',0, '3','4','1',0, '3','4','2',0, '3','4','3',0, '3','4','4',0, 
'3','4','5',0, '3','4','6',0, '3','4','7',0, '3','4','8',0, '3','4','9',0, 
'3','5','0',0, '3','5','1',0, '3','5','2',0, '3','5','3',0, '3','5','4',0, 
'3','5','5',0, '3','5','6',0, '3','5','7',0, '3','5','8',0, '3','5','9',0, 
'3','6','0',0, '3','6','1',0, '3','6','2',0, '3','6','3',0, '3','6','4',0, 
'3','6','5',0, '3','6','6',0, '3','6','7',0, '3','6','8',0, '3','6','9',0, 
'3','7','0',0, '3','7','1',0, '3','7','2',0, '3','7','3',0, '3','7','4',0, 
'3','7','5',0, '3','7','6',0, '3','7','7',0, '3','7','8',0, '3','7','9',0, 
'3','8','0',0, '3','8','1',0, '3','8','2',0, '3','8','3',0, '3','8','4',0, 
'3','8','5',0, '3','8','6',0, '3','8','7',0, '3','8','8',0, '3','8','9',0, 
'3','9','0',0, '3','9','1',0, '3','9','2',0, '3','9','3',0, '3','9','4',0, 
'3','9','5',0, '3','9','6',0, '3','9','7',0, '3','9','8',0, '3','9','9',0, 

'4','0','0',0, '4','0','1',0, '4','0','2',0, '4','0','3',0, '4','0','4',0, 
'4','0','5',0, '4','0','6',0, '4','0','7',0, '4','0','8',0, '4','0','9',0, 
'4','1','0',0, '4','1','1',0, '4','1','2',0, '4','1','3',0, '4','1','4',0, 
'4','1','5',0, '4','1','6',0, '4','1','7',0, '4','1','8',0, '4','1','9',0, 
'4','2','0',0, '4','2','1',0, '4','2','2',0, '4','2','3',0, '4','2','4',0, 
'4','2','5',0, '4','2','6',0, '4','2','7',0, '4','2','8',0, '4','2','9',0, 
'4','3','0',0, '4','3','1',0, '4','3','2',0, '4','3','3',0, '4','3','4',0, 
'4','3','5',0, '4','3','6',0, '4','3','7',0, '4','3','8',0, '4','3','9',0, 
'4','4','0',0, '4','4','1',0, '4','4','2',0, '4','4','3',0, '4','4','4',0, 
'4','4','5',0, '4','4','6',0, '4','4','7',0, '4','4','8',0, '4','4','9',0, 
'4','5','0',0, '4','5','1',0, '4','5','2',0, '4','5','3',0, '4','5','4',0, 
'4','5','5',0, '4','5','6',0, '4','5','7',0, '4','5','8',0, '4','5','9',0, 
'4','6','0',0, '4','6','1',0, '4','6','2',0, '4','6','3',0, '4','6','4',0, 
'4','6','5',0, '4','6','6',0, '4','6','7',0, '4','6','8',0, '4','6','9',0, 
'4','7','0',0, '4','7','1',0, '4','7','2',0, '4','7','3',0, '4','7','4',0, 
'4','7','5',0, '4','7','6',0, '4','7','7',0, '4','7','8',0, '4','7','9',0, 
'4','8','0',0, '4','8','1',0, '4','8','2',0, '4','8','3',0, '4','8','4',0, 
'4','8','5',0, '4','8','6',0, '4','8','7',0, '4','8','8',0, '4','8','9',0, 
'4','9','0',0, '4','9','1',0, '4','9','2',0, '4','9','3',0, '4','9','4',0, 
'4','9','5',0, '4','9','6',0, '4','9','7',0, '4','9','8',0, '4','9','9',0, 

'5','0','0',0, '5','0','1',0, '5','0','2',0, '5','0','3',0, '5','0','4',0, 
'5','0','5',0, '5','0','6',0, '5','0','7',0, '5','0','8',0, '5','0','9',0, 
'5','1','0',0, '5','1','1',0, '5','1','2',0, '5','1','3',0, '5','1','4',0, 
'5','1','5',0, '5','1','6',0, '5','1','7',0, '5','1','8',0, '5','1','9',0, 
'5','2','0',0, '5','2','1',0, '5','2','2',0, '5','2','3',0, '5','2','4',0, 
'5','2','5',0, '5','2','6',0, '5','2','7',0, '5','2','8',0, '5','2','9',0, 
'5','3','0',0, '5','3','1',0, '5','3','2',0, '5','3','3',0, '5','3','4',0, 
'5','3','5',0, '5','3','6',0, '5','3','7',0, '5','3','8',0, '5','3','9',0, 
'5','4','0',0, '5','4','1',0, '5','4','2',0, '5','4','3',0, '5','4','4',0, 
'5','4','5',0, '5','4','6',0, '5','4','7',0, '5','4','8',0, '5','4','9',0, 
'5','5','0',0, '5','5','1',0, '5','5','2',0, '5','5','3',0, '5','5','4',0, 
'5','5','5',0, '5','5','6',0, '5','5','7',0, '5','5','8',0, '5','5','9',0, 
'5','6','0',0, '5','6','1',0, '5','6','2',0, '5','6','3',0, '5','6','4',0, 
'5','6','5',0, '5','6','6',0, '5','6','7',0, '5','6','8',0, '5','6','9',0, 
'5','7','0',0, '5','7','1',0, '5','7','2',0, '5','7','3',0, '5','7','4',0, 
'5','7','5',0, '5','7','6',0, '5','7','7',0, '5','7','8',0, '5','7','9',0, 
'5','8','0',0, '5','8','1',0, '5','8','2',0, '5','8','3',0, '5','8','4',0, 
'5','8','5',0, '5','8','6',0, '5','8','7',0, '5','8','8',0, '5','8','9',0, 
'5','9','0',0, '5','9','1',0, '5','9','2',0, '5','9','3',0, '5','9','4',0, 
'5','9','5',0, '5','9','6',0, '5','9','7',0, '5','9','8',0, '5','9','9',0, 

'6','0','0',0, '6','0','1',0, '6','0','2',0, '6','0','3',0, '6','0','4',0, 
'6','0','5',0, '6','0','6',0, '6','0','7',0, '6','0','8',0, '6','0','9',0, 
'6','1','0',0, '6','1','1',0, '6','1','2',0, '6','1','3',0, '6','1','4',0, 
'6','1','5',0, '6','1','6',0, '6','1','7',0, '6','1','8',0, '6','1','9',0, 
'6','2','0',0, '6','2','1',0, '6','2','2',0, '6','2','3',0, '6','2','4',0, 
'6','2','5',0, '6','2','6',0, '6','2','7',0, '6','2','8',0, '6','2','9',0, 
'6','3','0',0, '6','3','1',0, '6','3','2',0, '6','3','3',0, '6','3','4',0, 
'6','3','5',0, '6','3','6',0, '6','3','7',0, '6','3','8',0, '6','3','9',0, 
'6','4','0',0, '6','4','1',0, '6','4','2',0, '6','4','3',0, '6','4','4',0, 
'6','4','5',0, '6','4','6',0, '6','4','7',0, '6','4','8',0, '6','4','9',0, 
'6','5','0',0, '6','5','1',0, '6','5','2',0, '6','5','3',0, '6','5','4',0, 
'6','5','5',0, '6','5','6',0, '6','5','7',0, '6','5','8',0, '6','5','9',0, 
'6','6','0',0, '6','6','1',0, '6','6','2',0, '6','6','3',0, '6','6','4',0, 
'6','6','5',0, '6','6','6',0, '6','6','7',0, '6','6','8',0, '6','6','9',0, 
'6','7','0',0, '6','7','1',0, '6','7','2',0, '6','7','3',0, '6','7','4',0, 
'6','7','5',0, '6','7','6',0, '6','7','7',0, '6','7','8',0, '6','7','9',0, 
'6','8','0',0, '6','8','1',0, '6','8','2',0, '6','8','3',0, '6','8','4',0, 
'6','8','5',0, '6','8','6',0, '6','8','7',0, '6','8','8',0, '6','8','9',0, 
'6','9','0',0, '6','9','1',0, '6','9','2',0, '6','9','3',0, '6','9','4',0, 
'6','9','5',0, '6','9','6',0, '6','9','7',0, '6','9','8',0, '6','9','9',0, 

'7','0','0',0, '7','0','1',0, '7','0','2',0, '7','0','3',0, '7','0','4',0, 
'7','0','5',0, '7','0','6',0, '7','0','7',0, '7','0','8',0, '7','0','9',0, 
'7','1','0',0, '7','1','1',0, '7','1','2',0, '7','1','3',0, '7','1','4',0, 
'7','1','5',0, '7','1','6',0, '7','1','7',0, '7','1','8',0, '7','1','9',0, 
'7','2','0',0, '7','2','1',0, '7','2','2',0, '7','2','3',0, '7','2','4',0, 
'7','2','5',0, '7','2','6',0, '7','2','7',0, '7','2','8',0, '7','2','9',0, 
'7','3','0',0, '7','3','1',0, '7','3','2',0, '7','3','3',0, '7','3','4',0, 
'7','3','5',0, '7','3','6',0, '7','3','7',0, '7','3','8',0, '7','3','9',0, 
'7','4','0',0, '7','4','1',0, '7','4','2',0, '7','4','3',0, '7','4','4',0, 
'7','4','5',0, '7','4','6',0, '7','4','7',0, '7','4','8',0, '7','4','9',0, 
'7','5','0',0, '7','5','1',0, '7','5','2',0, '7','5','3',0, '7','5','4',0, 
'7','5','5',0, '7','5','6',0, '7','5','7',0, '7','5','8',0, '7','5','9',0, 
'7','6','0',0, '7','6','1',0, '7','6','2',0, '7','6','3',0, '7','6','4',0, 
'7','6','5',0, '7','6','6',0, '7','6','7',0, '7','6','8',0, '7','6','9',0, 
'7','7','0',0, '7','7','1',0, '7','7','2',0, '7','7','3',0, '7','7','4',0, 
'7','7','5',0, '7','7','6',0, '7','7','7',0, '7','7','8',0, '7','7','9',0, 
'7','8','0',0, '7','8','1',0, '7','8','2',0, '7','8','3',0, '7','8','4',0, 
'7','8','5',0, '7','8','6',0, '7','8','7',0, '7','8','8',0, '7','8','9',0, 
'7','9','0',0, '7','9','1',0, '7','9','2',0, '7','9','3',0, '7','9','4',0, 
'7','9','5',0, '7','9','6',0, '7','9','7',0, '7','9','8',0, '7','9','9',0, 

'8','0','0',0, '8','0','1',0, '8','0','2',0, '8','0','3',0, '8','0','4',0, 
'8','0','5',0, '8','0','6',0, '8','0','7',0, '8','0','8',0, '8','0','9',0, 
'8','1','0',0, '8','1','1',0, '8','1','2',0, '8','1','3',0, '8','1','4',0, 
'8','1','5',0, '8','1','6',0, '8','1','7',0, '8','1','8',0, '8','1','9',0, 
'8','2','0',0, '8','2','1',0, '8','2','2',0, '8','2','3',0, '8','2','4',0, 
'8','2','5',0, '8','2','6',0, '8','2','7',0, '8','2','8',0, '8','2','9',0, 
'8','3','0',0, '8','3','1',0, '8','3','2',0, '8','3','3',0, '8','3','4',0, 
'8','3','5',0, '8','3','6',0, '8','3','7',0, '8','3','8',0, '8','3','9',0, 
'8','4','0',0, '8','4','1',0, '8','4','2',0, '8','4','3',0, '8','4','4',0, 
'8','4','5',0, '8','4','6',0, '8','4','7',0, '8','4','8',0, '8','4','9',0, 
'8','5','0',0, '8','5','1',0, '8','5','2',0, '8','5','3',0, '8','5','4',0, 
'8','5','5',0, '8','5','6',0, '8','5','7',0, '8','5','8',0, '8','5','9',0, 
'8','6','0',0, '8','6','1',0, '8','6','2',0, '8','6','3',0, '8','6','4',0, 
'8','6','5',0, '8','6','6',0, '8','6','7',0, '8','6','8',0, '8','6','9',0, 
'8','7','0',0, '8','7','1',0, '8','7','2',0, '8','7','3',0, '8','7','4',0, 
'8','7','5',0, '8','7','6',0, '8','7','7',0, '8','7','8',0, '8','7','9',0, 
'8','8','0',0, '8','8','1',0, '8','8','2',0, '8','8','3',0, '8','8','4',0, 
'8','8','5',0, '8','8','6',0, '8','8','7',0, '8','8','8',0, '8','8','9',0, 
'8','9','0',0, '8','9','1',0, '8','9','2',0, '8','9','3',0, '8','9','4',0, 
'8','9','5',0, '8','9','6',0, '8','9','7',0, '8','9','8',0, '8','9','9',0, 

'9','0','0',0, '9','0','1',0, '9','0','2',0, '9','0','3',0, '9','0','4',0, 
'9','0','5',0, '9','0','6',0, '9','0','7',0, '9','0','8',0, '9','0','9',0, 
'9','1','0',0, '9','1','1',0, '9','1','2',0, '9','1','3',0, '9','1','4',0, 
'9','1','5',0, '9','1','6',0, '9','1','7',0, '9','1','8',0, '9','1','9',0, 
'9','2','0',0, '9','2','1',0, '9','2','2',0, '9','2','3',0, '9','2','4',0, 
'9','2','5',0, '9','2','6',0, '9','2','7',0, '9','2','8',0, '9','2','9',0, 
'9','3','0',0, '9','3','1',0, '9','3','2',0, '9','3','3',0, '9','3','4',0, 
'9','3','5',0, '9','3','6',0, '9','3','7',0, '9','3','8',0, '9','3','9',0, 
'9','4','0',0, '9','4','1',0, '9','4','2',0, '9','4','3',0, '9','4','4',0, 
'9','4','5',0, '9','4','6',0, '9','4','7',0, '9','4','8',0, '9','4','9',0, 
'9','5','0',0, '9','5','1',0, '9','5','2',0, '9','5','3',0, '9','5','4',0, 
'9','5','5',0, '9','5','6',0, '9','5','7',0, '9','5','8',0, '9','5','9',0, 
'9','6','0',0, '9','6','1',0, '9','6','2',0, '9','6','3',0, '9','6','4',0, 
'9','6','5',0, '9','6','6',0, '9','6','7',0, '9','6','8',0, '9','6','9',0, 
'9','7','0',0, '9','7','1',0, '9','7','2',0, '9','7','3',0, '9','7','4',0, 
'9','7','5',0, '9','7','6',0, '9','7','7',0, '9','7','8',0, '9','7','9',0, 
'9','8','0',0, '9','8','1',0, '9','8','2',0, '9','8','3',0, '9','8','4',0, 
'9','8','5',0, '9','8','6',0, '9','8','7',0, '9','8','8',0, '9','8','9',0, 
'9','9','0',0, '9','9','1',0, '9','9','2',0, '9','9','3',0, '9','9','4',0, 
'9','9','5',0, '9','9','6',0, '9','9','7',0, '9','9','8',0, '9','9','9',0
};
#endif /* IRC_SPRINTF_C */
