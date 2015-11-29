# bus - simple message broadcasting IPC system
# See LICENSE file for copyright and license details.

PREFIX = /usr
BIN = /bin
BINDIR = ${PREFIX}${BIN}
LIB = /lib
LIBDIR = ${PREFIX}${LIB}
INCLUDE = /include
INCLUDEDIR = ${PREFIX}${INCLUDE}
DATA = /share
DATADIR = ${PREFIX}${DATA}
LICENSEDIR = ${DATADIR}/licenses
MANDIR = ${DATADIR}/man
INFODIR = $(DATADIR)/info
DOCDIR = $(DATADIR)/doc

PKGNAME = bus


MAN1 = bus bus-broadcast bus-create bus-listen bus-remove bus-wait bus-chmod bus-chown bus-chgrp
MAN3 = bus_create bus_unlink bus_open bus_close bus_read bus_write bus_poll bus_chmod bus_chown
MAN5 = bus
MAN7 = libbus

EXAMPLES = audio-volume-control daemon-depedencies nonblocking telephony-and-music timed
EXAMPLE_audio-volume-control = amixer cleanup init monitor README
EXAMPLE_daemon-depedencies = announce.c await-ready.c await-started.c cleanup.c d-network d-ntp \
                             d-ssh init.c Makefile README require.c start-daemon.c test-daemon.c
EXAMPLE_nonblocking = cleanup.c init.c Makefile poll.c README write.c
EXAMPLE_telephony-and-music = cleanup.c end-call.c init.c Makefile monitor.c README receive-or-make-call.c
EXAMPLE_timed = cleanup.c init.c Makefile poll.c read.c README slow-poll.c write.c


FLAGS = -std=c99 -Wall -Wextra -pedantic -O2

LIB_MAJOR = 3
LIB_MINOR = 1
LIB_VERSION = ${LIB_MAJOR}.${LIB_MINOR}
VERSION = 3.1.5



default: bus man info
all: bus doc
doc: man info pdf dvi ps
man: man1 man3 man5 man7
bus: bin lib
bin: bin/bus
lib: so a
so: bin/libbus.so.${LIB_VERSION} bin/libbus.so.${LIB_MAJOR} bin/libbus.so
a: bin/libbus.a
man1: $(foreach M,${MAN1},bin/${M}.1)
man3: $(foreach M,${MAN3},bin/${M}.3)
man5: $(foreach M,${MAN5},bin/${M}.5)
man7: $(foreach M,${MAN7},bin/${M}.7)
info: bin/bus.info
pdf: bin/bus.pdf
dvi: bin/bus.dvi
ps: bin/bus.ps

bin/%.1: doc/man/%.1
	@echo SED $@
	@mkdir -p bin
	@sed 's/%VERSION%/${VERSION}/g' < $< > $@

bin/%.3: doc/man/%.3
	@echo SED $@
	@mkdir -p bin
	@sed 's/%VERSION%/${VERSION}/g' < $< > $@

bin/%.5: doc/man/%.5
	@echo SED $@
	@mkdir -p bin
	@sed 's/%VERSION%/${VERSION}/g' < $< > $@

bin/%.7: doc/man/%.7
	@echo SED $@
	@mkdir -p bin
	@sed 's/%VERSION%/${VERSION}/g' < $< > $@

bin/libbus.a: obj/bus-fpic.o
	@echo AR $@
	@mkdir -p bin
	@ar rcs $@ $^

bin/bus: obj/cmdline-nofpic.o obj/bus-nofpic.o
	@echo CC -o $@
	@mkdir -p bin
	@${CC} ${FLAGS} -lrt -o $@ $^ ${LDFLAGS}

bin/libbus.so.${LIB_VERSION}: obj/bus-fpic.o
	@echo CC -o $@
	@mkdir -p bin
	@${CC} ${FLAGS} -lrt -shared -Wl,-soname,libbus.so.${LIB_MAJOR} -o $@ $^ ${LDFLAGS}

bin/libbus.so.${LIB_MAJOR}:
	@echo LN -s $@
	@mkdir -p bin
	@ln -sf libbus.so.${LIB_VERSION} $@

bin/libbus.so:
	@echo LN -s $@
	@mkdir -p bin
	@ln -sf libbus.so.${LIB_VERSION} $@

obj/%-nofpic.o: src/%.c src/*.h
	@echo CC -c $@
	@mkdir -p obj
	@${CC} ${FLAGS} -c -o $@ ${CPPFLAGS} ${CFLAGS} $<

obj/%-fpic.o: src/%.c src/*.h
	@echo CC -c $@
	@mkdir -p obj
	@${CC} ${FLAGS} -fPIC -c -o $@ ${CPPFLAGS} ${CFLAGS} $<

bin/%.info: doc/info/%.texinfo
	@echo MAKEINFO $@
	@mkdir -p bin
	@$(MAKEINFO) $<
	@mv $*.info $@

bin/%.pdf: doc/info/%.texinfo
	@echo TEXI2PDF $@
	@! test -d obj/pdf || rm -rf obj/pdf
	@mkdir -p bin obj/pdf
	@cd obj/pdf && texi2pdf ../../"$<" < /dev/null
	@mv obj/pdf/$*.pdf $@

bin/%.dvi: doc/info/%.texinfo
	@echo TEXI2DVI $@
	@! test -d obj/dvi || rm -rf obj/dvi
	@mkdir -p bin obj/dvi
	@cd obj/dvi && $(TEXI2DVI) ../../"$<" < /dev/null
	@mv obj/dvi/$*.dvi $@

bin/%.ps: doc/info/%.texinfo
	@echo TEXI2PS $@
	@! test -d obj/ps || rm -rf obj/ps
	@mkdir -p bin obj/ps
	@cd obj/ps && texi2pdf --ps ../../"$<" < /dev/null
	@mv obj/ps/$*.ps $@



install: install-bus install-man install-info install-examples
install-all: install-bus install-doc
install-lib: install-so install-a install-h
install-doc: install-man install-info install-pdf install-dvi install-ps install-examples
install-man: install-man1 install-man3 install-man5 install-man7
install-bus: install-bin install-lib install-license

install-bin: bin/bus
	@echo INSTALL bus
	@install -dm755 -- "${DESTDIR}${BINDIR}"
	@install -m755 $^ -- "${DESTDIR}${BINDIR}"

install-so: bin/libbus.so.${LIB_VERSION}
	@echo INSTALL libbus.so
	@install -dm755 -- "${DESTDIR}${LIBDIR}"
	@install -m755 $^ -- "${DESTDIR}${LIBDIR}"
	@ln -sf -- "libbus.so.${LIB_VERSION}" "${DESTDIR}${LIBDIR}/libbus.so.${LIB_MAJOR}"
	@ln -sf -- "libbus.so.${LIB_VERSION}" "${DESTDIR}${LIBDIR}/libbus.so"

install-a: bin/libbus.a
	@echo INSTALL libbus.a
	@install -dm755 -- "${DESTDIR}${LIBDIR}"
	@install -m644 $^ -- "${DESTDIR}${LIBDIR}"

install-h:
	@echo INSTALL bus.h
	@install -dm755 -- "${DESTDIR}${INCLUDEDIR}"
	@install -m644 src/bus.h -- "${DESTDIR}${INCLUDEDIR}"

install-license:
	@echo INSTALL LICENSE
	@install -dm755 -- "${DESTDIR}${LICENSEDIR}/${PKGNAME}"
	@install -m644 LICENSE -- "${DESTDIR}${LICENSEDIR}/${PKGNAME}"

install-man1: $(foreach M,${MAN1},bin/${M}.1)
	@echo INSTALL $(foreach M,${MAN1},${M}.1)
	@install -dm755 -- "${DESTDIR}${MANDIR}/man1"
	@install -m644 $^ -- "${DESTDIR}${MANDIR}/man1"

install-man3: $(foreach M,${MAN3},bin/${M}.3)
	@echo INSTALL $(foreach M,${MAN3},${M}.3)
	@install -dm755 -- "${DESTDIR}${MANDIR}/man3"
	@install -m644 $^ -- "${DESTDIR}${MANDIR}/man3"
	@ln -sf -- "bus_poll.3" "${DESTDIR}${MANDIR}/man3/bus_poll_start.3"
	@ln -sf -- "bus_poll.3" "${DESTDIR}${MANDIR}/man3/bus_poll_stop.3"

install-man5: $(foreach M,${MAN5},bin/${M}.5)
	@echo INSTALL $(foreach M,${MAN5},${M}.5)
	@install -dm755 -- "${DESTDIR}${MANDIR}/man5"
	@install -m644 $^ -- "${DESTDIR}${MANDIR}/man5"

install-man7: $(foreach M,${MAN7},bin/${M}.7)
	@echo INSTALL $(foreach M,${MAN7},${M}.7)
	@install -dm755 -- "${DESTDIR}${MANDIR}/man7"
	@install -m644 $^ -- "${DESTDIR}${MANDIR}/man7"

install-info: bin/bus.info
	@echo INSTALL bus.info
	@install -dm755 -- "$(DESTDIR)$(INFODIR)"
	@install -m644 $< -- "$(DESTDIR)$(INFODIR)/$(PKGNAME).info"

install-pdf: bin/bus.pdf
	@echo INSTALL bus.pdf
	@install -dm755 -- "$(DESTDIR)$(DOCDIR)"
	@install -m644 $< -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).pdf"

install-dvi: bin/bus.dvi
	@echo INSTALL bus.dvi
	@install -dm755 -- "$(DESTDIR)$(DOCDIR)"
	@install -m644 $< -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).dvi"

install-ps: bin/bus.ps
	@echo INSTALL bus.ps
	@install -dm755 -- "$(DESTDIR)$(DOCDIR)"
	@install -m644 $< -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).ps"

install-examples:
	@echo INSTALL examples
	@install -dm755 -- $(foreach E,$(EXAMPLES),"$(DESTDIR)$(DOCDIR)/$(PKGNAME)/examples/$(E)")
	@$(foreach E,$(EXAMPLES),cp -- $(foreach F,$(EXAMPLE_$(E)),doc/examples/$(E)/$(F))  \
	    "$(DESTDIR)$(DOCDIR)/$(PKGNAME)/examples/$(E)" &&) true



uninstall:
	-rm -- "${DESTDIR}${BINDIR}/bus"
	-rm -- "${DESTDIR}${LIBDIR}/libbus.so.${LIB_VERSION}"
	-rm -- "${DESTDIR}${LIBDIR}/libbus.so.${LIB_MAJOR}"
	-rm -- "${DESTDIR}${LIBDIR}/libbus.so"
	-rm -- "${DESTDIR}${LIBDIR}/libbus.a"
	-rm -- "${DESTDIR}${INCLUDEDIR}/bus.h"
	-rm -- "${DESTDIR}${LICENSEDIR}/${PKGNAME}/LICENSE"
	-rmdir -- "${DESTDIR}${LICENSEDIR}/${PKGNAME}"
	-rm -- $(foreach M,${MAN1},"${DESTDIR}${MANDIR}/man1/${M}.1")
	-rm -- $(foreach M,${MAN3},"${DESTDIR}${MANDIR}/man3/${M}.3")
	-rm -- "${DESTDIR}${MANDIR}/man3/bus_poll_start.3"
	-rm -- "${DESTDIR}${MANDIR}/man3/bus_poll_stop.3"
	-rm -- "${DESTDIR}${MANDIR}/man3/bus_poll_timed.3"
	-rm -- "${DESTDIR}${MANDIR}/man3/bus_read_timed.3"
	-rm -- "${DESTDIR}${MANDIR}/man3/bus_write_timed.3"
	-rm -- $(foreach M,${MAN5},"${DESTDIR}${MANDIR}/man5/${M}.5")
	-rm -- $(foreach M,${MAN7},"${DESTDIR}${MANDIR}/man7/${M}.7")
	-rm -- "$(DESTDIR)$(INFODIR)/$(PKGNAME).info"
	-rm -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).pdf"
	-rm -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).dvi"
	-rm -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).ps"
	-$(foreach E,$(EXAMPLES),rm --  \
	    $(foreach F,$(EXAMPLE_$(E)),"$(DESTDIR)$(DOCDIR)/$(PKGNAME)/examples/$(E)/$(F)");)
	-rmdir -- $(foreach E,$(EXAMPLES),"$(DESTDIR)$(DOCDIR)/$(PKGNAME)/examples/$(E)")



clean:
	@echo cleaning
	@-rm -rf obj bin



.PHONY: default all doc bin bus lib so a man man1 man3 man5 man7 info pdf dvi \
        ps install install-all install-doc install-man install-bin install-so \
        install-a install-h install-lib install-license install-man1 install-bus \
        install-man3 install-man5 install-man7 install-info install-pdf install-dvi \
        install-ps install-examples uninstall clean

