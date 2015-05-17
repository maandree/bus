COMMANDS = init cleanup write poll read

all: ${COMMANDS}

%: %.c
	${CC} -Wall -Wextra -pedantic -std=c99 -lbus -o $@ $<

clean:
	-rm ${COMMANDS}


.PHONY: all clean

