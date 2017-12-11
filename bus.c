/* See LICENSE file for copyright and license details. */
#include "bus.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arg.h"



/**
 * Statement wrapper that goes to `fail` on failure
 */
#define t(inst) do { if ((inst) == -1) goto fail; } while (0)



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
#define U S_IRWXU
#define G S_IRWXG
#define O S_IRWXO
	const char *s = str;
	int numerical = 1;
	mode_t mode = 0;
	char op = '=';
	mode_t bits;

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
		*or = (*or & U) ? (*or | U) : (*or & (mode_t)~U);
		*or = (*or & G) ? (*or | G) : (*or & (mode_t)~G);
		*or = (*or & O) ? (*or | O) : (*or & (mode_t)~O);
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
 *                  <argv0> create [-x] [--] [<path>]             # create a bus
 *                  <argv0> remove [--] <path>                    # remove a bus
 *                  <argv0> listen [--] <path> <command>          # listen for new messages
 *                  <argv0> wait [--] <path> <command>            # listen for one new message
 *                  <argv0> broadcast [-n] [--] <path> <message>  # broadcast a message
 *                  <argv0> chmod [--] <mode> <path>              # change permissions
 *                  <argv0> chown [--] <owner>[:<group>] <path>   # change ownership
 *                  <argv0> chgrp [--] <group> <path>             # change group
 *                <command> will be spawned with $arg set to the message
 * @return        0 on sucess, 1 on error, 2 on invalid command
 */
int
main(int argc, char *argv[])
{
	int xflag = 0;
	int nflag = 0;
	bus_t bus;
	char *file;
	struct stat attr;
	uid_t uid;
	gid_t gid;
	mode_t mode_andnot, mode_or;

	/* Parse arguments. */
	ARGBEGIN {
	case 'x':
		xflag = 1;
		break;
	case 'n':
		nflag = 1;
		break;
	default:
		return 2;
	} ARGEND;

	/* Check options. */
	if (xflag && strcmp(argv[0], "create") && (argc != 2))
		return 2;
	if (nflag && strcmp(argv[0], "broadcast") && (argc != 3))
		return 2;

	/* Create a new bus with selected name. */
	if ((argc == 2) && !strcmp(argv[0], "create")) {
		t(bus_create(argv[1], xflag * BUS_EXCL, NULL));

	/* Create a new bus with random name. */
	} else if ((argc == 1) && !strcmp(argv[0], "create")) {
		t(bus_create(NULL, 0, &file));
		printf("%s\n", file);
		free(file);

	/* Remove a bus. */
	} else if ((argc == 2) && !strcmp(argv[0], "remove")) {
		t(bus_unlink(argv[1]));

	/* Listen on a bus in a loop. */
	} else if ((argc == 3) && !strcmp(argv[0], "listen")) {
		command = argv[2];
		t(bus_open(&bus, argv[1], BUS_RDONLY));
		t(bus_read(&bus, spawn_continue, NULL));
		t(bus_close(&bus));

	/* Listen on a bus for one message. */
	} else if ((argc == 3) && !strcmp(argv[0], "wait")) {
		command = argv[2];
		t(bus_open(&bus, argv[1], BUS_RDONLY));
		t(bus_read(&bus, spawn_break, NULL));
		t(bus_close(&bus));

	/* Broadcast a message on a bus. */
	} else if ((argc == 3) && !strcmp(argv[0], "broadcast")) {
		t(bus_open(&bus, argv[1], BUS_WRONLY));
		t(bus_write(&bus, argv[2], nflag * BUS_NOWAIT));
		t(bus_close(&bus));

	/* Change permissions. */
	} else if ((argc == 3) && !strcmp(argv[0], "chmod")) {
		t(parse_mode(argv[1], &mode_andnot, &mode_or));
		t(stat(argv[2], &attr));
		attr.st_mode &= ~mode_andnot;
		attr.st_mode |= mode_or;
		t(bus_chmod(argv[2], attr.st_mode));

	/* Change ownership. */
	} else if ((argc == 3) && !strcmp(argv[0], "chown")) {
		if (strchr(argv[1], ':')) {
			t(parse_owner(argv[1], &uid, &gid));
			t(bus_chown(argv[2], uid, gid));
		} else {
			t(parse_owner(argv[1], &uid, NULL));
			t(stat(argv[2], &attr));
			t(bus_chown(argv[2], uid, attr.st_gid));
		}

	/* Change group. */
	} else if ((argc == 3) && !strcmp(argv[0], "chgrp")) {
		t(parse_owner(argv[1], NULL, &gid));
		t(stat(argv[2], &attr));
		t(bus_chown(argv[2], attr.st_uid, gid));

	} else
		return 2;

	return 0;

fail:
	if (!errno)
		return 2;
	perror(argv0);
	return 1;
}
