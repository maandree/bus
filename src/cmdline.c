/**
 * MIT/X Consortium License
 * 
 * Copyright © 2015  Mattias Andrée <maandree@member.fsf.org>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "bus.h"


/**
 * Statement wrapper that goes to `fail` on failure
 */
#define t(inst)  if ((inst) == -1)  goto fail


char *argv0;
static const char *command;


static int
get_keys(void)
{
	int saved_errno;
	char *line;
	size_t len;

	line = NULL, len = 0;
	t(getline(&line, &len, stdin));
	t(key_sem = (key_t)atoll(line));
	free(line);

	line = NULL, len = 0;
	t(getline(&line, &len, stdin));
	t(key_shm = (key_t)atoll(line));
	free(line);

	return 0;

fail:
	saved_errno = errno;
	free(line);
	errno = saved_errno;
	return -1;
}


static int
spawn_continue(const char *message)
{
	pid_t pid = fork();
	if (pid)
		return pid == -1 ? -1 : 1;
	setenv("arg", message, 1);
	execlp("sh", "sh", "-c", command, NULL);
	perror(argv0);
	exit(1);
}


static int
spawn_break(const char *message)
{
	pid_t pid = fork();
	if (pid)
		return pid == -1 ? -1 : 0;
	setenv("arg", message, 1);
	execlp("sh", "sh", "-c", command, NULL);
	perror(argv0);
	exit(1);
}


int
main(int argc, char *argv[])
{
	bus_t bus;

	argv0 = *argv;

	if ((argc == 3) && !strcmp(argv[1], "create")) {
		t(bus_create(argv[2], 0) ? 0 : -1);

	} else if ((argc == 2) && !strcmp(argv[1], "create")) {
		char *file = bus_create(NULL, 0);
		t(file ? 0 : -1);
		printf("%s\n", file);

	} else if ((argc == 3) && !strcmp(argv[1], "remove")) {
		t(bus_unlink(argv[2]));

	} else if ((argc == 4) && !strcmp(argv[1], "listen")) {
		command = argv[3];
		t(bus_open(&bus, argv[2], BUS_RDONLY));
		t(bus_read(&bus, spawn_continue, NULL));
		t(bus_close(&bus));

	} else if ((argc == 4) && !strcmp(argv[1], "wait")) {
		command = argv[3];
		t(bus_open(&bus, argv[2], BUS_RDONLY));
		t(bus_read(&bus, spawn_break, NULL));
		t(bus_close(&bus));

	} else if ((argc == 4) && !strcmp(argv[1], "broadcast")) {
		t(bus_open(&bus, argv[2], BUS_WRONLY));
		t(bus_write(&bus, argv[3]))
		t(bus_close(&bus));

	} else
		return 2;

	return 0;

fail:
	perror(argv0);
	return 1;
}


#undef t

