#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define t(stmt)  if (stmt) goto fail


static char arg[4098];


int main(int argc, char *argv[])
{
	bus_t bus;
	if (argc != 3)
		return fprintf(stderr, "This program should be called from ./init\n"), 2;
retry:
	sprintf(arg, "grep '^%s$' < \"${XDG_RUNTIME_DIR}/%s-daemons\" >/dev/null", argv[1], argv[2]);
	if (!WEXITSTATUS(system(arg))) {
		t(bus_open(&bus, getenv("BUS_INIT"), BUS_WRONLY));
		sprintf(arg, "0 %s %s", argv[2], argv[1]);
		t(bus_write(&bus, arg, 0));
		bus_close(&bus);
	} else if (!strcmp(argv[2], "started")) {
		argv[2] = "ready";
		goto retry;
	}
	return 0;

fail:
	perror("test-daemon");
	return 1;
}

