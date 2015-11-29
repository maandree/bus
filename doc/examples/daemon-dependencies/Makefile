COMMANDS = announce await-ready await-started cleanup init require start-daemon test-daemon

all: ${COMMANDS}

%: %.c
	${CC} -Wall -Wextra -pedantic -std=c99 -lbus -o $@ $<

clean:
	-rm ${COMMANDS}
	-rm -r run


.PHONY: all clean

