#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define t(stmt)  if (stmt) goto fail


static char arg[4098];
static int argc;
static char **argv;
static int remaining = 0;
static char *started = NULL;
static char msg[BUS_MEMORY_SIZE];


static void announce_wait(pid_t pid)
{
	bus_t bus;
	int i;
	t(bus_open(&bus, getenv("BUS_INIT"), BUS_WRONLY));
	for (i = 1; i < argc; i++) {
		if (!started[i]) {
			sprintf(arg, "%ji awaiting-ready %s", (intmax_t)pid, argv[i]);
			t(bus_write(&bus, arg));
		}
	}
	t(bus_close(&bus));
	return;

fail:
	perror("await-ready");
}


static int callback(const char *message, void *user_data)
{
	int i;
	char *arg2;
	char *arg3;
	pid_t pid;
	pid_t ppid;

	if (!message) {
		ppid = getppid();
		pid = fork();
		if (pid == 0) {
			if (fork() == 0)
				announce_wait(ppid);
			exit(0);
		} else {
			(void) waitpid(pid, NULL, 0); /* Let's pretend everything will go swimmingly. */
		}
		return 1;
	}

	strncpy(msg, message, BUS_MEMORY_SIZE - 1);
	msg[BUS_MEMORY_SIZE - 1] = 0;

	arg2 = strchr(msg, ' ');
	if (!arg2)
		return 1;
	arg3 = strchr(++arg2, ' ');
	if (!arg3)
		return 1;
	*arg3++ = 0;

	if (strcmp(arg2, "ready"))
		return 1;

	for (i = 1; i < argc; i++)
		if (!started[i] && !strcmp(argv[i], arg3))
			started[i] = 1, remaining--;

	return !!remaining;
	(void) user_data;
}


int main(int argc_, char *argv_[])
{
	bus_t bus;
	int i;

	argc = argc_;
	argv = argv_;

	if (argc < 2)
		return fprintf(stderr, "USAGE: %s daemon...", *argv), 2;
	t(bus_open(&bus, getenv("BUS_INIT"), BUS_RDONLY));
	started = calloc(argc, sizeof(char));
	t(started == NULL);

	started[0] = 1;
	for (i = 1; i < argc; i++) {
		sprintf(arg, "grep '^%s$' < \"${XDG_RUNTIME_DIR}/ready-daemons\" >/dev/null", argv[i]);
		if (!WEXITSTATUS(system(arg)))
			started[i] = 1;
		else
		        remaining++;
	}

	if (remaining)
		bus_read(&bus, callback, NULL);

	bus_close(&bus);
	free(started);
	return 0;

fail:
	perror("await-ready");
	bus_close(&bus);
	free(started);
	return 1;
}

