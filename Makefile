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

PKGNAME = bus


MAN1 = bus bus-broadcast bus-create bus-listen bus-remove bus-wait
MAN3 = bus_create bus_unlink bus_open bus_close bus_read bus_write
MAN5 = bus
MAN7 = libbus


FLAGS = -std=c99 -Wall -Wextra -pedantic -O2

LIB_MAJOR = 1
LIB_MINOR = 0
LIB_VERSION = ${LIB_MAJOR}.${LIB_MINOR}
VERSION = 1.0


all: bus doc
doc: man
man: man1 man3 man5 man7

bus: bin/bus bin/libbus.so.${LIB_VERSION} bin/libbus.so.${LIB_MAJOR} bin/libbus.so bin/libbus.a
man1: $(foreach M,${MAN1},bin/${M}.1)
man3: $(foreach M,${MAN3},bin/${M}.3)
man5: $(foreach M,${MAN5},bin/${M}.5)
man7: $(foreach M,${MAN7},bin/${M}.7)

bin/%.1: doc/%.1
	@echo SED $@
	@mkdir -p bin
	@sed 's/%VERSION%/${VERSION}/g' < $< > $@

bin/%.3: doc/%.3
	@echo SED $@
	@mkdir -p bin
	@sed 's/%VERSION%/${VERSION}/g' < $< > $@

bin/%.5: doc/%.5
	@echo SED $@
	@mkdir -p bin
	@sed 's/%VERSION%/${VERSION}/g' < $< > $@

bin/%.7: doc/%.7
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
	@${CC} ${FLAGS} -o $@ $^ ${LDFLAGS}

bin/libbus.so.${LIB_VERSION}: obj/bus-fpic.o
	@echo CC -o $@
	@mkdir -p bin
	@${CC} ${FLAGS} -shared -Wl,-soname,libbus.so.${LIB_MAJOR} -o $@ $^ ${LDFLAGS}

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

install: install-bin install-so install-a install-h install-license install-doc
install-doc: install-man
install-man: install-man1 install-man3 install-man5 install-man7

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
	@echo INSTALL $(foreach M,${MAN1},${M}.3)
	@install -dm755 -- "${DESTDIR}${MANDIR}/man3"
	@install -m644 $^ -- "${DESTDIR}${MANDIR}/man3"

install-man5: $(foreach M,${MAN5},bin/${M}.5)
	@echo INSTALL $(foreach M,${MAN1},${M}.5)
	@install -dm755 -- "${DESTDIR}${MANDIR}/man5"
	@install -m644 $^ -- "${DESTDIR}${MANDIR}/man5"

install-man7: $(foreach M,${MAN7},bin/${M}.7)
	@echo INSTALL $(foreach M,${MAN1},${M}.7)
	@install -dm755 -- "${DESTDIR}${MANDIR}/man7"
	@install -m644 $^ -- "${DESTDIR}${MANDIR}/man7"

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
	-rm -- $(foreach M,${MAN5},"${DESTDIR}${MANDIR}/man5/${M}.5")
	-rm -- $(foreach M,${MAN7},"${DESTDIR}${MANDIR}/man7/${M}.7")

clean:
	@echo cleaning
	@-rm -rf obj bin

.PHONY: all doc bus man man1 clean install install-bin install-so install-a install-h include-license install-doc install-man install-man1 install-man3 install-man5 install-man7 uninstall

