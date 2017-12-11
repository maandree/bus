/* See LICENSE file for copyright and license details. */
#ifndef BUS_H
#define BUS_H


#ifndef _DEFAULT_SOURCE
# define _DEFAULT_SOURCE
#endif
#include <sys/types.h>
#include <time.h>



#if defined(__GNUC__)
# define BUS_COMPILER_GCC(X)  X
#else
# define BUS_COMPILER_GCC(X)  /* ignore */
#endif



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
	 * The key for the shared memory
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
BUS_COMPILER_GCC(__attribute__((__warn_unused_result__)))
int bus_create(const char *restrict, int, char **restrict);

/**
 * Remove a bus
 * 
 * @param   file  The pathname of the bus
 * @return        0 on success, -1 on error
 */
BUS_COMPILER_GCC(__attribute__((__nonnull__)))
int bus_unlink(const char *);


/**
 * Open an existing bus
 * 
 * @param   bus    Bus information to fill
 * @param   file   The filename of the bus
 * @param   flags  `BUS_RDONLY`, `BUS_WRONLY` or `BUS_RDWR`,
 *                 the value must not be negative
 * @return         0 on success, -1 on error
 */
BUS_COMPILER_GCC(__attribute__((__nonnull__, __warn_unused_result__)))
int bus_open(bus_t *restrict, const char *restrict, int);

/**
 * Close a bus
 * 
 * @param   bus  Bus information
 * @return       0 on success, -1 on error
 */
BUS_COMPILER_GCC(__attribute__((__nonnull__)))
int bus_close(bus_t *);


/**
 * Broadcast a message on a bus
 * 
 * @param   bus      Bus information
 * @param   message  The message to write, may not be longer than
 *                   `BUS_MEMORY_SIZE` including the NUL-termination
 * @param   flags    `BUS_NOWAIT` if this function shall fail if
 *                   another process is currently running this
 *                   procedure
 * @return           0 on success, -1 on error
 */
BUS_COMPILER_GCC(__attribute__((__nonnull__, __warn_unused_result__)))
int bus_write(const bus_t *, const char *, int);

/**
 * Broadcast a message on a bus
 * 
 * @param   bus      Bus information
 * @param   message  The message to write, may not be longer than
 *                   `BUS_MEMORY_SIZE` including the NUL-termination
 * @param   timeout  The time the operation shall fail with errno set
 *                   to `EAGAIN` if not completed
 * @param   clockid  The ID of the clock the `timeout` is measured with,
 *                   it most be a predictable clock
 * @return           0 on success, -1 on error
 */
BUS_COMPILER_GCC(__attribute__((__nonnull__(1, 2), __warn_unused_result__)))
int bus_write_timed(const bus_t *, const char *, const struct timespec *, clockid_t);


/**
 * Listen (in a loop, forever) for new message on a bus
 * 
 * @param   bus                     Bus information
 * @param   callback                Function to call when a message is received, the
 *            (message, user_data)  input parameters will be the read message and
 *                                  `user_data` from `bus_read`'s parameter with the
 *                                  same name. The message must have been parsed or
 *                                  copied when `callback` returns as it may be over
 *                                  overridden after that time. `callback` should
 *                                  return either of the the values:
 *                                    *  0:  stop listening
 *                                    *  1:  continue listening
 *                                    * -1:  an error has occurred
 *                                  However, the function [`bus_read`] will invoke
 *                                  `callback` with `message` set to `NULL`one time
 *                                  directly after it has started listening on the
 *                                  bus. This is to the the program now it can safely
 *                                  continue with any action that requires that the
 *                                  programs is listening on the bus.
 * @param   user_data               Parameter passed to `callback`
 * @return                          0 on success, -1 on error
 */
BUS_COMPILER_GCC(__attribute__((__nonnull__(1, 2), __warn_unused_result__)))
int bus_read(const bus_t *restrict, int (*)(const char *, void *), void *);

/**
 * Listen (in a loop, forever) for new message on a bus
 * 
 * @param   bus                     Bus information
 * @param   callback                Function to call when a message is received, the
 *            (message, user_data)  input parameters will be the read message and
 *                                  `user_data` from `bus_read`'s parameter with the
 *                                  same name. The message must have been parsed or
 *                                  copied when `callback` returns as it may be over
 *                                  overridden after that time. `callback` should
 *                                  return either of the the values:
 *                                    *  0:  stop listening
 *                                    *  1:  continue listening
 *                                    * -1:  an error has occurred
 *                                  However, the function [`bus_read`] will invoke
 *                                  `callback` with `message` set to `NULL`one time
 *                                  directly after it has started listening on the
 *                                  bus. This is to the the program now it can safely
 *                                  continue with any action that requires that the
 *                                  programs is listening on the bus.
 * @param   user_data               Parameter passed to `callback`
 * @param   timeout                 The time the operation shall fail with errno set
 *                                  to `EAGAIN` if not completed, note that the callback
 *                                  function may or may not have been called
 * @param   clockid                 The ID of the clock the `timeout` is measured with,
 *                                  it most be a predictable clock
 * @return                          0 on success, -1 on error
 */
BUS_COMPILER_GCC(__attribute__((__nonnull__(1, 2), __warn_unused_result__)))
int bus_read_timed(const bus_t *restrict, int (*)(const char *, void *),
                   void *, const struct timespec *, clockid_t);


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
BUS_COMPILER_GCC(__attribute__((__nonnull__, __warn_unused_result__)))
int bus_poll_start(bus_t *);

/**
 * Announce that the thread has stopped listening on the bus.
 * This is required so that the thread does not cause others
 * to wait indefinitely.
 * 
 * @param   bus  Bus information
 * @return       0 on success, -1 on error
 */
BUS_COMPILER_GCC(__attribute__((__nonnull__, __warn_unused_result__)))
int bus_poll_stop(const bus_t *);

/**
 * Wait for a message to be broadcasted on the bus.
 * The caller should make a copy of the received message,
 * without freeing the original copy, and parse it in a
 * separate thread. When the new thread has started be
 * started, the caller of this function should then
 * either call `bus_poll` again or `bus_poll_stop`.
 * 
 * @param   bus    Bus information
 * @param   flags  `BUS_NOWAIT` if the bus should fail and set `errno` to
 *                 `EAGAIN` if there isn't already a message available on the bus
 * @return         The received message, `NULL` on error
 */
BUS_COMPILER_GCC(__attribute__((__nonnull__, __warn_unused_result__)))
const char *bus_poll(bus_t *, int);

/**
 * Wait for a message to be broadcasted on the bus.
 * The caller should make a copy of the received message,
 * without freeing the original copy, and parse it in a
 * separate thread. When the new thread has started be
 * started, the caller of this function should then
 * either call `bus_poll_timed` again or `bus_poll_stop`.
 * 
 * @param   bus      Bus information
 * @param   timeout  The time the operation shall fail with errno set
 *                   to `EAGAIN` if not completed
 * @param   clockid  The ID of the clock the `timeout` is measured with,
 *                   it most be a predictable clock
 * @return           The received message, `NULL` on error
 */
BUS_COMPILER_GCC(__attribute__((__nonnull__(1), __warn_unused_result__)))
const char *bus_poll_timed(bus_t *, const struct timespec *, clockid_t);


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
BUS_COMPILER_GCC(__attribute__((__nonnull__, __warn_unused_result__)))
int bus_chown(const char *, uid_t, gid_t);

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
BUS_COMPILER_GCC(__attribute__((__nonnull__, __warn_unused_result__)))
int bus_chmod(const char *, mode_t);



#endif

