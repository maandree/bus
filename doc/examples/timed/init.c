#include <bus.h>
#include <stdio.h>

int
main()
{
	return bus_create("/tmp/example-bus", 0, NULL) && (perror("init"), 1);
}

