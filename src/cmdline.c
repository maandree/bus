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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <grp.h>
#include <pwd.h>



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
	setenv("msg", message, 1);
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
	if ((pid = fork()))
		return pid == -1 ? -1 : 0;
	setenv("msg", message, 1);
	execlp("sh", "sh", "-c", command, NULL);
	perror(argv0);
	exit(1);
	(void) user_data;
}


/**
 * Parse a permission string
 * 
 * @param   str     The permission string
 * @param   andnot  Output paramter for the mask of bits to remove (before applying `*or`)
 * @param   or      Output paramter for the mask of bits to apply
 * @return          0 on success, -1 on error
 */
static int
parse_mode(const char *str, mode_t *andnot, mode_t *or)
{
#define U  S_IRWXU
#define G  S_IRWXG
#define O  S_IRWXO
	const char *s = str;
	int numerical = 1;
	mode_t mode = 0;
	char op = '=';
	int bits;

	*andnot = 0;
	*or = 0;

	if (!*s)
		return errno = 0, -1;

	for (s = str; *s; s++) {
		if (('0' > *s) || (*s > '7')) {
			numerical = 0;
			break;
		} else {
			mode = (mode << 3) | (*s & 15);
		}
	}

	if (numerical) {
		*andnot = U | G | O;
		*or = mode;
		*or &= U | G | O;
		*or = (*or & U) ? (*or | U) : (*or & ~U);
		*or = (*or & G) ? (*or | G) : (*or & ~G);
		*or = (*or & O) ? (*or | O) : (*or & ~O);
		return 0;
	}

	for (s = str; *s; s++) {
		if (strchr("+-=", *s)) {
			op = *s;
		} else if (strchr("ugo", *s)) {
			if (*s == 'u')
				bits = U;
			else if (*s == 'g')
				bits = G;
			else
				bits = O;
			if (op == '+') {
				*andnot |= bits;
				*or |= bits;
			}
			else if (op == '-') {
				*andnot |= bits;
				*or &= ~bits;
			}
			else if (op == '=') {
				*andnot |= U | G | O;
				*or |= bits;
			}
		} else {
			return errno = 0, -1;
		}
	}

	return 0;
}


/**
 * Parse a user name/identifier string
 * 
 * @param   str  The user's name or identifier
 * @param   uid  Output parameter for the user's identifier
 * @return       0 on success, -1 on error
 */
static int
parse_uid(const char *str, uid_t *uid)
{
	const char *s = str;
	int numerical = 1;
	uid_t rc = 0;
	struct passwd *pwd;

	if (!*s || (*s == ':'))
		return errno = 0, -1;

	for (s = str; *s; s++) {
		if (('0' > *s) || (*s > '9')) {
			numerical = 0;
			break;
		}
	}

	if (numerical) {
		for (s = str; *s; s++)
			rc = (rc * 10) + (*s & 15);
		*uid = rc;
		return 0;
	}

	pwd = getpwnam(str);
	if (!pwd) {
		return -1;
	}
	*uid = pwd->pw_uid;
	return 0;
}


/**
 * Parse a group name/identifier string
 * 
 * @param   str  The group's name or identifier
 * @param   gid  Output parameter for the group's identifier
 * @return       0 on success, -1 on error
 */
static int
parse_gid(const char *str, gid_t *gid)
{
	const char *s = str;
	int numerical = 1;
	gid_t rc = 0;
	struct group *grp;

	if (!*s || strchr(s, ':'))
		return errno = 0, -1;

	for (s = str; *s; s++) {
		if (('0' > *s) || (*s > '9')) {
			numerical = 0;
			break;
		}
	}

	if (numerical) {
		for (s = str; *s; s++)
			rc = (rc * 10) + (*s & 15);
		*gid = rc;
		return 0;
	}

	grp = getgrnam(str);
	if (!grp)
		return -1;
	*gid = grp->gr_gid;
	return 0;
}


/**
 * Parse a ownership string
 * 
 * @param   str  The ownership string
 * @param   uid  Output parameter for the owner, `NULL` if `str` only contains the group
 * @param   gid  Output parameter for the group, `NULL` if `str` only contains the owner
 * @return       0 on success, -1 on error
 */
static int
parse_owner(char *str, uid_t *uid, gid_t *gid)
{
	int r = 0;
	char* group;

	if (!uid)
		return parse_gid(str, gid);
	if (!gid)
		return parse_uid(str, uid);

	group = strchr(str, ':');
	*group++ = 0;

	r = parse_gid(group, gid);
	if (r)
		return r;
	return parse_uid(str, uid);
}



/**
 * Main function of the command line interface for the bus system
 * 
 * @param   argc  The number of elements in `argv`
 * @param   argv  The command. Valid commands:
 *                  <argv0> create [<path>]                 # create a bus
 *                  <argv0> remove <path>                   # remove a bus
 *                  <argv0> listen <path> <command>         # listen for new messages
 *                  <argv0> wait <path> <command>           # listen for one new message
 *                  <argv0> broadcast <path> <message>      # broadcast a message
 *                  <argv0> chmod <mode> <path>             # change permissions
 *                  <argv0> chown <owner>[:<group>] <path>  # change ownership
 *                  <argv0> chgrp <group> <path>            # change group
 *                <command> will be spawned with $arg set to the message
 * @return        0 on sucess, 1 on error, 2 on invalid command
 */
int
main(int argc, char *argv[])
{
	bus_t bus;
	char *file;
	struct stat attr;
	uid_t uid;
	gid_t gid;
	mode_t mode_andnot, mode_or;

	argv0 = *argv;

	/* Create a new bus with selected name. */
	if ((argc == 3) && !strcmp(argv[1], "create")) {
		t(bus_create(argv[2], 0, NULL));
		/* TODO add -x */

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
		t(bus_write(&bus, argv[3], 0));
		t(bus_close(&bus));
		/* TODO add -n */

	/* Change permissions. */
	} else if ((argc == 4) && !strcmp(argv[1], "chmod")) {
		t(parse_mode(argv[2], &mode_andnot, &mode_or));
		t(stat(argv[3], &attr));
		attr.st_mode &= ~mode_andnot;
		attr.st_mode |= mode_or;
		t(bus_chmod(argv[3], attr.st_mode));

	/* Change ownership. */
	} else if ((argc == 4) && !strcmp(argv[1], "chown")) {
		if (strchr(argv[2], ':')) {
			t(parse_owner(argv[2], &uid, &gid));
			t(bus_chown(argv[3], uid, gid));
		} else {
			t(parse_owner(argv[2], &uid, NULL));
			t(stat(argv[3], &attr));
			t(bus_chown(argv[3], uid, attr.st_gid));
		}

	/* Change group. */
	} else if ((argc == 4) && !strcmp(argv[1], "chgrp")) {
		t(parse_owner(argv[2], NULL, &gid));
		t(stat(argv[3], &attr));
		t(bus_chown(argv[3], attr.st_uid, gid));

	} else
		return 2;

	return 0;

fail:
	if (errno == 0)
		return 2;
	perror(argv0);
	return 1;
}

