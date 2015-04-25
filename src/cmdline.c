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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>



/**
 * Statement wrapper that goes to `fail` on failure
 */
#define t(inst)  if ((inst) == -1)  goto fail



/**
 * The name of the process
 */
char *argv0;

/**
 * The command to spawn when a message is received
 */
static const char *command;



/**
 * Spawn a command because a message has been received
 * 
 * @param   message    The received message
 * @param   user_data  Not used
 * @return             1 (continue listening) on success, -1 on error
 */
static int
spawn_continue(const char *message, void *user_data)
{
	pid_t pid;
	if (!message)
		return 1;
	if ((pid = fork()))
		return pid == -1 ? -1 : 1;
	setenv("arg", message, 1);
	execlp("sh", "sh", "-c", command, NULL);
	perror(argv0);
	exit(1);
	(void) user_data;
}


/**
 * Spawn a command because a message has been received
 * 
 * @param   message    The received message
 * @param   user_data  Not used
 * @return             0 (stop listening) on success, -1 on error, or 1 if `message` is `NULL`
 */
static int
spawn_break(const char *message, void *user_data)
{
	pid_t pid;
	if (!message)
		return 1;
	if (pid = fork())
		return pid == -1 ? -1 : 0;
	setenv("arg", message, 1);
	execlp("sh", "sh", "-c", command, NULL);
	perror(argv0);
	exit(1);
	(void) user_data;
}



/**
 * Main function of the command line interface for the bus system
 * 
 * @param   argc  The number of elements in `argv`
 * @param   argv  The command. Valid commands:
 *                  <argv0> create [<path>]             # create a bus
 *                  <argv0> remove <path>               # remove a bus
 *                  <argv0> listen <path> <command>     # listen for new messages
 *                  <argv0> wait <path> <command>       # listen for one new message
 *                  <argv0> broadcast <path> <message>  # broadcast a message
 *                <command> will be spawned with $arg set to the message
 * @return        0 on sucess, 1 on error, 2 on invalid command
 */
int
main(int argc, char *argv[])
{
	bus_t bus;
	char *file;

	argv0 = *argv;

	/* Create a new bus with selected name. */
	if ((argc == 3) && !strcmp(argv[1], "create")) {
		t(bus_create(argv[2], 0, NULL));

	/* Create a new bus with random name. */
	} else if ((argc == 2) && !strcmp(argv[1], "create")) {
		t(bus_create(NULL, 0, &file));
		printf("%s\n", file);
		free(file);

	/* Remove a bus. */
	} else if ((argc == 3) && !strcmp(argv[1], "remove")) {
		t(bus_unlink(argv[2]));

	/* Listen on a bus in a loop. */
	} else if ((argc == 4) && !strcmp(argv[1], "listen")) {
		command = argv[3];
		t(bus_open(&bus, argv[2], BUS_RDONLY));
		t(bus_read(&bus, spawn_continue, NULL));
		t(bus_close(&bus));

	/* Listen on a bus for one message. */
	} else if ((argc == 4) && !strcmp(argv[1], "wait")) {
		command = argv[3];
		t(bus_open(&bus, argv[2], BUS_RDONLY));
		t(bus_read(&bus, spawn_break, NULL));
		t(bus_close(&bus));

	/* Broadcast a message on a bus. */
	} else if ((argc == 4) && !strcmp(argv[1], "broadcast")) {
		t(bus_open(&bus, argv[2], BUS_WRONLY));
		t(bus_write(&bus, argv[3]));
		t(bus_close(&bus));

	} else
		return 2;

	return 0;

fail:
	perror(argv0);
	return 1;
}

