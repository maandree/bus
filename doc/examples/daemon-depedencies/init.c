#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define t(stmt)  if (stmt) goto fail


static char msg[BUS_MEMORY_SIZE];
static int argc;
static char **argv;
static char arg[4098];


static void
start_daemons()
{
	int i;
	for (i = 1; i < argc; i++)
		if (fork() == 0)
			execl("./start-daemon", "./start-daemon", argv[i], NULL); 
}


static int
callback(const char *message, void *user_data)
{
	pid_t pid;
	char *arg2;
	char *arg3;
	if (!message) {
		pid = fork();
		t(pid == -1);
		if (pid == 0) {
			if (fork() == 0) {
				start_daemons();
			}
			exit(0);
		} else {
			(void) waitpid(pid, NULL, 0); /* Let's pretend everything will go swimmingly. */
		}
		return 1;
	}

	strncpy(msg, message, BUS_MEMORY_SIZE - 1);
	msg[BUS_MEMORY_SIZE - 1] = 0;

	pid = fork();
	t(pid == -1);

	if (pid == 0) {
		if (fork() == 0) {
			arg2 = strchr(msg, ' ');
			if (arg2 == NULL)
				exit(0);
			arg3 = strchr(++arg2, ' ');
			if (arg3 == NULL)
				exit(0);
			*arg3++ = 0;
			if (!strcmp(arg2, "require")) {
				execl("./start-daemon", "./start-daemon", arg3, NULL);
			} else if (!strcmp(arg2, "awaiting-started")) {
				execl("./test-daemon", "./test-daemon", arg3, "started", NULL);
			} else if (!strcmp(arg2, "awaiting-ready") || !strcmp(arg2, "awaiting")) {
				execl("./test-daemon", "./test-daemon", arg3, "ready", NULL);
			} else if (!strcmp(arg2, "started")) {
				sprintf(arg,
					"grep '^%s\\$' < \"${XDG_RUNTIME_DIR}/started-daemons\" >/dev/null || "
					"echo %s >> \"${XDG_RUNTIME_DIR}/started-daemons\"",
					arg3, arg3);
				execlp("sh", "sh", "-c", arg, NULL);
			} else if (!strcmp(arg2, "ready")) {
				sprintf(arg,
					"grep '^%s\\$' < \"${XDG_RUNTIME_DIR}/ready-daemons\" >/dev/null || "
					"echo %s >> \"${XDG_RUNTIME_DIR}/ready-daemons\"",
					arg3, arg3);
				execlp("sh", "sh", "-c", arg, NULL);
			}
		}
		exit(0);
	} else {
		(void) waitpid(pid, NULL, 0); /* Let's pretend everything will go swimmingly. */
	}

	return 1;
	(void) user_data;

fail:
	perror("init");
	return -1;
}


int
main(int argc_, char *argv_[])
{
	char *bus_address = NULL;
	bus_t bus;
	argv = argv_;
	argc = argc_;
	if (argc < 2) {
		fprintf(stderr, "USAGE: %s daemon...\n", *argv);
		return 1;
	}
	t(setenv("XDG_RUNTIME_DIR", "./run", 1));  /* Real init systems with not have the period. */
	system("mkdir -p -- \"${XDG_RUNTIME_DIR}\"");
	system("truncate -s 0 -- \"${XDG_RUNTIME_DIR}/started-daemons\"");
	system("truncate -s 0 -- \"${XDG_RUNTIME_DIR}/ready-daemons\"");
	t(bus_create(NULL, 1, &bus_address));
	fprintf(stderr, "\033[00;01;31mexport BUS_INIT=%s\033[00m\n", bus_address);
	fprintf(stderr, "\033[00;31mexport XDG_RUNTIME_DIR=./run\033[00m\n");
	t(setenv("BUS_INIT", bus_address, 1));
	t(bus_open(&bus, bus_address, BUS_RDONLY));
	t(bus_read(&bus, callback, NULL));
	bus_close(&bus);
	free(bus_address);
	return 0;

fail:
	perror("init");
	bus_close(&bus);
	free(bus_address);
	return 1;
}

