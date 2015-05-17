#include <bus.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#define t(stmt)  if (stmt) goto fail



static char message[BUS_MEMORY_SIZE];



int
main(int argc, char *argv[])
{
	bus_t bus;
	struct timespec timeout;
	if (argc < 2) {
		fprintf(stderr, "%s: USAGE: %s message\n", argv[0], argv[0]);
		return 2;
	}
	sprintf(message, "0 %s", argv[1]);
	t(bus_open(&bus, "/tmp/example-bus", BUS_WRONLY));
	t(clock_gettime(CLOCK_MONOTONIC, &timeout));
	timeout.tv_nsec += 100000000L;
	t(bus_write_timed(&bus, message, &timeout, CLOCK_MONOTONIC));
	bus_close(&bus);
	return 0;

fail:
	perror("write");
	bus_close(&bus);
	return 1;
}

