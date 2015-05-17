COMMANDS = init cleanup write poll

all: ${COMMANDS}

%: %.c
	${CC} -Wall -Wextra -pedantic -std=c99 -lbus -o $@ $<

clean:
	-rm ${COMMANDS}


.PHONY: all clean

