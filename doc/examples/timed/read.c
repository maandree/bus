#include <bus.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define t(stmt)  if (stmt) goto fail



static int
callback(const char *message, void *user_data)
{
	(void) user_data;

	if (message == NULL)
		return 1;

	message = strchr(message, ' ') + 1;
	if (!strcmp(message, "stop"))
		return 0;
	printf("%s\n", message);
	return 1;
}


int
main()
{
	bus_t bus;
	struct timespec timeout;
	t(bus_open(&bus, "/tmp/example-bus", BUS_RDONLY));
	t(clock_gettime(CLOCK_MONOTONIC, &timeout));
	timeout.tv_sec += 10;
	t(bus_read_timed(&bus, callback, NULL, &timeout, CLOCK_MONOTONIC));
	bus_close(&bus);
	return 0;

fail:
	perror("poll");
	bus_poll_stop(&bus);
	bus_close(&bus);
	return 1;
}

