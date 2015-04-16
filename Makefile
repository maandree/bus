# bus - simple message broadcasting IPC system
# See LICENSE file for copyright and license details.

all: bus

bus: bin/bus

bin/bus: obj/cmdline.o obj/bus.o
	@echo CC -o $@
	@mkdir -p bin
	@${CC} -o $@ $^ ${LDFLAGS}

obj/%.o: src/%.c src/*.h
	@echo CC -c $<
	@mkdir -p obj
	@${CC} -Wall -Wextra -pedantic -std=c99 -c -o $@ ${CPPFLAGS} ${CFLAGS} $<

clean:
	@echo cleaning
	@-rm -rf obj bin

.PHONY: all bus clean install uninstall

