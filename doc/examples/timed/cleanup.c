#include <bus.h>
#include <stdio.h>

int main()
{
	return bus_unlink("/tmp/example-bus") && (perror("cleanup"), 1);
}

