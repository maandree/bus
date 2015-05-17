#include <bus.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define t(stmt)  if (stmt) goto fail



int
main()
{
	bus_t bus;
	const char *message;
	long long tick = 0;
	t(bus_open(&bus, "/tmp/example-bus", BUS_RDONLY));
	t(bus_poll_start(&bus));
	for (;;) {
		message = bus_poll(&bus, BUS_NOWAIT);
		if (message == NULL) {
			t(errno != EAGAIN);
			printf("waiting... %lli\n", ++tick);
			sleep(1);
			continue;
		}
		tick = 0;
		message = strchr(message, ' ') + 1;
		if (!strcmp(message, "stop"))
			break;
		printf("\033[01m%s\033[21m\n", message);
	}
	t(bus_poll_stop(&bus));
	bus_close(&bus);
	return 0;

fail:
	perror("poll");
	bus_poll_stop(&bus);
	bus_close(&bus);
	return 1;
}

