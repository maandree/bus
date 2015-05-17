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
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include "bus.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>



#ifdef BUS_SEMAPHORES_ARE_SYNCHRONOUS_ME_EVEN_HARDER
# ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS_ME_HARDER
#  define BUS_SEMAPHORES_ARE_SYNCHRONOUS_ME_HARDER
# endif
#endif
#ifdef BUS_SEMAPHORES_ARE_SYNCHRONOUS_ME_HARDER
# ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS
#  define BUS_SEMAPHORES_ARE_SYNCHRONOUS
# endif
#endif



/**
 * Semaphore used to signal `bus_write` that `bus_read` is ready
 */
#define S  0

/**
 * Semaphore for making `bus_write` wait while `bus_read` is reseting `S`
 */
#define W  1

/**
 * Binary semaphore for making `bus_write` exclusively locked
 */
#define X  2

/**
 * Semaphore used to cue `bus_read` that it may read the shared memory
 */
#define Q  3

#ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS_ME_EVEN_HARDER
/**
 * Semaphore used to notify `bus_read` that it may restore `S`
 */
#define N  4

/**
 * The number of semaphores in the semaphore array
 */
#define BUS_SEMAPHORES  5
#else
#define BUS_SEMAPHORES  4
#endif

/**
 * The default permission mits of the bus
 */
#define DEFAULT_MODE  0600



/**
 * Decrease the value of a semaphore by 1
 * 
 * @param   bus:const bus_t *  The bus
 * @param   semaphore:int      The index of the semaphore, `S`, `W`, `X` or `Q`
 * @param   flags:int          `SEM_UNDO` if the action should be undone when the program exits,
 *                             `IPC_NOWAIT` if the action should fail if it would block
 * @return  :int               0 on success, -1 on error
 */
#define acquire_semaphore(bus, semaphore, flags) \
	semaphore_op(bus, semaphore, -1, flags)

/**
 * Increase the value of a semaphore by 1
 * 
 * @param   bus:const bus_t *  The bus
 * @param   semaphore:int      The index of the semaphore, `S`, `W`, `X` or `Q`
 * @param   flags:int          `SEM_UNDO` if the action should be undone when the program exits
 * @return  :int               0 on success, -1 on error
 */
#define release_semaphore(bus, semaphore, flags) \
	semaphore_op(bus, semaphore, +1, flags)

/**
 * Wait for the value of a semaphore to become 0
 * 
 * @param   bus:const bus_t *  The bus
 * @param   semaphore:int      The index of the semaphore, `S`, `W`, `X` or `Q`
 * @param   flags:int          `IPC_NOWAIT` if the action should fail if it would block
 * @return  :int               0 on success, -1 on error
 */
#define zero_semaphore(bus, semaphore, flags) \
	semaphore_op(bus, semaphore, 0, flags)

/**
 * Decrease the value of a semaphore by 1
 * 
 * @param   bus:const bus_t *                The bus
 * @param   semaphore:int                    The index of the semaphore, `S`, `W`, `X` or `Q`
 * @param   flags:int                        `SEM_UNDO` if the action should be undone when the program exits,
 *                                           `IPC_NOWAIT` if the action should fail if it would block
 * @param   timeout:const struct timespec *  The amount of time to wait before failing
 * @return  :int                             0 on success, -1 on error
 */
#define acquire_semaphore_timed(bus, semaphore, flags, timeout) \
	semaphore_op_timed(bus, semaphore, -1, flags, timeout)

/**
 * Increase the value of a semaphore by 1
 * 
 * @param   bus:const bus_t *                The bus
 * @param   semaphore:int                    The index of the semaphore, `S`, `W`, `X` or `Q`
 * @param   flags:int                        `SEM_UNDO` if the action should be undone when the program exits
 * @param   timeout:const struct timespec *  The amount of time to wait before failing
 * @return  :int                             0 on success, -1 on error
 */
#define release_semaphore_timed(bus, semaphore, flags, timeout) \
	semaphore_op_timed(bus, semaphore, +1, flags, timeout)

/**
 * Wait for the value of a semaphore to become 0
 * 
 * @param   bus:const bus_t *                The bus
 * @param   semaphore:int                    The index of the semaphore, `S`, `W`, `X` or `Q`
 * @param   flags:int                        `IPC_NOWAIT` if the action should fail if it would block
 * @param   timeout:const struct timespec *  The amount of time to wait before failing
 * @return  :int                             0 on success, -1 on error
 */
#define zero_semaphore_timed(bus, semaphore, flags, timeout) \
	semaphore_op_timed(bus, semaphore, 0, flags, timeout)

/**
 * Open the semaphore array
 * 
 * @param   bus:const bus_t *  The bus
 * @return  :int               0 on success, -1 on error
 */
#define open_semaphores(bus) \
	(((bus)->sem_id = semget((bus)->key_sem, BUS_SEMAPHORES, 0)) == -1 ? -1 : 0)

/**
 * Write a message to the shared memory
 * 
 * @param   bus:const bus_t *  The bus
 * @param   msg:const char *   The message
 * @return  :int               0 on success, -1 on error
 */
#define write_shared_memory(bus, msg) \
	(memcpy((bus)->message, msg, (strlen(msg) + 1) * sizeof(char)))


/**
 * Set `delta` to the convertion of `timeout` from absolute to relative time,
 * measured in the clock whose ID is specified by `clockid`
 * 
 * @scope  timeout:struct timespec          Output variable for relative time
 * @scope  timeout:const struct timespec *  The absolute time
 * @scope  clockid:clockid_t                The clock time is measured
 */
#define DELTA \
	do { \
		if (absolute_time_to_delta_time(&delta, timeout, clockid) < 0)  goto fail; \
		else if ((delta.tv_sec < 0) || (delta.tv_nsec < 0))  { errno = EAGAIN; goto fail; } \
	} while (0)


/**
 * If `flags & (bus_flag)`, this macro evalutes to `sys_flag`,
 * otherwise this macro evalutes to 0.
 */
#define F(bus_flag, sys_flag)  \
	((flags & (bus_flag)) ? sys_flag : 0)



/**
 * Statement wrapper that goes to `fail` on failure
 */
#define t(inst) \
	if ((inst) == -1)  goto fail



#ifdef _SEM_SEMUN_UNDEFINED
union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};
#endif



/**
 * Create a semaphore array for the bus
 * 
 * @param   bus  Bus information to fill with the key of the created semaphore array
 * @return       0 on success, -1 on error
 */
static int
create_semaphores(bus_t *bus)
{
	int id = -1, rint, saved_errno;
	double r;
	union semun values;

	values.array = NULL;

	/* Create semaphore array. */
	for (;;) {
		rint = rand();
		r = (double)rint;
		r /= (double)RAND_MAX + 1;
		r *= (1 << (8 * sizeof(key_t) - 2)) - 1;
		bus->key_sem = (key_t)r + 1;
		if (bus->key_sem == IPC_PRIVATE)
			continue;
		id = semget(bus->key_sem, BUS_SEMAPHORES, IPC_CREAT | IPC_EXCL | DEFAULT_MODE);
		if (id != -1)
			break;
		if ((errno != EEXIST) && (errno != EINTR))
			goto fail;
	}

	/* Initialise the array. */
	values.array = calloc((size_t)BUS_SEMAPHORES, sizeof(unsigned short));
	values.array[X] = 1;
	if (!values.array)
		goto fail;
	if (semctl(id, 0, SETALL, values.array) == -1)
		goto fail;
	free(values.array);
	values.array = NULL;

	return 0;

fail:
	saved_errno = errno;
	if (id != -1)
		semctl(id, 0, IPC_RMID);
	free(values.array);
	errno = saved_errno;
	return -1;
}


/**
 * Create a shared memory for the bus
 * 
 * @param   bus  Bus information to fill with the key of the created shared memory
 * @return       0 on success, -1 on error
 */
static int
create_shared_memory(bus_t *bus)
{
	int id = -1, rint, saved_errno;
	double r;
	struct shmid_ds _info;

	/* Create shared memory. */
	for (;;) {
		rint = rand();
		r = (double)rint;
		r /= (double)RAND_MAX + 1;
		r *= (1 << (8 * sizeof(key_t) - 2)) - 1;
		bus->key_shm = (key_t)r + 1;
		if (bus->key_shm == IPC_PRIVATE)
			continue;
		id = shmget(bus->key_shm, (size_t)BUS_MEMORY_SIZE, IPC_CREAT | IPC_EXCL | DEFAULT_MODE);
		if (id != -1)
			break;
		if ((errno != EEXIST) && (errno != EINTR))
			goto fail;
	}

	return 0;

fail:
	saved_errno = errno;
	if (id != -1)
		shmctl(id, IPC_RMID, &_info);
	errno = saved_errno;
	return -1;
}


/**
 * Remove the semaphore array for the bus
 * 
 * @param   bus  Bus information
 * @return       0 on success, -1 on error
 */
static int
remove_semaphores(const bus_t *bus)
{
	int id = semget(bus->key_sem, BUS_SEMAPHORES, 0);
	return ((id == -1) || (semctl(id, 0, IPC_RMID) == -1)) ? -1 : 0;
}


/**
 * Remove the shared memory for the bus
 * 
 * @param   bus  Bus information
 * @return       0 on success, -1 on error
 */
static int
remove_shared_memory(const bus_t *bus)
{
	struct shmid_ds _info;
	int id = shmget(bus->key_shm, (size_t)BUS_MEMORY_SIZE, 0);
	return ((id == -1) || (shmctl(id, IPC_RMID, &_info) == -1)) ? -1 : 0;
}


/**
 * Increase or decrease the value of a semaphore, or wait the it to become 0
 * 
 * @param   bus        Bus information
 * @param   semaphore  The index of the semaphore, `S`, `W`, `X` or `Q`
 * @param   delta      The adjustment to make to the semaphore's value, 0 to wait for it to become 0
 * @param   flags      `SEM_UNDO` if the action should be undone when the program exits
 * @return             0 on success, -1 on error
 */
static int
semaphore_op(const bus_t *bus, int semaphore, int delta, int flags)
{
	struct sembuf op;
	op.sem_num = (unsigned short)semaphore;
	op.sem_op = (short)delta;
	op.sem_flg = (short)flags;
	return semop(bus->sem_id, &op, (size_t)1);
}


/**
 * Increase or decrease the value of a semaphore, or wait the it to become 0
 * 
 * @param   bus        Bus information
 * @param   semaphore  The index of the semaphore, `S`, `W`, `X` or `Q`
 * @param   delta      The adjustment to make to the semaphore's value, 0 to wait for it to become 0
 * @param   flags      `SEM_UNDO` if the action should be undone when the program exits
 * @param   timeout    The amount of time to wait before failing
 * @return             0 on success, -1 on error
 */
static int
semaphore_op_timed(const bus_t *bus, int semaphore, int delta, int flags, const struct timespec *timeout)
{
	struct sembuf op;
	op.sem_num = (unsigned short)semaphore;
	op.sem_op = (short)delta;
	op.sem_flg = (short)flags;
	return semtimedop(bus->sem_id, &op, (size_t)1, timeout);
}


/**
 * Set the value of a semaphore
 * 
 * @param   bus        Bus information
 * @param   semaphore  The index of the semaphore, `S`, `W`, `X` or `Q`
 * @param   value      The new value of the semaphore
 * @return             0 on success, -1 on error
 */
static int
write_semaphore(const bus_t *bus, unsigned semaphore, int value)
{
	union semun semval;
	semval.val = value;
	return semctl(bus->sem_id, (unsigned short)semaphore, SETVAL, semval);
}


/**
 * Open the shared memory for the bus
 * 
 * @param   bus    Bus information
 * @param   flags  `BUS_RDONLY`, `BUS_WRONLY` or `BUS_RDWR`
 * @return         0 on success, -1 on error
 */
static int
open_shared_memory(bus_t *bus, int flags)
{
	int id;
	void *address;
	t(id = shmget(bus->key_shm, (size_t)BUS_MEMORY_SIZE, 0));
	address = shmat(id, NULL, (flags & BUS_RDONLY) ? SHM_RDONLY : 0);
	if ((address == (void *)-1) || !address)
		goto fail;
	bus->message = (char *)address;
	return 0;
fail:
	return -1;
}


/**
 * Close the shared memory for the bus
 * 
 * @param   bus  Bus information
 * @return       0 on success, -1 on error
 */
static int
close_shared_memory(bus_t *bus)
{
	t(shmdt(bus->message));
	bus->message = NULL;
	return 0;
fail:
	return -1;
}


/**
 * Get a random ASCII letter or digit
 * 
 * @return  A random ASCII letter or digit
 */
static char
randomchar(void)
{
	int rint = rand();
	double r = (double)rint;
	r /= (double)RAND_MAX + 1;
	r *= 10 + 26 + 26;
	return "0123456789qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM"[(int)r];
}


/**
 * Basically, this is `mkdir -p -m $mode $pathname`
 * 
 * @param   pathname  The pathname of the directory to create if missing
 * @param   mode      The permission bits of any created directory
 * @return            0 on sucess, -1 on error
 */
static int
mkdirs(char *pathname, mode_t mode)
{
	size_t i, n = strlen(pathname);
	char c;
	for (i = 0; i < n; i++)
		if (pathname[i] != '/')
			break;
	for (; i < n; i++) {
		if (pathname[i] == '/') {
			c = pathname[i];
			if (access(pathname, F_OK))
				if (mkdir(pathname, mode) < 0)
					return -1;
			pathname[i] = c;
			break;
		}
	}
	if (access(pathname, F_OK))
		if (mkdir(pathname, mode) < 0)
			return -1;
	return 0;
}


/**
 * Convert an absolute time to a relative time
 * 
 * @param   delta     Output parameter for the relative time
 * @param   absolute  The absolute time
 * @param   clockid   The ID of the clock the time is measured in
 * @return            0 on success, -1 on error
 */
static int
absolute_time_to_delta_time(struct timespec *delta, const struct timespec *absolute, clockid_t clockid)
{
	if (clock_gettime(clockid, delta) < 0)
		return -1;

	delta->tv_sec  = absolute->tv_sec  - delta->tv_sec;
	delta->tv_nsec = absolute->tv_nsec - delta->tv_nsec;

	if (delta->tv_nsec < 0L) {
		delta->tv_nsec += 1000000000L;
		delta->tv_sec -= 1;
	}
	if (delta->tv_nsec >= 1000000000L) {
		delta->tv_nsec -= 1000000000L;
		delta->tv_sec += 1;
	}

	return 0;
}



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
int
bus_create(const char *file, int flags, char **out_file)
{
	int fd = -1, saved_errno;
	bus_t bus;
	char buf[1 + 2 * (3 * sizeof(ssize_t) + 2)];
	size_t ptr, len;
	ssize_t wrote;
	char *genfile = NULL;
	const char *env;

	if (out_file)
		*out_file = NULL;

	bus.sem_id = -1;
	bus.key_sem = -1;
	bus.key_shm = -1;
	bus.message = NULL;
	bus.first_poll = 0;

	srand((unsigned int)time(NULL) + (unsigned int)rand());

	if (file) {
		fd = open(file, O_WRONLY | O_CREAT | O_EXCL, DEFAULT_MODE);
		if (fd == -1) {
			if ((errno != EEXIST) || (flags & BUS_EXCL))
				return -1;
			goto done;
		}
	} else {
		env = getenv("XDG_RUNTIME_DIR");
		if (!env || !*env)
			env = "/run";
		genfile = malloc((strlen(env) + 6 + 7 + 30) * sizeof(char));
		if (!genfile)
			goto fail;
		if (out_file)
			*out_file = genfile;
		sprintf(genfile, "%s/bus", env);
		t(mkdirs(genfile, 0755));
		sprintf(genfile, "%s/bus/random.", env);
		len = strlen(genfile);
		genfile[len + 30] = '\0';
	retry:
		for (ptr = 0; ptr < 30; ptr++)
			genfile[len + ptr] = randomchar();
		fd = open(genfile, O_WRONLY | O_CREAT | O_EXCL, DEFAULT_MODE);
		if (fd == -1) {
			if (errno == EEXIST)
				goto retry;
			return -1;
		}
	}

	t(create_semaphores(&bus));
	t(create_shared_memory(&bus));

	sprintf(buf, "%zi\n%zi\n", (ssize_t)(bus.key_sem), (ssize_t)(bus.key_shm));
	for (len = strlen(buf), ptr = 0; ptr < len;) {
		wrote = write(fd, buf + ptr, len - ptr);
		if (wrote < 0) {
			if ((errno != EINTR) || (flags & BUS_INTR))
				goto fail;
		} else {
			ptr += (size_t)wrote;
		}
	}
	close(fd);

done:
	if (out_file && !*out_file) {
		len = strlen(file) + 1;
		*out_file = malloc(len * sizeof(char));
		memcpy(*out_file, file, len * sizeof(char));
	} else if (!out_file) {
		free(genfile);
	}
	return 0;

fail:
	saved_errno = errno;
	if (bus.key_sem)
		remove_semaphores(&bus);
	if (bus.key_shm)
		remove_shared_memory(&bus);
	if (fd == -1)
		close(fd);
	if (out_file)
		*out_file = NULL;
	free(genfile);
	unlink(file);
	errno = saved_errno;
	return -1;
}


/**
 * Remove a bus
 * 
 * @param   file  The pathname of the bus
 * @return        0 on success, -1 on error
 */
int
bus_unlink(const char *file)
{
	int r = 0, saved_errno = 0;
	bus_t bus;
	t(bus_open(&bus, file, -1));

	r |= remove_semaphores(&bus);
	if (r && !saved_errno)
		saved_errno = errno;

	r |= remove_shared_memory(&bus);
	if (r && !saved_errno)
		saved_errno = errno;

	r |= unlink(file);
	if (r && !saved_errno)
		saved_errno = errno;

	errno = saved_errno;
	return r;
fail:
	return -1;
}


/**
 * Open an existing bus
 * 
 * @param   bus    Bus information to fill
 * @param   file   The filename of the bus
 * @param   flags  `BUS_RDONLY`, `BUS_WRONLY` or `BUS_RDWR`
 *                 any negative value is used internally
 *                 for telling the function to not actually
 *                 opening the bus, but just to parse the file
 * @return         0 on success, -1 on error
 */
int
bus_open(bus_t *bus, const char *file, int flags)
{
	int saved_errno;
	char *line = NULL;
	size_t len = 0;
	FILE *f;

	bus->sem_id = -1;
	bus->key_sem = -1;
	bus->key_shm = -1;
	bus->message = NULL;

	f = fopen(file, "r");

	t(getline(&line, &len, f));
	t(bus->key_sem = (key_t)atoll(line));
	free(line), line = NULL, len = 0;

	t(getline(&line, &len, f));
	t(bus->key_shm = (key_t)atoll(line));
	free(line), line = NULL;

	fclose(f);

	if (flags >= 0) {
		t(open_semaphores(bus));
		t(open_shared_memory(bus, flags));
	}
	return 0;
fail:
	saved_errno = errno;
	free(line);
	errno = saved_errno;
	return -1;
}


/**
 * Close a bus
 * 
 * @param   bus  Bus information
 * @return       0 on success, -1 on error
 */
int
bus_close(bus_t *bus)
{
	bus->sem_id = -1;
	if (bus->message)
		t(close_shared_memory(bus));
	bus->message = NULL;
	return 0;

fail:
	return -1;
}


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
int
bus_write(const bus_t *bus, const char *message, int flags)
{
	int saved_errno;
#ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS
	int state = 0;
#endif
	if (acquire_semaphore(bus, X, SEM_UNDO | F(BUS_NOWAIT, IPC_NOWAIT)) == -1)
		return -1;
	t(zero_semaphore(bus, W, 0));
	write_shared_memory(bus, message);
#ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS
	t(release_semaphore(bus, N, SEM_UNDO));  state++;
#endif
	t(write_semaphore(bus, Q, 0));
	t(zero_semaphore(bus, S, 0));
#ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS
	t(acquire_semaphore(bus, N, SEM_UNDO));  state--;
#endif
	t(release_semaphore(bus, X, SEM_UNDO));
	return 0;

fail:
	saved_errno = errno;
#ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS
	if (state > 0)
		acquire_semaphore(bus, N, SEM_UNDO);
#endif
	release_semaphore(bus, X, SEM_UNDO);
	errno = saved_errno;
	return -1;
}


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
int bus_write_timed(const bus_t *bus, const char *message,
		    const struct timespec *timeout, clockid_t clockid)
{
	int saved_errno;
#ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS
	int state = 0;
#endif
	struct timespec delta;
	if (!timeout)
		return bus_write(bus, message, 0);

	DELTA;
	if (acquire_semaphore_timed(bus, X, SEM_UNDO, &delta) == -1)
		return -1;
	DELTA;
	t(zero_semaphore_timed(bus, W, 0, &delta));
	write_shared_memory(bus, message);
#ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS
	t(release_semaphore(bus, N, SEM_UNDO));  state++;
#endif
	t(write_semaphore(bus, Q, 0));
	t(zero_semaphore(bus, S, 0));
#ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS
	t(acquire_semaphore(bus, N, SEM_UNDO));  state--;
#endif
	t(release_semaphore(bus, X, SEM_UNDO));
	return 0;

fail:
	saved_errno = errno;
#ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS
	if (state > 0)
		acquire_semaphore(bus, N, SEM_UNDO);
#endif
	release_semaphore(bus, X, SEM_UNDO);
	errno = saved_errno;
	return -1;

}


/**
 * Listen (in a loop, forever) for new message on a bus
 * 
 * @param   bus        Bus information
 * @param   callback   Function to call when a message is received, the
 *                     input parameters will be the read message and
 *                     `user_data` from `bus_read`'s parameter with the
 *                     same name. The message must have been parsed or
 *                     copied when `callback` returns as it may be over
 *                     overridden after that time. `callback` should
 *                     return either of the the values:
 *                       *  0:  stop listening
 *                       *  1:  continue listening
 *                       * -1:  an error has occurred
 *                     However, the function [`bus_read`] will invoke
 *                     `callback` with `message` set to `NULL`one time
 *                     directly after it has started listening on the
 *                     bus. This is to the the program now it can safely
 *                     continue with any action that requires that the
 *                     programs is listening on the bus.
 * @param   user_data  Parameter passed to `callback`
 * @return             0 on success, -1 on error
 */
int
bus_read(const bus_t *bus, int (*callback)(const char *message, void *user_data), void *user_data)
{
	int r, state = 0, saved_errno;
	if (release_semaphore(bus, S, SEM_UNDO) == -1)
		return -1;
	t(r = callback(NULL, user_data));
	if (!r)  goto done;
	for (;;) {
		t(release_semaphore(bus, Q, 0));
		t(zero_semaphore(bus, Q, 0));
		t(r = callback(bus->message, user_data));
		if (!r)  goto done;
		t(release_semaphore(bus, W, SEM_UNDO));  state++;
		t(acquire_semaphore(bus, S, SEM_UNDO));  state++;
		t(zero_semaphore(bus, S, 0));
#ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS_ME_HARDER
		t(zero_semaphore(bus, N, 0));
#endif
		t(release_semaphore(bus, S, SEM_UNDO));  state--;
		t(acquire_semaphore(bus, W, SEM_UNDO));  state--;
	}

fail:
	saved_errno = errno;
	if (state > 1)
		release_semaphore(bus, S, SEM_UNDO);
	if (state > 0)
		acquire_semaphore(bus, W, SEM_UNDO);
	acquire_semaphore(bus, S, SEM_UNDO);
	errno = saved_errno;
	return -1;

done:
	t(acquire_semaphore(bus, S, SEM_UNDO));
	return 0;
}


/**
 * Listen (in a loop, forever) for new message on a bus
 * 
 * @param   bus        Bus information
 * @param   callback   Function to call when a message is received, the
 *                     input parameters will be the read message and
 *                     `user_data` from `bus_read`'s parameter with the
 *                     same name. The message must have been parsed or
 *                     copied when `callback` returns as it may be over
 *                     overridden after that time. `callback` should
 *                     return either of the the values:
 *                       *  0:  stop listening
 *                       *  1:  continue listening
 *                       * -1:  an error has occurred
 *                     However, the function [`bus_read`] will invoke
 *                     `callback` with `message` set to `NULL`one time
 *                     directly after it has started listening on the
 *                     bus. This is to the the program now it can safely
 *                     continue with any action that requires that the
 *                     programs is listening on the bus.
 * @param   user_data  Parameter passed to `callback`
 * @param   timeout    The time the operation shall fail with errno set
 *                     to `EAGAIN` if not completed, note that the callback
 *                     function may or may not have been called
 * @param   clockid    The ID of the clock the `timeout` is measured with,
 *                     it most be a predictable clock
 * @return             0 on success, -1 on error
 */
int bus_read_timed(const bus_t *bus, int (*callback)(const char *message, void *user_data),
		   void *user_data, const struct timespec *timeout, clockid_t clockid)
{
	int r, state = 0, saved_errno;
	struct timespec delta;
	if (!timeout)
		return bus_read(bus, callback, user_data);

	DELTA;
	if (release_semaphore_timed(bus, S, SEM_UNDO, &delta) == -1)
		return -1;
	t(r = callback(NULL, user_data));
	if (!r)  goto done;
	for (;;) {
		DELTA;
		t(release_semaphore_timed(bus, Q, 0, &delta));
		DELTA;
		t(zero_semaphore_timed(bus, Q, 0, &delta));
		t(r = callback(bus->message, user_data));
		if (!r)  goto done;
		t(release_semaphore(bus, W, SEM_UNDO));  state++;
		t(acquire_semaphore(bus, S, SEM_UNDO));  state++;
		t(zero_semaphore(bus, S, 0));
#ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS_ME_HARDER
		t(zero_semaphore(bus, N, 0));
#endif
		t(release_semaphore(bus, S, SEM_UNDO));  state--;
		t(acquire_semaphore(bus, W, SEM_UNDO));  state--;
	}

fail:
	saved_errno = errno;
	if (state > 1)
		release_semaphore(bus, S, SEM_UNDO);
	if (state > 0)
		acquire_semaphore(bus, W, SEM_UNDO);
	acquire_semaphore(bus, S, SEM_UNDO);
	errno = saved_errno;
	return -1;

done:
	t(acquire_semaphore(bus, S, SEM_UNDO));
	return 0;
}


/**
 * Announce that the thread is listening on the bus.
 * This is required so the will does not miss any
 * messages due to race conditions. Additionally,
 * not calling this function will cause the bus the
 * misbehave, is `bus_poll` is written to expect
 * this function to have been called.
 * 
 * @param   bus    Bus information
 * @param   flags  `BUS_NOWAIT` if the bus should fail and set `errno` to
 *                 `EAGAIN` if there isn't already a message available on
 *                 the bus when `bus_poll` is called
 * @return         0 on success, -1 on error
 */
int
bus_poll_start(bus_t *bus, int flags)
{
	bus->first_poll = 1;
	bus->flags = flags;
	t(release_semaphore(bus, S, SEM_UNDO));
	if (flags & BUS_NOWAIT) {
		t(release_semaphore(bus, Q, 0));
	}
	return 0;

fail:
	return -1;
}


/**
 * Announce that the thread has stopped listening on the bus.
 * This is required so that the thread does not cause others
 * to wait indefinitely.
 * 
 * @param   bus  Bus information
 * @return       0 on success, -1 on error
 */
int
bus_poll_stop(const bus_t *bus)
{
	return acquire_semaphore(bus, S, SEM_UNDO | IPC_NOWAIT);
}


/**
 * Wait for a message to be broadcasted on the bus.
 * The caller should make a copy of the received message,
 * without freeing the original copy, and parse it in a
 * separate thread. When the new thread has started be
 * started, the caller of this function should then
 * either call `bus_poll` again or `bus_poll_stop`.
 * 
 * @param   bus  Bus information
 * @return       The received message, `NULL` on error
 */
const char *
bus_poll(bus_t *bus)
{
	int state = 0, saved_errno;
	if (!bus->first_poll) {
		t(release_semaphore(bus, W, SEM_UNDO));  state++;
		t(acquire_semaphore(bus, S, SEM_UNDO));  state++;
		t(zero_semaphore(bus, S, 0));
#ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS_ME_HARDER
		t(zero_semaphore(bus, N, 0));
#endif
		t(release_semaphore(bus, S, SEM_UNDO));  state--;
		t(acquire_semaphore(bus, W, SEM_UNDO));  state--;
		if (bus->flags & BUS_NOWAIT) {
			t(release_semaphore(bus, Q, 0));
		}
	} else {
		bus->first_poll = 0;
	}
	state--;
	if (bus->flags & BUS_NOWAIT) {
		t(zero_semaphore(bus, Q, IPC_NOWAIT));
	} else {
		t(release_semaphore(bus, Q, 0));
		t(zero_semaphore(bus, Q, 0));
	}
	return bus->message;

fail:
	saved_errno = errno;
	if (state > 1)
		release_semaphore(bus, S, SEM_UNDO);
	if (state > 0)
		acquire_semaphore(bus, W, SEM_UNDO);
	if (state < 0)
		bus->first_poll = 1;
	errno = saved_errno;
	return NULL;
}


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
const char *bus_poll_timed(bus_t *bus, const struct timespec *timeout, clockid_t clockid)
{
	int state = 0, saved_errno;
	struct timespec delta;
	if (!timeout)
		return bus_poll(bus);

	if (!bus->first_poll) {
		t(release_semaphore(bus, W, SEM_UNDO));  state++;
		t(acquire_semaphore(bus, S, SEM_UNDO));  state++;
		t(zero_semaphore(bus, S, 0));
#ifndef BUS_SEMAPHORES_ARE_SYNCHRONOUS_ME_HARDER
		t(zero_semaphore(bus, N, 0));
#endif
		t(release_semaphore(bus, S, SEM_UNDO));  state--;
		t(acquire_semaphore(bus, W, SEM_UNDO));  state--;
	} else {
		bus->first_poll = 0;
	}
	state--;
	DELTA;
	t(release_semaphore_timed(bus, Q, 0, &delta));
	DELTA;
	t(zero_semaphore_timed(bus, Q, 0, &delta));
	return bus->message;

fail:
	saved_errno = errno;
	if (state > 1)
		release_semaphore(bus, S, SEM_UNDO);
	if (state > 0)
		acquire_semaphore(bus, W, SEM_UNDO);
	if (state < 0)
		bus->first_poll = 1;
	errno = saved_errno;
	return NULL;
}


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
int
bus_chown(const char *file, uid_t owner, gid_t group)
{
	bus_t bus;
	struct semid_ds sem_stat;
	struct shmid_ds shm_stat;
	int shm_id;

	t(bus_open(&bus, file, -1));
	t(chown(file, owner, group));

	/* chown sem */
	t(open_semaphores(&bus));
	t(semctl(bus.sem_id, 0, IPC_STAT, &sem_stat));
	sem_stat.sem_perm.uid = owner;
	sem_stat.sem_perm.gid = group;
	t(semctl(bus.sem_id, 0, IPC_SET, &sem_stat));

	/* chown shm */
	t(shm_id = shmget(bus.key_shm, (size_t)BUS_MEMORY_SIZE, 0));
	t(shmctl(shm_id, IPC_STAT, &shm_stat));
	shm_stat.shm_perm.uid = owner;
	shm_stat.shm_perm.gid = group;
	t(shmctl(shm_id, IPC_SET, &shm_stat));

	return 0;
fail:
	return -1;
}


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
int
bus_chmod(const char *file, mode_t mode)
{
	bus_t bus;
	mode_t fmode;
	struct semid_ds sem_stat;
	struct shmid_ds shm_stat;
	int shm_id;

	mode = (mode & S_IRWXU) ? (mode | S_IRWXU) : (mode & ~S_IRWXU);
	mode = (mode & S_IRWXG) ? (mode | S_IRWXG) : (mode & ~S_IRWXG);
	mode = (mode & S_IRWXO) ? (mode | S_IRWXO) : (mode & ~S_IRWXO);
	mode &= (S_IWUSR | S_IWGRP | S_IWOTH | S_IRUSR | S_IRGRP | S_IROTH);
	fmode = mode & ~(S_IWGRP | S_IWOTH);

	t(bus_open(&bus, file, -1));
	t(chmod(file, fmode));

	/* chmod sem */
	t(open_semaphores(&bus));
	t(semctl(bus.sem_id, 0, IPC_STAT, &sem_stat));
	sem_stat.sem_perm.mode = mode;
	t(semctl(bus.sem_id, 0, IPC_SET, &sem_stat));

	/* chmod shm */
	t(shm_id = shmget(bus.key_shm, (size_t)BUS_MEMORY_SIZE, 0));
	t(shmctl(shm_id, IPC_STAT, &shm_stat));
	shm_stat.shm_perm.mode = mode;
	t(shmctl(shm_id, IPC_SET, &shm_stat));

	return 0;
fail:
	return -1;
}

