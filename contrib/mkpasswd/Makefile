# Makefile for the mkpasswd
# CVS $Id: Makefile,v 1.1.2.1 2001/10/19 21:49:05 chopin Exp $

CC=gcc
CFLAGS	= -O2 -Wall
LDFLAGS = -lcrypt

OBJS =	mkpasswd.o
SRCS =	mkpasswd.c

.c.o:
	$(CC) $(CFLAGS) -c $<

all: mkpasswd

clean:
	/bin/rm -rf *.o core *.core

mkpasswd: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

# Catch any changes in compilation options at the top of this file
$(OBJS): Makefile
