# bus - simple message broadcasting IPC system
# See LICENSE file for copyright and license details.

FLAGS = -std=c99 -Wall -Wextra -pedantic -O2


all: bus

bus: bin/bus bin/libbus.so

bin/bus: obj/cmdline-nofpic.o obj/bus-nofpic.o
	@echo CC -o $@
	@mkdir -p bin
	@${CC} ${FLAGS} -o $@ $^ ${LDFLAGS}

bin/libbus.so: obj/bus-fpic.o
	@echo CC -o $@
	@mkdir -p bin
	@${CC} ${FLAGS} -shared -o $@ $^ ${LDFLAGS}

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

