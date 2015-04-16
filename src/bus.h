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
#define BUS_RDWR    0

/**
 * Fail to create bus if its file already exists
 */
#define BUS_EXCL    2



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
} bus_t;



const char *bus_create(const char *file, int flags);
int bus_unlink(const char *file);

int bus_open(bus_t *bus, const char *file, int flags);
int bus_close(bus_t *bus);

int bus_write(const bus_t *bus, const char *message);
int bus_read(const bus_t *bus, int (*callback)(const char *message, void *user_data), void *user_data);



#endif

