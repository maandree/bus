#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define t(stmt)  if (stmt) goto fail


static char arg[4098];


int
main(int argc, char *argv[])
{
	bus_t bus;
	int i;
	if (argc < 2)
		return fprintf(stderr, "USAGE: %s daemon...", *argv), 2;
	t(bus_open(&bus, getenv("BUS_INIT"), BUS_WRONLY));

	for (i = 1; i < argc; i++) {
		sprintf(arg, "grep '^%s$' < \"${XDG_RUNTIME_DIR}/started-daemons\" >/dev/null", argv[i]);
		if (WEXITSTATUS(system(arg))) {
			sprintf(arg, "%ji require %s", (intmax_t)getppid(), argv[i]);
			t(bus_write(&bus, arg, 0));
		}
	}

	bus_close(&bus);
	return 0;

fail:
	perror("require");
	bus_close(&bus);
	return 1;
}

