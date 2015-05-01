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
#ifndef BUS_H
#define BUS_H


#ifndef _DEFAULT_SOURCE
# define _DEFAULT_SOURCE
#endif
#include <sys/types.h>



/**
 * Open the bus for reading only
 */
#define BUS_RDONLY  1

/**
 * Open the bus for writing only
 */
#define BUS_WRONLY  0

/**
 * Open the bus for both reading and writing only
 */
#define BUS_RDWR  0

/**
 * Fail to create bus if its file already exists
 */
#define BUS_EXCL  2

/**
 * Fail if interrupted
 */
#define BUS_INTR  4

/**
 * Function shall fail with errno set to `EAGAIN`
 * if the it would block and this flag is used
 */
#define BUS_NOWAIT  1



/**
 * The number of bytes in storeable in the shared memory,
 * note that this includes the NUL-termination.
 * This means that message can be at most one byte smaller.
 */
#define BUS_MEMORY_SIZE  2048



/**
 * Bus information
 */
typedef struct bus
{
	/**
	 * The key for the semaphore array
	 */
	key_t key_sem;

	/**
	 * The key for the 
	 */
	key_t key_shm;

	/**
	 * The ID of the semaphore array
	 */
	int sem_id;

	/**
	 * The address of the shared memory
	 */
	char *message;

	/**
	 * Non-zero if and only if `bus_poll` has not been
	 * called since the last `bus_poll_start`, or
	 * if `bus_poll` failed during reading
	 */
	int first_poll;
} bus_t;



/**
 * Create a new bus
 * 
 * @param   file      The pathname of the bus, `NULL` to create a random one
 * @param   flags     `BUS_EXCL` (if `file` is not `NULL`) to fail if the file
 *                    already exists, otherwise if the file exists, nothing
 *                    will happen;
 *                    `BUS_INTR` to fail if interrupted
 * @param   out_file  Output parameter for the pathname of the bus
 * @return            0 on success, -1 on error
 */
int bus_create(const char *file, int flags, char **out_file);

/**
 * Remove a bus
 * 
 * @param   file  The pathname of the bus
 * @return        0 on success, -1 on error
 */
int bus_unlink(const char *file);


/**
 * Open an existing bus
 * 
 * @param   bus    Bus information to fill
 * @param   file   The filename of the bus
 * @param   flags  `BUS_RDONLY`, `BUS_WRONLY` or `BUS_RDWR`,
 *                 the value must not be negative
 * @return         0 on success, -1 on error
 */
int bus_open(bus_t *bus, const char *file, int flags);

/**
 * Close a bus
 * 
 * @param   bus  Bus information
 * @return       0 on success, -1 on error
 */
int bus_close(bus_t *bus);


/**
 * Broadcast a message a bus
 * 
 * @param   bus      Bus information
 * @param   message  The message to write, may not be longer than
 *                   `BUS_MEMORY_SIZE` including the NUL-termination
 * @param   flags    `BUS_NOWAIT` if this function shall fail if
 *                   another process is currently running this
 *                   procedure
 * @return           0 on success, -1 on error
 */
int bus_write(const bus_t *bus, const char *message, int flags);
/* TODO bus_write_timed */

/**
 * Listen (in a loop, forever) for new message on a bus
 * 
 * @param   bus       Bus information
 * @param   callback  Function to call when a message is received, the
 *                    input parameters will be the read message and
 *                    `user_data` from `bus_read`'s parameter with the
 *                    same name. The message must have been parsed or
 *                    copied when `callback` returns as it may be over
 *                    overridden after that time. `callback` should
 *                    return either of the the values:
 *                      *  0:  stop listening
 *                      *  1:  continue listening
 *                      * -1:  an error has occurred
 *		      However, the function [`bus_read`] will invoke
 *                    `callback` with `message` set to `NULL`one time
 *                    directly after it has started listening on the
 *                    bus. This is to the the program now it can safely
 *                    continue with any action that requires that the
 *                    programs is listening on the bus.
 * @return            0 on success, -1 on error
 */
int bus_read(const bus_t *bus, int (*callback)(const char *message, void *user_data), void *user_data);
/* TODO bus_read_timed */


/**
 * Announce that the thread is listening on the bus.
 * This is required so the will does not miss any
 * messages due to race conditions. Additionally,
 * not calling this function will cause the bus the
 * misbehave, is `bus_poll` is written to expect
 * this function to have been called.
 * 
 * @param   bus  Bus information
 * @return       0 on success, -1 on error
 */
int bus_poll_start(bus_t *bus);

/**
 * Announce that the thread has stopped listening on the bus.
 * This is required so that the thread does not cause others
 * to wait indefinitely.
 * 
 * @param   bus  Bus information
 * @return       0 on success, -1 on error
 */
int bus_poll_stop(const bus_t *bus);

/**
 * Wait for a message to be broadcasted on the bus.
 * The caller should make a copy of the received message,
 * without freeing the original copy, and parse it in a
 * separate thread. When the new thread has started be
 * started, the caller of this function should then
 * either call `bus_poll` again or `bus_poll_stop`.
 * 
 * @param   bus    Bus information
 * @param   flags  `IPC_NOWAIT` if the bus should fail and set `errno` to
 *                 `EAGAIN` if this isn't already a message available on the bus
 * @return         The received message, `NULL` on error
 */
const char *bus_poll(bus_t *bus, int flags);
/* TODO bus_poll_timed */


/**
 * Change the ownership of a bus
 * 
 * `stat(2)` can be used of the bus's associated file to get the bus's ownership
 * 
 * @param   file   The pathname of the bus
 * @param   owner  The user ID of the bus's new owner
 * @param   group  The group ID of the bus's new group
 * @return         0 on success, -1 on error
 */
int bus_chown(const char *file, uid_t owner, gid_t group);

/**
 * Change the permissions for a bus
 * 
 * `stat(2)` can be used of the bus's associated file to get the bus's permissions
 * 
 * @param   file  The pathname of the bus
 * @param   mode  The permissions of the bus, any permission for a user implies
 *                full permissions for that user, except only the owner may
 *                edit the bus's associated file
 * @return        0 on success, -1 on error
 */
int bus_chmod(const char *file, mode_t mode);



#endif

