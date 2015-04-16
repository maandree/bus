# bus - simple message broadcasting IPC system
# See LICENSE file for copyright and license details.

FLAGS = -std=c99 -Wall -Wextra -pedantic -O2

LIB_MAJOR = 1
LIB_MINOR = 0
LIB_VERSION = ${LIB_MAJOR}.${LIB_MINOR}


all: bus

bus: bin/bus bin/libbus.so.$(LIB_VERSION) bin/libbus.so.$(LIB_MAJOR) bin/libbus.so bin/libbus.a

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

.PHONY: all bus clean install uninstall

