#include <bus.h>
#include <stdio.h>
#include <stdlib.h>

#define t(stmt)  if (stmt) goto fail



int
main()
{
	char *bus_address = getenv("BUS_INIT");
	if (!bus_address || !*bus_address) {
		fprintf(stderr, "$BUS_INIT has not been set, its export statement "
			"should have been printed in bold red by ./init\n");
		return 1;
	}
	t(bus_unlink(bus_address));
	return 0;

fail:
	perror("cleanup");
	return 1;
}

