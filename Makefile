# bus - simple message broadcasting IPC system
# See LICENSE file for copyright and license details.

FLAGS = -std=c99 -Wall -Wextra -pedantic -O2

LIB_MAJOR = 1
LIB_MINOR = 0
LIB_VERSION = ${LIB_MAJOR}.${LIB_MINOR}
VERSION = 1.0


all: bus doc
doc: man
man: man1 man3 man5 man7

bus: bin/bus bin/libbus.so.$(LIB_VERSION) bin/libbus.so.$(LIB_MAJOR) bin/libbus.so bin/libbus.a
man1: bin/bus.1 bin/bus-broadcast.1 bin/bus-create.1 bin/bus-listen.1 bin/bus-remove.1 bin/bus-wait.1
man3: bin/bus_create.3 bin/bus_unlink.3 bin/bus_open.3 bin/bus_close.3 bin/bus_read.3 bin/bus_write.3
man5: bin/bus.5
man7: bin/libbus.7

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

clean:
	@echo cleaning
	@-rm -rf obj bin

.PHONY: all doc bus man man1 clean install uninstall

