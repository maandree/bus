#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define t(stmt)  if (stmt) goto fail


static char arg[4098];


int main(int argc, char *argv[])
{
	bus_t bus;
	if (argc < 3)
		return fprintf(stderr, "USAGE: %s state daemon", *argv), 2;
	t(bus_open(&bus, getenv("BUS_INIT"), BUS_WRONLY));
	sprintf(arg, "%ji %s %s", (intmax_t)getppid(), argv[1], argv[2]);
	t(bus_write(&bus, arg));
	t(bus_close(&bus));
	return 0;

fail:
	perror("announce");
	return 1;
}
