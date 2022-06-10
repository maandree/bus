PREFIX    = /usr/local
MANPREFIX = $(PREFIX)/share/man

CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700 -D_GNU_SOURCE
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -O2 $(CPPFLAGS)
LDFLAGS  = -s -lrt

# Add -DSEMUN_ALREADY_DEFINED to CPPFLAGS if `union semun` is already defined by libc
