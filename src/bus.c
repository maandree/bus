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
#include "bus.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>



/**
 * Semaphore used to signal `bus_write` that `bus_read` is ready
 */
#define S 0

/**
 * Semaphore for making `bus_write` wait while `bus_read` is reseting `S`
 */
#define W 1

/**
 * Binary semaphore for making `bus_write` exclusively locked
 */
#define X 2

/**
 * Semaphore used to cue `bus_read` that it may read the shared memory
 */
#define Q 3

/**
 * The number of semaphores in the semaphore array
 */
#define BUS_SEMAPHORES 4



/**
 * Decrease the value of a semaphore by 1
 * 
 * @param   bus:const bus_t *         The bus
 * @param   semaphore:unsigned short  The index of the semaphore, `S`, `W`, `X` or `Q`
 * @param   flags:short               `SEM_UNDO` if the action should be undone when the program exits
 * @return  :int                      0 on success, -1 on error
 */
#define acquire_semaphore(bus, semaphore, flags) \
	semaphore_op(bus, semaphore, -1, flags)

/**
 * Increase the value of a semaphore by 1
 * 
 * @param   bus:const bus_t *         The bus
 * @param   semaphore:unsigned short  The index of the semaphore, `S`, `W`, `X` or `Q`
 * @param   flags:short               `SEM_UNDO` if the action should be undone when the program exits
 * @return  :int                      0 on success, -1 on error
 */
#define release_semaphore(bus, semaphore, flags) \
	semaphore_op(bus, semaphore, +1, flags)

/**
 * Wait for the value of a semphore to become 0
 * 
 * @param   bus:const bus_t *         The bus
 * @param   semaphore:unsigned short  The index of the semaphore, `S`, `W`, `X` or `Q`
 * @return  :int                      0 on success, -1 on error
 */
#define zero_semaphore(bus, semaphore) \
	semaphore_op(bus, semaphore, 0, 0)

/**
 * Open the semaphore array
 * 
 * @param   bus:const bus_t *  The bus
 * @return  :int               0 on success, -1 on error
 */
#define open_semaphores(bus) \
	(((bus)->sem_id = semget((bus)->key_sem, BUS_SEMAPHORES, 0600)) == -1 ? -1 : 0)

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
		id = semget(bus->key_sem, BUS_SEMAPHORES, IPC_CREAT | IPC_EXCL | 0600);
		if (id != -1)
			break;
		if ((errno != EEXIST) && (errno != EINTR))
			goto fail;
	}

	/* Initialise the array. */
	values.array = calloc(BUS_SEMAPHORES, sizeof(unsigned short));
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
	if ((id != -1) && (semctl(id, 0, IPC_RMID) == -1))
		perror(argv0);
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
		id = shmget(bus->key_shm, BUS_MEMORY_SIZE, IPC_CREAT | IPC_EXCL | 0600);
		if (id != -1)
			break;
		if ((errno != EEXIST) && (errno != EINTR))
			goto fail;
	}

	return 0;

fail:
	saved_errno = errno;
	if ((id != -1) && (shmctl(id, IPC_RMID, &_info) == -1))
		perror(argv0);
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
	int id = semget(bus->key_sem, BUS_SEMAPHORES, 0600);
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
	int id = shmget(bus->key_shm, BUS_MEMORY_SIZE, 0600);
	return ((id == -1) || (shmctl(sem_id, IPC_RMID, &_info) == -1)) ? -1 : 0;
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
semaphore_op(const bus_t *bus, unsigned short semaphore, short delta, short flags)
{
	struct sembuf op;
	op.sem_op = delta;
	op.sem_num = semaphore;
	op.sem_flg = flags;
	return semop(bus->sem_id, &op, 1);
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
write_semaphore(const bus_t *bus, unsigned short semaphore, int value)
{
	union semun semval;
	semval.val = value;
	return semctl(bus->sem_id, semaphore, SETVAL, semval);
}


/**
 * Open the shared memory for the bus
 * 
 * @param   bus    Bus information
 * @param   flags  `BUS_RDONLY`, `BUS_WRONLY` or `BUS_RDWR`
 * @return         0 on success, -1 on error
 */
static int
open_shared_memory(const bus_t *bus, int flags)
{
	int id;
	void *address;
	t(id = shmget(bus->key_shm, BUS_MEMORY_SIZE, 0600));
	address = shmat(id, NULL, (flags & BUS_RDONLY) ? SHM_RDONLY : 0);
	if ((address == (void *)-1) || !address)
		goto fail;
	this->message = (char *)address;
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
close_shared_memory(const bus_t *bus)
{
	t(shmdt(this->message));
	this->message = NULL;
	return 0;
fail:
	return -1;
}



const char *
bus_create(const char *file, int flags)
{
	int saved_errno;
	bus_t bus;
	bus.sem_id = -1;
	bus.key_sem = -1;
	bus.key_shm = -1;
	bus.message = NULL;

	srand((unsigned int)time(NULL) + (unsigned int)rand());

	t(create_semaphores(&bus));
	t(create_shared_memory(&bus));
	return NULL; /* TODO */

fail:
	saved_errno = errno;
	if (bus.key_sem)
		remove_semaphores(&bus);
	if (bus.key_shm)
		remove_shared_memory(&bus);
	errno = saved_errno;
	return NULL;
}


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


int
bus_open(bus_t *bus, const char *file, int flags)
{
	bus->sem_id = -1;
	bus->key_sem = -1;
	bus->key_shm = -1;
	bus->message = NULL;

	/* TODO */

	if (flags >= 0) {
		t(open_semaphores(bus));
		t(open_shared_memory(bus, flags));
	}
	return 0;
fail:
	return -1;
}


int
bus_close(bus_t *bus)
{
	if (bus->address)
		t(close_shared_memory(bus));
	return 0;
fail:
	return -1;
}


int
bus_write(const bus_t *bus, const char *message)
{
	t(acquire_semaphore(bus, X, SEM_UNDO));
	t(zero_semaphore(bus, W));
	t(write_shared_memory(bus, message));
	t(write_semaphore(bus, Q, 0));
	t(zero_semaphore(bus, S));
	t(release_semaphore(bus, X, SEM_UNDO));
	return 0;
fail:
	return -1;
}


int
bus_read(const bus_t *bus, int (*callback)(const char *message, void *user_data), void *user_data)
{
	int r;
	t(release_semaphore(bus, S, SEM_UNDO));
	for (;;) {
		t(release_semaphore(bus, Q, 0));
		t(zero_semaphore(bus, Q));
		t(r = callback(bus->message, user_data));
		if (!r) {
			t(acquire_semaphore(bus, S, SEM_UNDO));
			return 0;
		}
		t(release_semaphore(bus, W, SEM_UNDO));
		t(acquire_semaphore(bus, S, SEM_UNDO));
		t(zero_semaphore(bus, S));
		t(release_semaphore(bus, S, SEM_UNDO));
		t(acquire_semaphore(bus, W, SEM_UNDO));
	}
fail:
	return -1;
}

