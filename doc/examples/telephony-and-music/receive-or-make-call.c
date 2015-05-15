#include <bus.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#define t(stmt)  if (stmt) goto fail



static char message[BUS_MEMORY_SIZE];



int main()
{
	bus_t bus;
	sprintf(message, "%ji force-pause", (intmax_t)getppid());
	/* Yes, PPID; in this example we pretend the shell is the telephony process. */
	t(bus_open(&bus, "/tmp/example-bus", BUS_WRONLY));
	t(bus_write(&bus, message, 0));
	bus_close(&bus);
	return 0;

fail:
	perror("receive-or-make-call");
	bus_close(&bus);
	return 1;
}

