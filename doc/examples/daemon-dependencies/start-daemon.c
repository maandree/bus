#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define t(stmt)  if (stmt) goto fail


static char arg[4098];


int
main(int argc, char *argv[])
{
	if (argc != 2)
		return fprintf(stderr, "This program should be called from ./init\n"), 2;

	sprintf(arg, "grep '^%s$' < \"${XDG_RUNTIME_DIR}/started-daemons\" >/dev/null", argv[1]);
	if (!WEXITSTATUS(system(arg)))
		return 0;
	sprintf(arg, "grep '^%s$' < \"${XDG_RUNTIME_DIR}/ready-daemons\" >/dev/null", argv[1]);
	if (!WEXITSTATUS(system(arg)))
		return 0;

	sprintf(arg, "./%s", argv[1]);
	execlp(arg, arg, NULL);
	perror("start-daemon");
	return 1;
}

