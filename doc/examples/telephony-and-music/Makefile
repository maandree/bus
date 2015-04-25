COMMANDS = init cleanup monitor end-call receive-or-make-call

all: ${COMMANDS}

%: %.c
	${CC} -Wall -Wextra -pedantic -std=c99 -lbus -o $@ $<

clean:
	-rm ${COMMANDS}


.PHONY: all clean

