.POSIX:

CONFIGFILE = config.mk
include $(CONFIGFILE)

LIB_MAJOR   = 3
LIB_MINOR   = 1
LIB_VERSION = $(LIB_MAJOR).$(LIB_MINOR)
VERSION     = 3.1.7

MAN1 = bus.1 bus-broadcast.1 bus-create.1 bus-listen.1 bus-remove.1 bus-wait.1 bus-chmod.1 bus-chown.1 bus-chgrp.1
MAN3 = bus_create.3 bus_unlink.3 bus_open.3 bus_close.3 bus_read.3 bus_write.3 bus_poll.3 bus_chmod.3 bus_chown.3
MAN5 = bus.5
MAN7 = libbus.7

LOBJ = libbus.lo
OBJ  = bus.o libbus.o
HDR  = bus.h arg.h

all: bus libbus.a libbus.so

$(OBJ): $(@:.o=.c) $(HDR)
$(LOBJ): $(@:.lo=.c) $(HDR)

bus: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

.o.a:
	$(AR) $(ARFLAGS) $@ $<

.c.lo:
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

.lo.so:
	$(CC) -shared -Wl,-soname,$@.$(LIB_MAJOR) -o $@ $< $(LDFLAGS)

bus.pdf: bus.texinfo fdl.texinfo
	texi2pdf bus.texinfo < /dev/null

install: bus libbus.a libbus.so
	mkdir -p -- "$(DESTDIR)$(PREFIX)/bin"
	mkdir -p -- "$(DESTDIR)$(PREFIX)/lib"
	mkdir -p -- "$(DESTDIR)$(PREFIX)/include"
	mkdir -p -- "$(DESTDIR)$(PREFIX)/share/licenses/bus"
	mkdir -p -- "$(DESTDIR)$(MANPREFIX)/man1"
	mkdir -p -- "$(DESTDIR)$(MANPREFIX)/man3"
	mkdir -p -- "$(DESTDIR)$(MANPREFIX)/man5"
	mkdir -p -- "$(DESTDIR)$(MANPREFIX)/man7"
	cp -- bus       "$(DESTDIR)$(PREFIX)/bin"
	cp -- libbus.a  "$(DESTDIR)$(PREFIX)/lib"
	cp -- libbus.so "$(DESTDIR)$(PREFIX)/lib/libbus.so.$(LIB_VERSION)"
	cp -- bus.h     "$(DESTDIR)$(PREFIX)/include"
	cp -- LICENSE   "$(DESTDIR)$(PREFIX)/share/licenses/bus"
	ln -sf -- libbus.so.$(LIB_VERSION) "$(DESTDIR)$(PREFIX)/lib/libbus.so.$(LIB_MAJOR)"
	ln -sf -- libbus.so.$(LIB_VERSION) "$(DESTDIR)$(PREFIX)/lib/libbus.so"
	cp -- $(MAN1) "$(DESTDIR)$(MANPREFIX)/man1"
	cp -- $(MAN3) "$(DESTDIR)$(MANPREFIX)/man3"
	cp -- $(MAN5) "$(DESTDIR)$(MANPREFIX)/man5"
	cp -- $(MAN7) "$(DESTDIR)$(MANPREFIX)/man7"
	ln -sf -- bus_poll.3 "$(DESTDIR)$(MANPREFIX)/man3/bus_poll_start.3"
	ln -sf -- bus_poll.3 "$(DESTDIR)$(MANPREFIX)/man3/bus_poll_stop.3"
	ln -sf -- bus_poll.3 "$(DESTDIR)$(MANPREFIX)/man3/bus_poll_timed.3"
	ln -sf -- bus_read.3 "$(DESTDIR)$(MANPREFIX)/man3/bus_read_timed.3"
	ln -sf -- bus_write.3 "$(DESTDIR)$(MANPREFIX)/man3/bus_write_timed.3"

uninstall:
	-rm -f  -- "$(DESTDIR)$(PREFIX)/bin/bus"
	-rm -f  -- "$(DESTDIR)$(PREFIX)/lib/libbus.a"
	-rm -f  -- "$(DESTDIR)$(PREFIX)/lib/libbus.so.$(LIB_VERSION)"
	-rm -f  -- "$(DESTDIR)$(PREFIX)/lib/libbus.so.$(LIB_MAJOR)"
	-rm -f  -- "$(DESTDIR)$(PREFIX)/lib/libbus.so"
	-rm -f  -- "$(DESTDIR)$(PREFIX)/include/bus.h"
	-rm -rf -- "$(DESTDIR)$(PREFIX)/share/licenses/bus"
	-cd "$(DESTDIR)$(MANPREFIX)/man1" && rm -f -- $(MAN1)
	-cd "$(DESTDIR)$(MANPREFIX)/man3" && rm -f -- $(MAN3)
	-cd "$(DESTDIR)$(MANPREFIX)/man5" && rm -f -- $(MAN5)
	-cd "$(DESTDIR)$(MANPREFIX)/man7" && rm -f -- $(MAN7)
	-rm -f -- "$(DESTDIR)$(MANPREFIX)/man3/bus_poll_start.3"
	-rm -f -- "$(DESTDIR)$(MANPREFIX)/man3/bus_poll_stop.3"
	-rm -f -- "$(DESTDIR)$(MANPREFIX)/man3/bus_poll_timed.3"
	-rm -f -- "$(DESTDIR)$(MANPREFIX)/man3/bus_read_timed.3"
	-rm -f -- "$(DESTDIR)$(MANPREFIX)/man3/bus_write_timed.3"

clean:
	-rm -f -- bus *.o *.lo *.a *.so *.log *.toc *.aux *.pdf

.SUFFIXES:
.SUFFIXES: .so .a .o .lo .c .pdf

.PHONY: all install uninstall clean
