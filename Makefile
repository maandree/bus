# bus - simple message broadcasting IPC system
# See LICENSE file for copyright and license details.

all: bus

bus: bin/bus

bin/bus: obj/cmdline.o
	@echo CC -o $@
	@mkdir -p bin
	@${CC} -o $@ $^ ${LDFLAGS}

obj/%.o: src/%.c
	@echo CC -c $<
	@mkdir -p obj
	@${CC} -c -o $@ ${CPPFLAGS} ${CFLAGS} $<

clean:
	@echo cleaning
	@-rm -rf obj bin

.PHONY: all bus clean install uninstall

